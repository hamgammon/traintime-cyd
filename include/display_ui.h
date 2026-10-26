#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "types.h"

// Callback signature when user saves a new station CRS from the touch keyboard
typedef void (*OnCrsChangedCallback)(const String& newStationCrs, const String& newFilterCrs);
typedef void (*OnManualRefreshCallback)();

class DisplayUI {
public:
    DisplayUI();

    void begin(uint8_t brightness);
    void setBrightness(uint8_t brightness);

    // State transitions
    void showSplash(const String& statusText);
    void showApPortalInfo(const String& apSsid, const String& apIp);
    void showDepartureBoard(const BoardData& board, const AppConfig& config);
    void openCrsKeyboard(const AppConfig& config);

    // Periodic non-blocking animations & clock updates (call in loop())
    void tick(const BoardData& board, const AppConfig& config);

    // Touch handling (call when XPT2046 reports touch at screen coords x, y)
    void handleTouch(int16_t x, int16_t y, BoardData& board, AppConfig& config,
                     OnCrsChangedCallback onCrsChanged,
                     OnManualRefreshCallback onRefresh);

    void setFetchingIndicator(bool fetching);
    ScreenMode getScreenMode() const { return _mode; }

private:
    TFT_eSPI _tft;
    TFT_eSprite _tickerSprite;
    TFT_eSprite _clockSprite;

    ScreenMode _mode = ScreenMode::DEPARTURE_BOARD;
    uint8_t _selectedServiceIdx = 0;

    // Scrolling ticker state
    String _activeTickerText;
    int16_t _tickerScrollX = SCREEN_WIDTH;
    int16_t _tickerTextWidth = 0;
    unsigned long _lastTickerTick = 0;
    unsigned long _lastClockTick = 0;
    String _lastClockStr;

    // CRS Keyboard state
    bool _editingFilterCrs = false;
    String _kbStationInput;
    String _kbFilterInput;
    uint8_t _presetPage = 0;

    // Drawing helpers - Departure board
    void drawBoardHeader(const BoardData& board, const AppConfig& config);
    void drawBoardRows(const BoardData& board);
    void drawBottomBar(const AppConfig& config);
    void updateTickerSprite();
    void updateClockSprite();
    void rebuildActiveTicker(const BoardData& board);

    // Drawing helpers - CRS Touch Keyboard
    void drawKeyboardScreen();
    void drawKeyboardInputBox();
    void drawKeyboardPresets();
    void drawKeyboardKeys();
    void handleKeyboardTouch(int16_t x, int16_t y, BoardData& board, AppConfig& config,
                             OnCrsChangedCallback onCrsChanged);

    // Drawing helpers - Wi-Fi / Web UI modal
    void drawWifiInfoScreen(const AppConfig& config);
};
