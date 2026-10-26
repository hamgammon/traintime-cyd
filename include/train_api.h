#pragma once

#include "types.h"

class TrainApiClient {
public:
    static bool fetchDepartures(const AppConfig& config, BoardData& board);

private:
    static bool fetchFromHuxley2(const AppConfig& config, BoardData& board);
    static bool fetchFromRailFeed(const AppConfig& config, BoardData& board);
    static bool fetchRailFeedWindow(const String& crs, const String& filterCrs, uint8_t hours, BoardData& board);
    static bool fetchRailFeedCallingPoints(const String& uid, const String& dateStr, const String& originCrs, TrainService& svc);

    static String stripHtmlAndClean(const String& input);
    static String toTitleCase(const String& input);
    static String formatHhMm(const char* hhmmss);
    static String addMinutesToHhMm(const String& hhmm, int delayMins);
    static String buildTickerText(const TrainService& svc);
};
