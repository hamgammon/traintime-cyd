#include "storage.h"
#include <Preferences.h>

static const char* NVS_NAMESPACE = "traintime";

String ConfigStorage::sanitizeCrs(const String& raw) {
    String clean = "";
    for (size_t i = 0; i < raw.length() && clean.length() < 3; ++i) {
        char c = raw.charAt(i);
        if (isAlpha(c)) {
            clean += (char)toupper(c);
        }
    }
    return clean;
}

void ConfigStorage::load(AppConfig& config) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true); // Read-only

    config.wifiSsid = prefs.getString("wifiSsid", "");
    config.wifiPass = prefs.getString("wifiPass", "");
    config.stationCrs = sanitizeCrs(prefs.getString("stationCrs", DEFAULT_STATION_CRS));
    if (config.stationCrs.length() != 3) {
        config.stationCrs = DEFAULT_STATION_CRS;
    }
    config.filterCrs = sanitizeCrs(prefs.getString("filterCrs", ""));
    if (config.filterCrs.length() != 3) {
        config.filterCrs = "";
    }
    config.huxleyUrl = prefs.getString("huxleyUrl", DEFAULT_HUXLEY_URL);
    config.huxleyUrl.trim();
    while (config.huxleyUrl.endsWith("/")) {
        config.huxleyUrl.remove(config.huxleyUrl.length() - 1);
    }
    if (config.huxleyUrl.isEmpty()) {
        config.huxleyUrl = DEFAULT_HUXLEY_URL;
    }

    config.darwinToken = prefs.getString("darwinToken", "");
    config.darwinToken.trim();

    config.brightness = prefs.getUChar("brightness", DEFAULT_BRIGHTNESS);
    if (config.brightness < 15) {
        config.brightness = 15;
    }

    prefs.end();
}

void ConfigStorage::save(const AppConfig& config) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false); // Read-write

    prefs.putString("wifiSsid", config.wifiSsid);
    prefs.putString("wifiPass", config.wifiPass);
    prefs.putString("stationCrs", sanitizeCrs(config.stationCrs));
    prefs.putString("filterCrs", sanitizeCrs(config.filterCrs));

    String cleanUrl = config.huxleyUrl;
    cleanUrl.trim();
    while (cleanUrl.endsWith("/")) {
        cleanUrl.remove(cleanUrl.length() - 1);
    }
    if (cleanUrl.isEmpty()) {
        cleanUrl = DEFAULT_HUXLEY_URL;
    }
    prefs.putString("huxleyUrl", cleanUrl);
    prefs.putString("darwinToken", config.darwinToken);
    prefs.putUChar("brightness", config.brightness < 15 ? 15 : config.brightness);

    prefs.end();
}

void ConfigStorage::saveStationCrs(const String& stationCrs, const String& filterCrs) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    String cleanStation = sanitizeCrs(stationCrs);
    if (cleanStation.length() == 3) {
        prefs.putString("stationCrs", cleanStation);
    }
    String cleanFilter = sanitizeCrs(filterCrs);
    prefs.putString("filterCrs", cleanFilter.length() == 3 ? cleanFilter : "");
    prefs.end();
}
