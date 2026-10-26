#include "web_portal.h"
#include "config.h"
#include "storage.h"
#include <WiFi.h>
#include <ESPmDNS.h>

static const byte DNS_PORT = 53;

WebPortal::WebPortal(AppConfig& config, BoardData& board)
    : _config(config), _board(board), _server(80) {}

void WebPortal::begin(bool apMode, OnWebConfigSavedCallback onSaved) {
    _apMode = apMode;
    _onSaved = onSaved;

    if (_apMode) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_PORTAL_SSID);
        delay(100);
        _dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    } else {
        MDNS.begin(MDNS_HOSTNAME);
        MDNS.addService("http", "tcp", 80);
    }

    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/save-station", HTTP_POST, [this]() { handleSaveStation(); });
    _server.on("/save-settings", HTTP_POST, [this]() { handleSaveSettings(); });
    _server.on("/api/status", HTTP_GET, [this]() { handleStatusJson(); });

    // Captive portal redirect for Android / iOS / Windows connectivity checks
    _server.onNotFound([this]() {
        if (_apMode) {
            _server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
            _server.send(302, "text/plain", "");
        } else {
            handleRoot();
        }
    });

    _server.begin();
}

void WebPortal::handleClient() {
    if (_apMode) {
        _dnsServer.processNextRequest();
    }
    _server.handleClient();
}

static String htmlEscape(const String& s) {
    String out = s;
    out.replace("&", "&amp;");
    out.replace("<", "&lt;");
    out.replace(">", "&gt;");
    out.replace("\"", "&quot;");
    return out;
}

void WebPortal::handleRoot() {
    String html;
    html.reserve(7500);

    html += F("<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<title>TrainTime CYD - UK Departures</title>"
              "<style>"
              ":root{--bg:#0a0a0c;--card:#141619;--amber:#ffb000;--amber-hi:#ffd040;--amber-dim:#8a6200;--border:#2c2618;}"
              "*{box-sizing:border-box;font-family:'Courier New',monospace;}"
              "body{margin:0;padding:16px;background:var(--bg);color:var(--amber);max-width:600px;margin-inline:auto;}"
              "h1{margin:0 0 4px;color:var(--amber-hi);font-size:1.35rem;letter-spacing:1px;}"
              ".sub{color:var(--amber-dim);font-size:0.85rem;margin-bottom:16px;}"
              ".card{background:var(--card);border:1px solid var(--border);border-radius:8px;padding:16px;margin-bottom:16px;}"
              "h2{margin:0 0 12px;font-size:1rem;color:var(--amber-hi);border-bottom:1px dashed var(--amber-dim);padding-bottom:6px;}"
              "label{display:block;font-size:0.85rem;margin:10px 0 4px;color:var(--amber);}"
              "input,select{width:100%;padding:10px;background:#060708;border:1px solid var(--amber-dim);color:var(--amber-hi);"
              "border-radius:4px;font-size:1rem;}"
              "input.crs{text-transform:uppercase;font-size:1.25rem;font-weight:bold;letter-spacing:3px;}"
              "button{width:100%;margin-top:14px;padding:12px;background:var(--amber);color:#000;font-weight:bold;"
              "border:none;border-radius:4px;font-size:1rem;cursor:pointer;}"
              "button:hover{background:var(--amber-hi);}"
              ".chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:10px;}"
              ".chip{background:#1f1b12;border:1px solid var(--amber-dim);color:var(--amber-hi);padding:4px 8px;"
              "border-radius:4px;font-size:0.8rem;cursor:pointer;}"
              ".board-preview{background:#050505;border:1px solid var(--amber-dim);padding:10px;border-radius:4px;font-size:0.85rem;}"
              ".row{display:flex;justify-content:space-between;padding:3px 0;border-bottom:1px dotted #221a08;}"
              "</style></head><body>");

    html += F("<h1>UK RAIL DEPARTURE BOARD</h1>"
              "<div class='sub'>Cheap Yellow Display (ESP32-2432S028R) Controller</div>");

    // Live Preview Card
    html += F("<div class='card'><h2>LIVE BOARD STATUS</h2><div class='board-preview'>");
    html += "<div><strong>" + htmlEscape(_board.locationName.isEmpty() ? _config.stationCrs : _board.locationName) +
            " [" + htmlEscape(_config.stationCrs) + "]</strong>";
    if (_config.filterCrs.length() == 3) {
        html += " &rarr; calling at <strong>[" + htmlEscape(_config.filterCrs) + "]</strong>";
    }
    html += F("</div><hr style='border-color:#2c2618'>");
    if (_board.serviceCount == 0) {
        html += "<div>" + htmlEscape(_board.fetchSuccess ? "No departures currently listed." : _board.errorMessage) + "</div>";
    } else {
        for (uint8_t i = 0; i < _board.serviceCount; ++i) {
            const TrainService& s = _board.services[i];
            html += "<div class='row'><span>" + htmlEscape(s.std) + " " + htmlEscape(s.destinationName) +
                    "</span><span>Plat " + htmlEscape(s.platform.isEmpty() ? "-" : s.platform) +
                    " | " + htmlEscape(s.etd) + "</span></div>";
        }
    }
    html += F("</div></div>");

    // Quick Station CRS Form
    html += F("<div class='card'><h2>SET STATION CRS CODE</h2>"
              "<form method='POST' action='/save-station'>"
              "<label>Departure Station (3-Letter UK CRS Code)</label>"
              "<input class='crs' id='stationCrs' name='stationCrs' maxlength='3' minlength='3' required list='crsList' value='");
    html += htmlEscape(_config.stationCrs);
    html += F("'>"
              "<label>Optional Filter: Calling At Station (3-Letter CRS, or leave blank for all)</label>"
              "<input class='crs' id='filterCrs' name='filterCrs' maxlength='3' list='crsList' placeholder='ALL' value='");
    html += htmlEscape(_config.filterCrs);
    html += F("'>"
              "<div class='chips'>");

    // Quick-tap preset buttons
    const char* quickCrs[] = {"PAD", "KGX", "EUS", "WAT", "VIC", "LST", "LBG", "STP", "MAN", "BHM", "EDB", "GLC", "LDS", "BRI", "RDG", "CBG", "OXF", "YRK"};
    for (size_t i = 0; i < sizeof(quickCrs) / sizeof(quickCrs[0]); ++i) {
        html += "<span class='chip' onclick=\"document.getElementById('stationCrs').value='";
        html += quickCrs[i];
        html += "'\">";
        html += quickCrs[i];
        html += "</span>";
    }
    html += F("</div><button type='submit'>UPDATE STATION NOW</button></form></div>");

    // Datalist of UK stations for easy search
    html += F("<datalist id='crsList'>");
    for (size_t i = 0; i < UK_STATION_PRESETS_COUNT; ++i) {
        html += "<option value='";
        html += UK_STATION_PRESETS[i].crs;
        html += "'>";
        html += htmlEscape(UK_STATION_PRESETS[i].name);
        html += "</option>";
    }
    html += F("</datalist>");

    // Wi-Fi, API & Brightness Form
    html += F("<div class='card'><h2>WI-FI, API &amp; DISPLAY SETTINGS</h2>"
              "<form method='POST' action='/save-settings'>"
              "<label>Wi-Fi Network SSID</label>"
              "<input name='wifiSsid' placeholder='Your 2.4GHz Wi-Fi SSID' value='");
    html += htmlEscape(_config.wifiSsid);
    html += F("'>"
              "<label>Wi-Fi Password</label>"
              "<input type='password' name='wifiPass' placeholder='Wi-Fi Password' value='");
    html += htmlEscape(_config.wifiPass);
    html += F("'>"
              "<label>Huxley2 JSON API Base URL</label>"
              "<input name='huxleyUrl' value='");
    html += htmlEscape(_config.huxleyUrl);
    html += F("'>"
              "<label>Optional Darwin API Access Token (leave blank if using public proxy)</label>"
              "<input name='darwinToken' placeholder='Optional UUID token' value='");
    html += htmlEscape(_config.darwinToken);
    html += F("'>"
              "<label>Display Backlight Brightness (20 - 255)</label>"
              "<input type='range' name='brightness' min='20' max='255' value='");
    html += String(_config.brightness);
    html += F("'>"
              "<button type='submit'>SAVE SETTINGS</button></form></div>"
              "</body></html>");

    _server.send(200, "text/html", html);
}

void WebPortal::handleSaveStation() {
    String newStation = ConfigStorage::sanitizeCrs(_server.arg("stationCrs"));
    String newFilter = ConfigStorage::sanitizeCrs(_server.arg("filterCrs"));

    bool changed = false;
    if (newStation.length() == 3) {
        _config.stationCrs = newStation;
        _config.filterCrs = (newFilter.length() == 3) ? newFilter : "";
        ConfigStorage::saveStationCrs(_config.stationCrs, _config.filterCrs);
        changed = true;
    }

    _server.sendHeader("Location", "/", true);
    _server.send(303, "text/plain", "Updated");

    if (changed && _onSaved) {
        _onSaved(false, true);
    }
}

void WebPortal::handleSaveSettings() {
    String newSsid = _server.arg("wifiSsid");
    newSsid.trim();
    String newPass = _server.arg("wifiPass");

    bool wifiChanged = (newSsid != _config.wifiSsid) || (newPass != _config.wifiPass);
    _config.wifiSsid = newSsid;
    _config.wifiPass = newPass;

    String newUrl = _server.arg("huxleyUrl");
    newUrl.trim();
    if (!newUrl.isEmpty()) {
        _config.huxleyUrl = newUrl;
    }

    _config.darwinToken = _server.arg("darwinToken");
    _config.darwinToken.trim();

    if (_server.hasArg("brightness")) {
        int b = _server.arg("brightness").toInt();
        _config.brightness = constrain(b, 20, 255);
    }

    ConfigStorage::save(_config);

    _server.sendHeader("Location", "/", true);
    _server.send(303, "text/plain", "Saved");

    if (_onSaved) {
        _onSaved(wifiChanged, true);
    }
}

void WebPortal::handleStatusJson() {
    String json = "{\"stationCrs\":\"" + _config.stationCrs +
                  "\",\"filterCrs\":\"" + _config.filterCrs +
                  "\",\"locationName\":\"" + _board.locationName +
                  "\",\"serviceCount\":" + String(_board.serviceCount) +
                  ",\"fetchSuccess\":" + String(_board.fetchSuccess ? "true" : "false") + "}";
    _server.send(200, "application/json", json);
}
