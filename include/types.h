#pragma once

#include <Arduino.h>
#include "config.h"

struct CallingPoint {
    String locationName;
    String crs;
    String st; // Scheduled time (e.g., "14:25")
    String et; // Estimated time (e.g., "On time", "14:28", "Cancelled")
};

struct TrainService {
    String std;             // Scheduled Time of Departure (e.g., "14:15")
    String etd;             // Estimated Time of Departure (e.g., "On time", "14:20", "Cancelled")
    String platform;        // Platform number/letter (e.g., "4", "1A", "-")
    String destinationName; // Primary destination name
    String destinationCrs;  // Primary destination CRS
    String via;             // Optional via text (e.g., "via Newbury")
    String operatorName;    // Train Operating Company (e.g., "Great Western Railway")
    String operatorCode;    // TOC code (e.g., "GW")
    String cancelReason;    // Cancellation reason if cancelled
    String delayReason;     // Delay reason if delayed
    bool isCancelled = false;
    uint8_t length = 0;     // Number of carriages (if known)

    CallingPoint callingPoints[MAX_CALLING_POINTS];
    uint8_t callingPointCount = 0;

    // Pre-formatted scrolling ticker string for this service
    String tickerText;
};

struct BoardData {
    String locationName;    // e.g., "London Paddington"
    String crs;             // e.g., "PAD"
    String filterLocationName;
    String filterCrs;
    String nrccMessage;     // Stripped station alert/disruption message
    TrainService services[MAX_SERVICES];
    uint8_t serviceCount = 0;
    bool areServicesAvailable = true;
    bool fetchSuccess = false;
    String errorMessage;
    unsigned long lastUpdatedMillis = 0;
};

struct AppConfig {
    String wifiSsid;
    String wifiPass;
    String stationCrs;      // 3-letter CRS code (e.g., "PAD")
    String filterCrs;       // Optional 3-letter "calling at" filter CRS
    String huxleyUrl;       // Base Huxley2 URL (no trailing slash)
    String darwinToken;     // Optional Darwin API token
    uint8_t brightness = DEFAULT_BRIGHTNESS;
};

enum class ScreenMode {
    DEPARTURE_BOARD,
    CRS_KEYBOARD,
    WIFI_INFO
};
