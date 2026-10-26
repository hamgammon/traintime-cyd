#pragma once

#include <Arduino.h>

// ============================================================================
// Hardware Pin Definitions: DIYmalls ESP32-2432S028R (2.8" 320x240 CYD)
// ============================================================================

// Display Backlight (TFT pins are configured via platformio.ini build_flags)
#define CYD_TFT_BL 21
#define CYD_BL_PWM_CHANNEL 0
#define CYD_BL_PWM_FREQ 5000
#define CYD_BL_PWM_RES 8

// XPT2046 Resistive Touch Controller (on separate SPI pins)
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// Raw touch calibration values for ESP32-2432S028R in landscape (Rotation 1)
#define TOUCH_MIN_X 200
#define TOUCH_MAX_X 3750
#define TOUCH_MIN_Y 240
#define TOUCH_MAX_Y 3800

// Onboard RGB LED (Active LOW)
#define CYD_LED_RED 4
#define CYD_LED_GREEN 16
#define CYD_LED_BLUE 17

// Screen Dimensions (Landscape Rotation 1)
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// ============================================================================
// Classic UK Railway Dot-Matrix Colour Palette (RGB565)
// ============================================================================
#define COL_BG 0x0000          // Pure black background
#define COL_BOARD_BG 0x0841    // Very dark charcoal for row panels
#define COL_HEADER_BG 0x10A2   // Dark slate header background
#define COL_AMBER 0xFD20       // Classic UK railway LED amber (#FFA500)
#define COL_AMBER_BRIGHT 0xFE60 // Bright yellow-amber for highlighted 1st train & clock
#define COL_AMBER_DIM 0x7A80   // Dimmed amber for labels, borders, and inactive hints
#define COL_RED_ALERT 0xF986   // Bright red/orange-red for CANCELLED status
#define COL_GREEN_OK 0x2688    // Subtle green for Wi-Fi connected indicator
#define COL_BTN_BG 0x2124      // Dark button fill
#define COL_BTN_ACTIVE 0x4228  // Pressed/active button fill
#define COL_WHITE 0xFFFF

// ============================================================================
// Application Defaults & Timings
// ============================================================================
#define DEFAULT_STATION_CRS "PAD"
#define DEFAULT_HUXLEY_URL "https://huxley2.azurewebsites.net"
#define DEFAULT_BRIGHTNESS 220

#define API_REFRESH_INTERVAL_MS 30000UL   // Refresh departures every 30 seconds
#define CLOCK_UPDATE_INTERVAL_MS 250UL    // Update clock check 4x per second
#define TICKER_SCROLL_INTERVAL_MS 25UL    // Smooth ~40fps scrolling calling points
#define TICKER_SCROLL_STEP_PX 2           // Pixels per scroll tick
#define TOUCH_DEBOUNCE_MS 180UL           // Debounce between touch taps

#define AP_PORTAL_SSID "TrainTime-CYD"
#define MDNS_HOSTNAME "traintime"

// UK Timezone (GMT / BST automatic transitions)
#define UK_POSIX_TZ "GMT0BST,M3.5.0/1,M10.5.0"
#define NTP_SERVER_1 "uk.pool.ntp.org"
#define NTP_SERVER_2 "pool.ntp.org"

// Maximum services stored/displayed
#define MAX_SERVICES 5
#define MAX_CALLING_POINTS 24

// ============================================================================
// Built-in UK Station Lookup Table (for instant on-screen CRS preview)
// ============================================================================
struct StationPreset {
    const char* crs;
    const char* name;
};

static const StationPreset UK_STATION_PRESETS[] = {
    {"PAD", "London Paddington"},
    {"KGX", "London Kings Cross"},
    {"EUS", "London Euston"},
    {"WAT", "London Waterloo"},
    {"VIC", "London Victoria"},
    {"LST", "London Liverpool St"},
    {"LBG", "London Bridge"},
    {"STP", "London St Pancras"},
    {"CHX", "London Charing Cross"},
    {"MYB", "London Marylebone"},
    {"FST", "London Fenchurch St"},
    {"CTK", "City Thameslink"},
    {"CLJ", "Clapham Junction"},
    {"MAN", "Manchester Piccadilly"},
    {"MCV", "Manchester Victoria"},
    {"BHM", "Birmingham New Street"},
    {"BMO", "Birmingham Moor Street"},
    {"EDB", "Edinburgh Waverley"},
    {"HYM", "Haymarket"},
    {"GLC", "Glasgow Central"},
    {"GLQ", "Glasgow Queen Street"},
    {"LDS", "Leeds"},
    {"LIV", "Liverpool Lime Street"},
    {"BRI", "Bristol Temple Meads"},
    {"BPW", "Bristol Parkway"},
    {"RDG", "Reading"},
    {"OXF", "Oxford"},
    {"CBG", "Cambridge"},
    {"YRK", "York"},
    {"NCL", "Newcastle"},
    {"SHF", "Sheffield"},
    {"NOT", "Nottingham"},
    {"DBY", "Derby"},
    {"LEI", "Leicester"},
    {"CDF", "Cardiff Central"},
    {"SWA", "Swansea"},
    {"NWP", "Newport (South Wales)"},
    {"EXD", "Exeter St Davids"},
    {"PLY", "Plymouth"},
    {"PNZ", "Penzance"},
    {"SOU", "Southampton Central"},
    {"PMS", "Portsmouth Harbour"},
    {"BTN", "Brighton"},
    {"GTW", "Gatwick Airport"},
    {"SSD", "Stansted Airport"},
    {"LTN", "Luton Airport Pkwy"},
    {"MIA", "Manchester Airport"},
    {"HXX", "Heathrow Term 2 & 3"},
    {"ABD", "Aberdeen"},
    {"INV", "Inverness"},
    {"PTH", "Perth"},
    {"DEE", "Dundee"},
    {"STG", "Stirling"},
    {"KDY", "Kirkcaldy"},
    {"CAR", "Carlisle"},
    {"PRE", "Preston"},
    {"CRE", "Crewe"},
    {"CTR", "Chester"},
    {"COV", "Coventry"},
    {"MKC", "Milton Keynes Central"},
    {"WFJ", "Watford Junction"},
    {"SAC", "St Albans City"},
    {"DID", "Didcot Parkway"},
    {"SWI", "Swindon"},
    {"BTH", "Bath Spa"},
    {"NRW", "Norwich"},
    {"IPS", "Ipswich"},
    {"COL", "Colchester"},
    {"CHM", "Chelmsford"},
    {"SRA", "Stratford (London)"},
    {"bks", ""} // Sentinel not needed, use array size
};

static const size_t UK_STATION_PRESETS_COUNT =
    sizeof(UK_STATION_PRESETS) / sizeof(UK_STATION_PRESETS[0]) - 1;

inline const char* lookupStationName(const String& crs) {
    for (size_t i = 0; i < UK_STATION_PRESETS_COUNT; ++i) {
        if (crs.equalsIgnoreCase(UK_STATION_PRESETS[i].crs)) {
            return UK_STATION_PRESETS[i].name;
        }
    }
    return nullptr;
}
