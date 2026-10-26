#include "display_ui.h"
#include "config.h"
#include "storage.h"
#include <WiFi.h>
#include <time.h>

static const int16_t TICKER_Y = 63;
static const int16_t TICKER_W = 312;
static const int16_t TICKER_H = 19;

static const int16_t CLOCK_X = 104;
static const int16_t CLOCK_Y = 213;
static const int16_t CLOCK_W = 112;
static const int16_t CLOCK_H = 25;

static const char* KB_ROW1 = "QWERTYUIOP";
static const char* KB_ROW2 = "ASDFGHJKL";
static const char* KB_ROW3 = "ZXCVBNM";

DisplayUI::DisplayUI()
    : _tft(TFT_eSPI()),
      _tickerSprite(&_tft),
      _clockSprite(&_tft) {}

void DisplayUI::begin(uint8_t brightness) {
    _tft.init();
    _tft.setRotation(1); // Landscape 320x240 (USB port on right/bottom depending on mount)
    _tft.fillScreen(COL_BG);

    // Configure PWM backlight on GPIO 21
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(CYD_TFT_BL, CYD_BL_PWM_FREQ, CYD_BL_PWM_RES);
#else
    ledcSetup(CYD_BL_PWM_CHANNEL, CYD_BL_PWM_FREQ, CYD_BL_PWM_RES);
    ledcAttachPin(CYD_TFT_BL, CYD_BL_PWM_CHANNEL);
#endif
    setBrightness(brightness);

    // Allocate sprites for flicker-free scrolling ticker and digital clock
    _tickerSprite.setColorDepth(16);
    _tickerSprite.createSprite(TICKER_W, TICKER_H);

    _clockSprite.setColorDepth(16);
    _clockSprite.createSprite(CLOCK_W, CLOCK_H);
}

void DisplayUI::setBrightness(uint8_t brightness) {
    uint8_t val = brightness < 15 ? 15 : brightness;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(CYD_TFT_BL, val);
#else
    ledcWrite(CYD_BL_PWM_CHANNEL, val);
#endif
}

void DisplayUI::showSplash(const String& statusText) {
    _tft.fillScreen(COL_BG);

    // Outer double amber border
    _tft.drawRect(4, 4, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 8, COL_AMBER_DIM);
    _tft.drawRect(6, 6, SCREEN_WIDTH - 12, SCREEN_HEIGHT - 12, COL_AMBER);

    // British Rail style double-arrow inspired header graphic
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BG);
    _tft.drawString("UK RAIL DEPARTURES", SCREEN_WIDTH / 2, 56, 4);

    _tft.setTextColor(COL_AMBER, COL_BG);
    _tft.drawString("CHEAP YELLOW DISPLAY EDITION", SCREEN_WIDTH / 2, 86, 2);

    for (int x = 30; x < SCREEN_WIDTH - 30; x += 6) {
        _tft.drawPixel(x, 108, COL_AMBER_DIM);
    }

    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BG);
    _tft.drawString(statusText, SCREEN_WIDTH / 2, 145, 2);

    _tft.setTextColor(COL_AMBER_DIM, COL_BG);
    _tft.drawString("Tap [SET CRS] anytime to change station", SCREEN_WIDTH / 2, 205, 2);
}

void DisplayUI::showApPortalInfo(const String& apSsid, const String& apIp) {
    _mode = ScreenMode::WIFI_INFO;
    _tft.fillScreen(COL_BG);

    _tft.drawRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, COL_AMBER);
    _tft.fillRect(4, 4, SCREEN_WIDTH - 8, 26, COL_HEADER_BG);

    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_HEADER_BG);
    _tft.drawString("WI-FI & STATION SETUP", SCREEN_WIDTH / 2, 17, 2);

    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_AMBER, COL_BG);
    _tft.drawString("1. Connect phone/PC to Wi-Fi:", 14, 40, 2);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BG);
    _tft.drawString("   SSID: " + apSsid, 14, 60, 4);

    _tft.setTextColor(COL_AMBER, COL_BG);
    _tft.drawString("2. Open browser at:", 14, 92, 2);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BG);
    _tft.drawString("   http://" + apIp, 14, 112, 4);

    _tft.setTextColor(COL_AMBER_DIM, COL_BG);
    _tft.drawString("You can set Wi-Fi & Station CRS in the", 14, 146, 2);
    _tft.drawString("web portal, or set Station CRS right now:", 14, 164, 2);

    // Button to open on-screen CRS keyboard directly
    _tft.fillRoundRect(40, 192, 240, 36, 4, COL_BTN_BG);
    _tft.drawRoundRect(40, 192, 240, 36, 4, COL_AMBER_BRIGHT);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BTN_BG);
    _tft.drawString("SET STATION CRS ON SCREEN", SCREEN_WIDTH / 2, 210, 2);
}

void DisplayUI::rebuildActiveTicker(const BoardData& board) {
    if (!board.fetchSuccess && !board.errorMessage.isEmpty()) {
        _activeTickerText = "STATUS: " + board.errorMessage + "  --  Tap [SET CRS] to change station or check Wi-Fi";
    } else if (board.serviceCount == 0) {
        if (!board.nrccMessage.isEmpty()) {
            _activeTickerText = "NATIONAL RAIL ALERT: " + board.nrccMessage;
        } else {
            _activeTickerText = "No scheduled departures found for " + board.locationName + " (" + board.crs + ").";
        }
    } else {
        if (_selectedServiceIdx >= board.serviceCount) {
            _selectedServiceIdx = 0;
        }
        _activeTickerText = board.services[_selectedServiceIdx].tickerText;
        if (!board.nrccMessage.isEmpty()) {
            _activeTickerText += "   *** ALERT: " + board.nrccMessage + " ***";
        }
    }

    _tickerSprite.setTextFont(2);
    _tickerTextWidth = _tickerSprite.textWidth(_activeTickerText);
    _tickerScrollX = TICKER_W;
}

void DisplayUI::showDepartureBoard(const BoardData& board, const AppConfig& config) {
    _mode = ScreenMode::DEPARTURE_BOARD;
    _tft.fillScreen(COL_BG);

    if (_selectedServiceIdx >= board.serviceCount) {
        _selectedServiceIdx = 0;
    }

    drawBoardHeader(board, config);
    drawBoardRows(board);
    rebuildActiveTicker(board);
    updateTickerSprite();
    drawBottomBar(config);
    _lastClockStr = "";
    updateClockSprite();
}

void DisplayUI::drawBoardHeader(const BoardData& board, const AppConfig& config) {
    // Top header background
    _tft.fillRect(0, 0, SCREEN_WIDTH, 26, COL_HEADER_BG);
    _tft.drawFastHLine(0, 26, SCREEN_WIDTH, COL_AMBER_DIM);

    // Station Name + CRS Code on left
    String title = board.locationName;
    if (title.isEmpty()) {
        const char* preset = lookupStationName(config.stationCrs);
        title = preset ? String(preset) : config.stationCrs;
    }
    title.toUpperCase();

    // Truncate title if needed so it doesn't collide with [SET CRS] button at x=246
    String badge = " [" + config.stationCrs;
    if (config.filterCrs.length() == 3) {
        badge += ">" + config.filterCrs;
    }
    badge += "]";

    _tft.setTextFont(2);
    int16_t badgeW = _tft.textWidth(badge);
    int16_t maxTitleW = 240 - badgeW;
    while (title.length() > 3 && _tft.textWidth(title) > maxTitleW) {
        title.remove(title.length() - 1);
    }

    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_HEADER_BG);
    _tft.drawString(title, 5, 13, 2);

    int16_t titleW = _tft.textWidth(title);
    _tft.setTextColor(COL_AMBER, COL_HEADER_BG);
    _tft.drawString(badge, 5 + titleW, 13, 2);

    // [SET CRS] Button on top right (x=246..317, y=2..23)
    _tft.fillRoundRect(246, 2, 71, 21, 3, COL_BTN_BG);
    _tft.drawRoundRect(246, 2, 71, 21, 3, COL_AMBER_BRIGHT);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BTN_BG);
    _tft.drawString("SET CRS", 281, 12, 2);
}

static String truncateToWidth(TFT_eSPI& tft, const String& str, int16_t maxWidth, uint8_t font) {
    tft.setTextFont(font);
    if (tft.textWidth(str) <= maxWidth) {
        return str;
    }
    String out = str;
    while (out.length() > 2 && tft.textWidth(out + "..") > maxWidth) {
        out.remove(out.length() - 1);
    }
    return out + "..";
}

void DisplayUI::drawBoardRows(const BoardData& board) {
    // Clear board content area (y = 27..208)
    _tft.fillRect(0, 27, SCREEN_WIDTH, 182, COL_BG);

    // Column Header labels (y = 27..39)
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_AMBER_DIM, COL_BG);
    _tft.drawString("TIME", 4, 27, 1);
    _tft.drawString("DESTINATION", 46, 27, 1);
    _tft.drawString("PLAT", 214, 27, 1);
    _tft.drawString("EXPECTED", 252, 27, 1);
    _tft.drawFastHLine(0, 38, SCREEN_WIDTH, COL_BOARD_BG);

    if (board.serviceCount == 0) {
        _tft.setTextDatum(MC_DATUM);
        if (!board.fetchSuccess) {
            _tft.setTextColor(COL_RED_ALERT, COL_BG);
            _tft.drawString("UNABLE TO FETCH TIMETABLE", SCREEN_WIDTH / 2, 110, 2);
            _tft.setTextColor(COL_AMBER, COL_BG);
            _tft.drawString(board.errorMessage, SCREEN_WIDTH / 2, 132, 2);
        } else {
            _tft.setTextColor(COL_AMBER_BRIGHT, COL_BG);
            _tft.drawString("NO DEPARTURES SCHEDULED", SCREEN_WIDTH / 2, 115, 2);
            _tft.setTextColor(COL_AMBER_DIM, COL_BG);
            _tft.drawString("Check station code or filter settings", SCREEN_WIDTH / 2, 138, 2);
        }
        return;
    }

    // ---- Primary / Selected Row at Top (y = 40..61) ----
    if (_selectedServiceIdx >= board.serviceCount) {
        _selectedServiceIdx = 0;
    }
    const TrainService& primary = board.services[_selectedServiceIdx];

    _tft.fillRect(0, 40, SCREEN_WIDTH, 21, COL_BOARD_BG);
    _tft.setTextDatum(ML_DATUM);

    // Scheduled Time
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BOARD_BG);
    _tft.drawString(primary.std, 4, 50, 2);

    // Destination (+ via if space allows)
    String primaryDest = primary.destinationName;
    if (!primary.via.isEmpty()) {
        primaryDest += " (" + primary.via + ")";
    }
    primaryDest = truncateToWidth(_tft, primaryDest, 164, 2);
    _tft.drawString(primaryDest, 46, 50, 2);

    // Platform
    String plat = primary.platform.isEmpty() ? "-" : primary.platform;
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BOARD_BG);
    _tft.drawString(plat, 226, 50, 2);

    // Expected / Status
    _tft.setTextDatum(MR_DATUM);
    bool isCanc = primary.isCancelled || primary.etd.equalsIgnoreCase("Cancelled");
    uint16_t statusCol = isCanc ? COL_RED_ALERT
                                : (primary.etd.equalsIgnoreCase("On time") ? COL_AMBER : COL_AMBER_BRIGHT);
    String etdDisplay = isCanc ? "CANCELLED" : primary.etd;
    _tft.setTextColor(statusCol, COL_BOARD_BG);
    _tft.drawString(etdDisplay, 316, 50, 2);

    // Dotted horizontal line below calling-points ticker (y = 85)
    for (int16_t x = 4; x < SCREEN_WIDTH - 4; x += 4) {
        _tft.drawPixel(x, 85, COL_AMBER_DIM);
    }

    // ---- Subsequent Departures Rows (y = 88..207, up to 4 rows) ----
    // Show all services (1..N) in the list below, or services other than primary;
    // showing rows 2..5 (or highlighting whichever row is selected) makes tapping any row intuitive!
    int16_t rowY = 88;
    const int16_t rowH = 29;

    uint8_t rowsDrawn = 0;
    for (uint8_t i = 0; i < board.serviceCount && rowsDrawn < 4; ++i) {
        // If we have >1 service and _selectedServiceIdx == 0, show indices 1..4 below.
        // If user tapped a lower row, keep indices 1..4 in consistent positions and highlight selected!
        if (i == 0 && _selectedServiceIdx == 0 && board.serviceCount > 1) {
            continue;
        }
        if (i == 0 && _selectedServiceIdx != 0) {
            // If user selected row >0, show index 0 in the first slot so they can tap back to 1st train
        } else if (i > 0 && rowsDrawn >= 4) {
            break;
        }

        const TrainService& svc = board.services[i];
        bool isSelected = (i == _selectedServiceIdx);
        uint16_t rowBg = isSelected ? COL_HEADER_BG : COL_BG;

        if (isSelected) {
            _tft.fillRect(0, rowY, SCREEN_WIDTH, rowH - 2, rowBg);
        }

        // Ordinal + Time
        _tft.setTextDatum(ML_DATUM);
        _tft.setTextColor(isSelected ? COL_AMBER_BRIGHT : COL_AMBER, rowBg);
        _tft.drawString(svc.std, 4, rowY + 13, 2);

        // Destination
        String dest = truncateToWidth(_tft, svc.destinationName, 164, 2);
        _tft.drawString(dest, 46, rowY + 13, 2);

        // Platform
        String p = svc.platform.isEmpty() ? "-" : svc.platform;
        _tft.setTextDatum(MC_DATUM);
        _tft.drawString(p, 226, rowY + 13, 2);

        // Expected
        _tft.setTextDatum(MR_DATUM);
        bool svcCanc = svc.isCancelled || svc.etd.equalsIgnoreCase("Cancelled");
        uint16_t sCol = svcCanc ? COL_RED_ALERT
                                : (svc.etd.equalsIgnoreCase("On time") ? COL_AMBER : COL_AMBER_BRIGHT);
        _tft.setTextColor(sCol, rowBg);
        _tft.drawString(svcCanc ? "CANCELLED" : svc.etd, 316, rowY + 13, 2);

        // Subtle separator line
        _tft.drawFastHLine(4, rowY + rowH - 1, SCREEN_WIDTH - 8, COL_BOARD_BG);

        rowY += rowH;
        rowsDrawn++;
    }
}

void DisplayUI::drawBottomBar(const AppConfig& config) {
    _tft.fillRect(0, 210, SCREEN_WIDTH, 30, COL_HEADER_BG);
    _tft.drawFastHLine(0, 210, SCREEN_WIDTH, COL_AMBER_DIM);

    // Left: Wi-Fi / Web UI status button (x = 4..98)
    bool wifiOk = (WiFi.status() == WL_CONNECTED);
    _tft.fillCircle(11, 225, 4, wifiOk ? COL_GREEN_OK : COL_RED_ALERT);
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(COL_AMBER, COL_HEADER_BG);
    if (wifiOk) {
        _tft.drawString(WiFi.localIP().toString(), 20, 225, 1);
    } else {
        _tft.drawString("SETUP WIFI", 20, 225, 1);
    }

    // Right: [REFRESH] button (x = 236..316)
    _tft.fillRoundRect(238, 214, 78, 22, 3, COL_BTN_BG);
    _tft.drawRoundRect(238, 214, 78, 22, 3, COL_AMBER_DIM);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_AMBER, COL_BTN_BG);
    _tft.drawString("REFRESH", 277, 225, 2);
}

void DisplayUI::setFetchingIndicator(bool fetching) {
    if (_mode != ScreenMode::DEPARTURE_BOARD) {
        return;
    }
    _tft.fillRoundRect(238, 214, 78, 22, 3, fetching ? COL_BTN_ACTIVE : COL_BTN_BG);
    _tft.drawRoundRect(238, 214, 78, 22, 3, fetching ? COL_AMBER_BRIGHT : COL_AMBER_DIM);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(fetching ? COL_AMBER_BRIGHT : COL_AMBER, fetching ? COL_BTN_ACTIVE : COL_BTN_BG);
    _tft.drawString(fetching ? "SYNCING.." : "REFRESH", 277, 225, 2);
}

void DisplayUI::updateTickerSprite() {
    if (_mode != ScreenMode::DEPARTURE_BOARD) {
        return;
    }

    _tickerSprite.fillSprite(COL_BG);
    _tickerSprite.setTextDatum(ML_DATUM);
    _tickerSprite.setTextColor(COL_AMBER, COL_BG);
    _tickerSprite.drawString(_activeTickerText, _tickerScrollX, TICKER_H / 2, 2);
    _tickerSprite.pushSprite(4, TICKER_Y);

    _tickerScrollX -= TICKER_SCROLL_STEP_PX;
    if (_tickerScrollX < -_tickerTextWidth - 20) {
        _tickerScrollX = TICKER_W;
    }
}

void DisplayUI::updateClockSprite() {
    if (_mode != ScreenMode::DEPARTURE_BOARD) {
        return;
    }

    struct tm timeinfo;
    char buf[16];
    if (getLocalTime(&timeinfo, 5)) {
        strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    } else {
        unsigned long sec = millis() / 1000UL;
        snprintf(buf, sizeof(buf), "--:--:%02lu", sec % 60);
    }

    String currentStr(buf);
    if (currentStr == _lastClockStr) {
        return;
    }
    _lastClockStr = currentStr;

    _clockSprite.fillSprite(COL_HEADER_BG);
    _clockSprite.drawRect(0, 0, CLOCK_W, CLOCK_H, COL_AMBER_DIM);
    _clockSprite.setTextDatum(MC_DATUM);
    _clockSprite.setTextColor(COL_AMBER_BRIGHT, COL_HEADER_BG);
    _clockSprite.drawString(currentStr, CLOCK_W / 2, CLOCK_H / 2 + 1, 4);
    _clockSprite.pushSprite(CLOCK_X, CLOCK_Y);
}

void DisplayUI::tick(const BoardData& board, const AppConfig& config) {
    if (_mode != ScreenMode::DEPARTURE_BOARD) {
        return;
    }

    unsigned long now = millis();
    if (now - _lastTickerTick >= TICKER_SCROLL_INTERVAL_MS) {
        _lastTickerTick = now;
        updateTickerSprite();
    }

    if (now - _lastClockTick >= CLOCK_UPDATE_INTERVAL_MS) {
        _lastClockTick = now;
        updateClockSprite();
    }
}

// ============================================================================
// On-Screen Touch Keyboard for Entering 3-Letter UK Station CRS Code
// ============================================================================

void DisplayUI::openCrsKeyboard(const AppConfig& config) {
    _mode = ScreenMode::CRS_KEYBOARD;
    _editingFilterCrs = false;
    _kbStationInput = config.stationCrs;
    _kbFilterInput = config.filterCrs;
    drawKeyboardScreen();
}

void DisplayUI::drawKeyboardScreen() {
    _tft.fillScreen(COL_BG);

    // Top mode tabs: [STATION CRS] (x=4..136) | [CALLING AT FILTER] (x=140..268) | [ESC] (x=272..316)
    _tft.fillRect(0, 0, SCREEN_WIDTH, 26, COL_HEADER_BG);

    uint16_t stBg = !_editingFilterCrs ? COL_BTN_ACTIVE : COL_BTN_BG;
    uint16_t stFg = !_editingFilterCrs ? COL_AMBER_BRIGHT : COL_AMBER_DIM;
    _tft.fillRoundRect(4, 2, 130, 22, 3, stBg);
    _tft.drawRoundRect(4, 2, 130, 22, 3, stFg);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(stFg, stBg);
    _tft.drawString("FROM STATION", 69, 13, 2);

    uint16_t flBg = _editingFilterCrs ? COL_BTN_ACTIVE : COL_BTN_BG;
    uint16_t flFg = _editingFilterCrs ? COL_AMBER_BRIGHT : COL_AMBER_DIM;
    _tft.fillRoundRect(138, 2, 130, 22, 3, flBg);
    _tft.drawRoundRect(138, 2, 130, 22, 3, flFg);
    _tft.setTextColor(flFg, flBg);
    _tft.drawString("FILTER (TO)", 203, 13, 2);

    // [ESC] button
    _tft.fillRoundRect(272, 2, 44, 22, 3, COL_BTN_BG);
    _tft.drawRoundRect(272, 2, 44, 22, 3, COL_RED_ALERT);
    _tft.setTextColor(COL_RED_ALERT, COL_BTN_BG);
    _tft.drawString("ESC", 294, 13, 2);

    drawKeyboardInputBox();
    drawKeyboardPresets();
    drawKeyboardKeys();
}

void DisplayUI::drawKeyboardInputBox() {
    // Clear input preview zone (y = 28..67)
    _tft.fillRect(0, 28, SCREEN_WIDTH, 40, COL_BG);

    const String& currentInput = _editingFilterCrs ? _kbFilterInput : _kbStationInput;

    // Draw 3 character slots at x = 8, 42, 76 (each 30x34)
    for (int i = 0; i < 3; ++i) {
        int16_t bx = 8 + i * 33;
        int16_t by = 31;
        bool isCursor = (i == (int)currentInput.length());
        _tft.fillRoundRect(bx, by, 30, 34, 3, COL_BOARD_BG);
        _tft.drawRoundRect(bx, by, 30, 34, 3, isCursor ? COL_AMBER_BRIGHT : COL_AMBER);

        if (i < (int)currentInput.length()) {
            char c[2] = {currentInput.charAt(i), '\0'};
            _tft.setTextDatum(MC_DATUM);
            _tft.setTextColor(COL_AMBER_BRIGHT, COL_BOARD_BG);
            _tft.drawString(c, bx + 15, by + 18, 4);
        } else if (isCursor) {
            _tft.drawFastHLine(bx + 7, by + 28, 16, COL_AMBER_BRIGHT);
        }
    }

    // Station Name Lookup Preview on right (x = 114..316)
    _tft.setTextDatum(TL_DATUM);
    if (currentInput.length() == 3) {
        const char* matchedName = lookupStationName(currentInput);
        _tft.setTextColor(COL_AMBER_DIM, COL_BG);
        _tft.drawString(_editingFilterCrs ? "Calling at station:" : "Selected UK Station:", 114, 32, 1);

        _tft.setTextColor(COL_AMBER_BRIGHT, COL_BG);
        if (matchedName) {
            _tft.drawString(truncateToWidth(_tft, String(matchedName), 200, 2), 114, 45, 2);
        } else {
            _tft.drawString("CRS: " + currentInput + " (Custom)", 114, 45, 2);
        }
    } else if (currentInput.isEmpty() && _editingFilterCrs) {
        _tft.setTextColor(COL_AMBER_DIM, COL_BG);
        _tft.drawString("Optional Filter:", 114, 32, 1);
        _tft.setTextColor(COL_AMBER, COL_BG);
        _tft.drawString("All Destinations", 114, 45, 2);
    } else {
        _tft.setTextColor(COL_AMBER_DIM, COL_BG);
        _tft.drawString("Enter 3-letter CRS", 114, 32, 1);
        _tft.setTextColor(COL_AMBER, COL_BG);
        _tft.drawString("or tap preset below", 114, 45, 2);
    }
}

void DisplayUI::drawKeyboardPresets() {
    // Quick-select preset bar at y = 69..94
    _tft.fillRect(0, 69, SCREEN_WIDTH, 26, COL_BOARD_BG);
    _tft.drawFastHLine(0, 69, SCREEN_WIDTH, COL_AMBER_DIM);
    _tft.drawFastHLine(0, 94, SCREEN_WIDTH, COL_AMBER_DIM);

    // [<] button (x = 3..29), 6 station chips (x = 33..287), [>] button (x = 291..317)
    _tft.fillRoundRect(3, 72, 26, 20, 2, COL_BTN_BG);
    _tft.drawRoundRect(3, 72, 26, 20, 2, COL_AMBER_DIM);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BTN_BG);
    _tft.drawString("<", 16, 82, 2);

    const uint8_t perPage = 6;
    uint8_t totalPages = (UK_STATION_PRESETS_COUNT + perPage - 1) / perPage;
    if (_presetPage >= totalPages) {
        _presetPage = 0;
    }

    for (uint8_t i = 0; i < perPage; ++i) {
        size_t idx = _presetPage * perPage + i;
        int16_t cx = 32 + i * 43;
        if (idx < UK_STATION_PRESETS_COUNT) {
            const char* crs = UK_STATION_PRESETS[idx].crs;
            const String& activeCrs = _editingFilterCrs ? _kbFilterInput : _kbStationInput;
            bool match = activeCrs.equalsIgnoreCase(crs);

            uint16_t bg = match ? COL_BTN_ACTIVE : COL_BTN_BG;
            uint16_t fg = match ? COL_AMBER_BRIGHT : COL_AMBER;
            _tft.fillRoundRect(cx, 72, 40, 20, 2, bg);
            _tft.drawRoundRect(cx, 72, 40, 20, 2, fg);
            _tft.setTextColor(fg, bg);
            _tft.drawString(crs, cx + 20, 82, 2);
        }
    }

    _tft.fillRoundRect(291, 72, 26, 20, 2, COL_BTN_BG);
    _tft.drawRoundRect(291, 72, 26, 20, 2, COL_AMBER_DIM);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BTN_BG);
    _tft.drawString(">", 304, 82, 2);
}

void DisplayUI::drawKeyboardKeys() {
    _tft.fillRect(0, 96, SCREEN_WIDTH, 144, COL_BG);
    _tft.setTextDatum(MC_DATUM);

    // Row 1: Q W E R T Y U I O P (y = 98..130, 10 keys, width 30, spacing 31)
    for (int i = 0; i < 10; ++i) {
        int16_t kx = 5 + i * 31;
        int16_t ky = 98;
        char label[2] = {KB_ROW1[i], '\0'};
        _tft.fillRoundRect(kx, ky, 29, 31, 3, COL_BTN_BG);
        _tft.drawRoundRect(kx, ky, 29, 31, 3, COL_AMBER_DIM);
        _tft.setTextColor(COL_AMBER_BRIGHT, COL_BTN_BG);
        _tft.drawString(label, kx + 14, ky + 15, 2);
    }

    // Row 2: A S D F G H J K L + [CLR] (y = 132..164)
    for (int i = 0; i < 9; ++i) {
        int16_t kx = 5 + i * 30;
        int16_t ky = 132;
        char label[2] = {KB_ROW2[i], '\0'};
        _tft.fillRoundRect(kx, ky, 28, 31, 3, COL_BTN_BG);
        _tft.drawRoundRect(kx, ky, 28, 31, 3, COL_AMBER_DIM);
        _tft.setTextColor(COL_AMBER_BRIGHT, COL_BTN_BG);
        _tft.drawString(label, kx + 14, ky + 15, 2);
    }
    // [CLR] key on Row 2 right (x = 275..315)
    _tft.fillRoundRect(275, 132, 40, 31, 3, COL_BTN_BG);
    _tft.drawRoundRect(275, 132, 40, 31, 3, COL_AMBER);
    _tft.setTextColor(COL_AMBER, COL_BTN_BG);
    _tft.drawString("CLR", 295, 147, 2);

    // Row 3: Z X C V B N M + [< DEL] (y = 166..198)
    for (int i = 0; i < 7; ++i) {
        int16_t kx = 5 + i * 32;
        int16_t ky = 166;
        char label[2] = {KB_ROW3[i], '\0'};
        _tft.fillRoundRect(kx, ky, 30, 31, 3, COL_BTN_BG);
        _tft.drawRoundRect(kx, ky, 30, 31, 3, COL_AMBER_DIM);
        _tft.setTextColor(COL_AMBER_BRIGHT, COL_BTN_BG);
        _tft.drawString(label, kx + 15, ky + 15, 2);
    }
    // [< DEL] key on Row 3 right (x = 231..315)
    _tft.fillRoundRect(231, 166, 84, 31, 3, COL_BTN_BG);
    _tft.drawRoundRect(231, 166, 84, 31, 3, COL_AMBER);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BTN_BG);
    _tft.drawString("< BKSP", 273, 181, 2);

    // Row 4 Action Bar (y = 202..236):
    // [CANCEL] (x=5..100) | [SAVE & LOAD BOARD] (x=106..315)
    _tft.fillRoundRect(5, 202, 95, 34, 4, COL_BTN_BG);
    _tft.drawRoundRect(5, 202, 95, 34, 4, COL_AMBER_DIM);
    _tft.setTextColor(COL_AMBER, COL_BTN_BG);
    _tft.drawString("CANCEL", 52, 219, 2);

    bool readyToSave = (_kbStationInput.length() == 3) &&
                       (_kbFilterInput.isEmpty() || _kbFilterInput.length() == 3);
    uint16_t saveBg = readyToSave ? COL_BTN_ACTIVE : COL_BTN_BG;
    uint16_t saveFg = readyToSave ? COL_AMBER_BRIGHT : COL_AMBER_DIM;
    _tft.fillRoundRect(106, 202, 209, 34, 4, saveBg);
    _tft.drawRoundRect(106, 202, 209, 34, 4, saveFg);
    _tft.setTextColor(saveFg, saveBg);

    String saveLabel = "LOAD " + _kbStationInput;
    if (_kbFilterInput.length() == 3) {
        saveLabel += " > " + _kbFilterInput;
    }
    _tft.drawString(readyToSave ? saveLabel : "ENTER 3-LETTER CRS", 210, 219, 2);
}

void DisplayUI::drawWifiInfoScreen(const AppConfig& config) {
    _mode = ScreenMode::WIFI_INFO;
    _tft.fillScreen(COL_BG);

    _tft.fillRect(0, 0, SCREEN_WIDTH, 26, COL_HEADER_BG);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_HEADER_BG);
    _tft.drawString("DEVICE & WEB PORTAL INFO", SCREEN_WIDTH / 2, 13, 2);

    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_AMBER, COL_BG);

    bool connected = (WiFi.status() == WL_CONNECTED);
    _tft.drawString("Wi-Fi Status:", 14, 38, 2);
    _tft.setTextColor(connected ? COL_AMBER_BRIGHT : COL_RED_ALERT, COL_BG);
    _tft.drawString(connected ? ("Connected (" + WiFi.SSID() + ")") : "AP Setup Mode Active", 120, 38, 2);

    _tft.setTextColor(COL_AMBER, COL_BG);
    _tft.drawString("Web Config URL:", 14, 66, 2);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BG);
    if (connected) {
        _tft.drawString("http://" + WiFi.localIP().toString() + "/", 14, 86, 4);
    } else {
        _tft.drawString("http://" + WiFi.softAPIP().toString() + "/", 14, 86, 4);
    }

    _tft.setTextColor(COL_AMBER, COL_BG);
    _tft.drawString("Station CRS:", 14, 122, 2);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BG);
    String stDesc = config.stationCrs;
    const char* name = lookupStationName(config.stationCrs);
    if (name) {
        stDesc += String(" (") + name + ")";
    }
    _tft.drawString(stDesc, 115, 122, 2);

    _tft.setTextColor(COL_AMBER_DIM, COL_BG);
    _tft.drawString("Open the Web URL in any phone/PC browser", 14, 150, 2);
    _tft.drawString("to configure Wi-Fi, CRS, or API URL.", 14, 168, 2);

    // Bottom buttons: [BACK TO BOARD] (x=14..154) | [SET CRS] (x=166..306)
    _tft.setTextDatum(MC_DATUM);
    _tft.fillRoundRect(14, 196, 140, 34, 4, COL_BTN_BG);
    _tft.drawRoundRect(14, 196, 140, 34, 4, COL_AMBER);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BTN_BG);
    _tft.drawString("BACK TO BOARD", 84, 213, 2);

    _tft.fillRoundRect(166, 196, 140, 34, 4, COL_BTN_ACTIVE);
    _tft.drawRoundRect(166, 196, 140, 34, 4, COL_AMBER_BRIGHT);
    _tft.setTextColor(COL_AMBER_BRIGHT, COL_BTN_ACTIVE);
    _tft.drawString("SET CRS CODE", 236, 213, 2);
}

void DisplayUI::handleTouch(int16_t x, int16_t y, BoardData& board, AppConfig& config,
                            OnCrsChangedCallback onCrsChanged,
                            OnManualRefreshCallback onRefresh) {
    if (_mode == ScreenMode::CRS_KEYBOARD) {
        handleKeyboardTouch(x, y, board, config, onCrsChanged);
        return;
    }

    if (_mode == ScreenMode::WIFI_INFO) {
        if (y >= 185) {
            if (x >= 160) {
                openCrsKeyboard(config);
            } else {
                showDepartureBoard(board, config);
            }
        } else {
            showDepartureBoard(board, config);
        }
        return;
    }

    // ScreenMode::DEPARTURE_BOARD touch zones:
    // 1. Header bar (y <= 28): open On-Screen Station CRS Keyboard
    if (y <= 28) {
        openCrsKeyboard(config);
        return;
    }

    // 2. Bottom bar (y >= 208):
    if (y >= 208) {
        if (x <= 104) {
            // Left side: Wi-Fi / Web UI info modal
            drawWifiInfoScreen(config);
            return;
        }
        if (x >= 230 && onRefresh) {
            // Right side: [REFRESH] button
            onRefresh();
            return;
        }
        return;
    }

    // 3. Tapping primary row (y = 40..62) resets selection to 1st train (index 0)
    if (y >= 40 && y <= 62 && board.serviceCount > 0) {
        _selectedServiceIdx = 0;
        drawBoardRows(board);
        rebuildActiveTicker(board);
        updateTickerSprite();
        return;
    }

    // 4. Tapping any of the 4 subsequent service rows (y = 88..204) selects that train
    //    so its calling points scroll across the ticker!
    if (y >= 88 && y <= 204 && board.serviceCount > 1) {
        uint8_t slot = (y - 88) / 29;
        uint8_t rowsDrawn = 0;
        for (uint8_t i = 0; i < board.serviceCount && rowsDrawn < 4; ++i) {
            if (i == 0 && _selectedServiceIdx == 0 && board.serviceCount > 1) {
                continue;
            }
            if (rowsDrawn == slot) {
                _selectedServiceIdx = i;
                drawBoardRows(board);
                rebuildActiveTicker(board);
                updateTickerSprite();
                break;
            }
            rowsDrawn++;
        }
    }
}

void DisplayUI::handleKeyboardTouch(int16_t x, int16_t y, BoardData& board, AppConfig& config,
                                    OnCrsChangedCallback onCrsChanged) {
    String& activeInput = _editingFilterCrs ? _kbFilterInput : _kbStationInput;

    // 1. Top Bar (y = 0..27): Mode tabs or [ESC]
    if (y <= 27) {
        if (x >= 270) {
            showDepartureBoard(board, config);
            return;
        }
        if (x < 136 && _editingFilterCrs) {
            _editingFilterCrs = false;
            drawKeyboardScreen();
            return;
        }
        if (x >= 136 && x < 270 && !_editingFilterCrs) {
            _editingFilterCrs = true;
            drawKeyboardScreen();
            return;
        }
        return;
    }

    // 2. Preset Carousel Bar (y = 68..95)
    if (y >= 68 && y <= 95) {
        const uint8_t perPage = 6;
        uint8_t totalPages = (UK_STATION_PRESETS_COUNT + perPage - 1) / perPage;
        if (x < 32) {
            _presetPage = (_presetPage == 0) ? (totalPages - 1) : (_presetPage - 1);
            drawKeyboardPresets();
            return;
        }
        if (x >= 288) {
            _presetPage = (_presetPage + 1) % totalPages;
            drawKeyboardPresets();
            return;
        }
        uint8_t chipIdx = (x - 32) / 43;
        if (chipIdx < perPage) {
            size_t presetIdx = _presetPage * perPage + chipIdx;
            if (presetIdx < UK_STATION_PRESETS_COUNT) {
                activeInput = UK_STATION_PRESETS[presetIdx].crs;
                drawKeyboardInputBox();
                drawKeyboardPresets();
                drawKeyboardKeys();
            }
        }
        return;
    }

    // 3. QWERTY Row 1 (y = 97..130): Q W E R T Y U I O P
    if (y >= 97 && y <= 130) {
        int idx = (x - 5) / 31;
        if (idx >= 0 && idx < 10 && activeInput.length() < 3) {
            activeInput += KB_ROW1[idx];
            drawKeyboardInputBox();
            drawKeyboardPresets();
            drawKeyboardKeys();
        }
        return;
    }

    // 4. QWERTY Row 2 (y = 131..164): A S D F G H J K L + [CLR]
    if (y >= 131 && y <= 164) {
        if (x >= 273) {
            // [CLR]
            activeInput = "";
            drawKeyboardInputBox();
            drawKeyboardPresets();
            drawKeyboardKeys();
            return;
        }
        int idx = (x - 5) / 30;
        if (idx >= 0 && idx < 9 && activeInput.length() < 3) {
            activeInput += KB_ROW2[idx];
            drawKeyboardInputBox();
            drawKeyboardPresets();
            drawKeyboardKeys();
        }
        return;
    }

    // 5. QWERTY Row 3 (y = 165..199): Z X C V B N M + [< BKSP]
    if (y >= 165 && y <= 199) {
        if (x >= 229) {
            // [< BKSP]
            if (activeInput.length() > 0) {
                activeInput.remove(activeInput.length() - 1);
                drawKeyboardInputBox();
                drawKeyboardPresets();
                drawKeyboardKeys();
            }
            return;
        }
        int idx = (x - 5) / 32;
        if (idx >= 0 && idx < 7 && activeInput.length() < 3) {
            activeInput += KB_ROW3[idx];
            drawKeyboardInputBox();
            drawKeyboardPresets();
            drawKeyboardKeys();
        }
        return;
    }

    // 6. Bottom Action Row (y >= 200): [CANCEL] | [SAVE & LOAD]
    if (y >= 200) {
        if (x <= 102) {
            // [CANCEL]
            showDepartureBoard(board, config);
            return;
        }
        if (x >= 106) {
            // [SAVE & LOAD]
            if (_kbStationInput.length() == 3 &&
                (_kbFilterInput.isEmpty() || _kbFilterInput.length() == 3)) {
                _selectedServiceIdx = 0;
                if (onCrsChanged) {
                    onCrsChanged(_kbStationInput, _kbFilterInput);
                }
            }
        }
    }
}
