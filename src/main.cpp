#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <XPT2046_Touchscreen.h>
#include <time.h>

#include "config.h"
#include "types.h"
#include "storage.h"
#include "train_api.h"
#include "display_ui.h"
#include "web_portal.h"

// Global state
AppConfig gConfig;
BoardData gBoard;
DisplayUI gDisplay;
WebPortal gWebPortal(gConfig, gBoard);

// Dedicated SPI bus for the XPT2046 resistive touch controller on the CYD
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen touch(XPT2046_CS, XPT2046_IRQ);

unsigned long gLastApiRefresh = 0;
unsigned long gLastTouchMillis = 0;
bool gNeedsImmediateRefresh = false;
bool gPendingWifiReconnect = false;

static void initRgbLedOff() {
    pinMode(CYD_LED_RED, OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE, OUTPUT);
    // Active LOW on ESP32-2432S028R
    digitalWrite(CYD_LED_RED, HIGH);
    digitalWrite(CYD_LED_GREEN, HIGH);
    digitalWrite(CYD_LED_BLUE, HIGH);
}

static void refreshBoardNow() {
    gDisplay.setFetchingIndicator(true);
    TrainApiClient::fetchDepartures(gConfig, gBoard);
    gDisplay.setFetchingIndicator(false);
    gLastApiRefresh = millis();

    if (gDisplay.getScreenMode() == ScreenMode::DEPARTURE_BOARD) {
        gDisplay.showDepartureBoard(gBoard, gConfig);
    }
}

static void onTouchCrsChanged(const String& newStationCrs, const String& newFilterCrs) {
    gConfig.stationCrs = ConfigStorage::sanitizeCrs(newStationCrs);
    gConfig.filterCrs = ConfigStorage::sanitizeCrs(newFilterCrs);
    ConfigStorage::saveStationCrs(gConfig.stationCrs, gConfig.filterCrs);

    // Pre-populate station title while loading
    const char* presetName = lookupStationName(gConfig.stationCrs);
    gBoard.crs = gConfig.stationCrs;
    gBoard.locationName = presetName ? String(presetName) : gConfig.stationCrs;
    gBoard.serviceCount = 0;
    gBoard.errorMessage = "Loading live departures...";
    gBoard.fetchSuccess = false;

    gDisplay.showDepartureBoard(gBoard, gConfig);
    gNeedsImmediateRefresh = true;
}

static void onManualRefreshRequested() {
    gNeedsImmediateRefresh = true;
}

static void onWebConfigSaved(bool wifiChanged, bool stationChanged) {
    gDisplay.setBrightness(gConfig.brightness);
    if (wifiChanged) {
        gPendingWifiReconnect = true;
    } else if (stationChanged) {
        gNeedsImmediateRefresh = true;
    }
}

static bool connectToWifi() {
    if (gConfig.wifiSsid.isEmpty()) {
        return false;
    }

    gDisplay.showSplash("Connecting to Wi-Fi: " + gConfig.wifiSsid + "...");
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(MDNS_HOSTNAME);
    WiFi.begin(gConfig.wifiSsid.c_str(), gConfig.wifiPass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 12000UL) {
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
        // Configure NTP with UK GMT/BST automatic DST rules
        configTzTime(UK_POSIX_TZ, NTP_SERVER_1, NTP_SERVER_2);
        return true;
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    initRgbLedOff();

    // Load saved station CRS, Wi-Fi credentials, and brightness from NVS
    ConfigStorage::load(gConfig);

    // Initialise TFT display
    gDisplay.begin(gConfig.brightness);
    gDisplay.showSplash("Initialising hardware...");

    // Initialise XPT2046 resistive touch on VSPI pins
    touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touch.begin(touchSpi);
    touch.setRotation(1); // Match TFT landscape rotation 1

    // Connect to Wi-Fi or start Captive Portal AP
    bool connected = connectToWifi();
    if (connected) {
        gWebPortal.begin(false, onWebConfigSaved);
        gDisplay.showSplash("Fetching " + gConfig.stationCrs + " departures...");
        TrainApiClient::fetchDepartures(gConfig, gBoard);
        gLastApiRefresh = millis();
        gDisplay.showDepartureBoard(gBoard, gConfig);
    } else {
        gWebPortal.begin(true, onWebConfigSaved);
        gDisplay.showApPortalInfo(AP_PORTAL_SSID, WiFi.softAPIP().toString());
    }
}

void loop() {
    gWebPortal.handleClient();

    if (gPendingWifiReconnect) {
        gPendingWifiReconnect = false;
        delay(300);
        ESP.restart();
    }

    // Poll XPT2046 touchscreen
    if (touch.tirqTouched() && touch.touched()) {
        unsigned long now = millis();
        if (now - gLastTouchMillis >= TOUCH_DEBOUNCE_MS) {
            TS_Point p = touch.getPoint();
            // Filter out light noise presses
            if (p.z > 200) {
                gLastTouchMillis = now;
                int16_t screenX = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH - 1);
                int16_t screenY = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT - 1);
                screenX = constrain(screenX, 0, SCREEN_WIDTH - 1);
                screenY = constrain(screenY, 0, SCREEN_HEIGHT - 1);

                gDisplay.handleTouch(
                    screenX,
                    screenY,
                    gBoard,
                    gConfig,
                    onTouchCrsChanged,
                    onManualRefreshRequested
                );
            }
        }
    }

    // Immediate or scheduled departure board refresh
    if (gNeedsImmediateRefresh) {
        gNeedsImmediateRefresh = false;
        refreshBoardNow();
    } else if (WiFi.status() == WL_CONNECTED &&
               gDisplay.getScreenMode() == ScreenMode::DEPARTURE_BOARD &&
               (millis() - gLastApiRefresh >= API_REFRESH_INTERVAL_MS)) {
        refreshBoardNow();
    }

    // Run non-blocking scrolling calling-points ticker & digital clock
    gDisplay.tick(gBoard, gConfig);
}
