#pragma once

#include "types.h"

class ConfigStorage {
public:
    static void load(AppConfig& config);
    static void save(const AppConfig& config);
    static void saveStationCrs(const String& stationCrs, const String& filterCrs = "");
    static String sanitizeCrs(const String& raw);
};
