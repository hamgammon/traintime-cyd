#pragma once

#include <Arduino.h>
#include <FS.h>
using fs::FS;
#include <WebServer.h>
#include <DNSServer.h>
#include "types.h"

typedef void (*OnWebConfigSavedCallback)(bool wifiChanged, bool stationChanged);

class WebPortal {
public:
    WebPortal(AppConfig& config, BoardData& board);

    void begin(bool apMode, OnWebConfigSavedCallback onSaved);
    void handleClient();
    bool isApMode() const { return _apMode; }

private:
    AppConfig& _config;
    BoardData& _board;
    WebServer _server;
    DNSServer _dnsServer;
    bool _apMode = false;
    OnWebConfigSavedCallback _onSaved = nullptr;

    void handleRoot();
    void handleSaveStation();
    void handleSaveSettings();
    void handleStatusJson();
};
