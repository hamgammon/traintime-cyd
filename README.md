# TrainTime CYD — UK Railway Departure Board for ESP32-2432S028R

A classic UK railway dot-matrix style live departure board built for the **DIYmalls 2.8" ESP32-2432S028R ("Cheap Yellow Display" / CYD)**.

## Features

- **Classic UK Dot-Matrix Aesthetics**: Amber-on-black LED departure board layout with smooth 40 FPS sprite-buffered scrolling calling points (`Calling at: Gleneagles (05:31), Dunblane (05:44)...`), coach formation counts, platform numbers, and a live NTP-synchronised UK platform clock (`GMT`/`BST`).
- **Dual-API Resilience (Huxley2 + Automatic RailFeed Fallback)**:
  - Queries **Huxley2** (`huxley2.azurewebsites.net`, or a custom Huxley2 URL + optional personal National Rail Darwin `accessToken`).
  - Automatically falls back to **RailFeed** (`railfeed.co.uk`) whenever the public Huxley2 instance hits National Rail's shared monthly token quota (`HTTP 500`), with a 15-minute cooldown so subsequent 30-second refreshes are instant.
  - Automatically expands the lookahead window from 2 hours to 10 hours overnight or at quieter stations so the next morning's departures stay visible.
  - Caches calling points per service UID in RAM and streams JSON through `ArduinoJson` v7 filters to keep ESP32 heap usage minimal.
- **Change Station Code Anytime (2 Ways)**:
  1. **Directly on the CYD Touchscreen**: Tap the **`[SET CRS]`** button in the top-right corner (or anywhere on the station header) to open the on-screen QWERTY touch keyboard. Enter any 3-letter UK CRS code (e.g. `PTH`, `EDB`, `GLQ`, `PAD`, `KGX`, `EUS`, `MAN`) with instant station-name preview, or tap one of the 60+ built-in quick-preset chips. You can also set an optional **`FILTER (TO)`** CRS code to only show trains calling at a specific station.
  2. **Via Phone / PC Web Browser**: Connect to the device's web interface (`http://traintime.local/` or its local IP shown in the bottom-left corner of the display) to search stations by name, change the CRS code, adjust screen brightness, or update Wi-Fi / API settings.
- **Interactive Calling Points**: Tap any departure row on the screen to inspect that train's calling points, coach count, delay/cancellation reason, and operator in the scrolling ticker.
- **Zero-Code Wi-Fi Setup**: On first boot (or if Wi-Fi is unavailable), the CYD launches an access point named **`TrainTime-CYD`** (`http://192.168.4.1`) with a captive portal to configure your 2.4 GHz Wi-Fi network and initial station code.
- **Persistent Settings**: Station CRS, filter CRS, Wi-Fi credentials, Huxley2 endpoint/token, and backlight brightness are stored in ESP32 NVS flash (`Preferences`) and survive power cycles.

---

## Project Structure

- [`platformio.ini`](file:///Users/scottcowie/repos/traintime-cyd/platformio.ini) — PlatformIO build configuration for the `ESP32-2432S028R` (`TFT_eSPI` on `HSPI`, `XPT2046_Touchscreen` on `VSPI`, and `ArduinoJson` configured via build flags—no manual library file edits needed).
- [`include/config.h`](file:///Users/scottcowie/repos/traintime-cyd/include/config.h) — Pin definitions, dot-matrix amber RGB565 palette, timing constants, and built-in UK station CRS lookup table.
- [`include/types.h`](file:///Users/scottcowie/repos/traintime-cyd/include/types.h) — Data structures for services, calling points, board state, and configuration.
- [`include/storage.h`](file:///Users/scottcowie/repos/traintime-cyd/include/storage.h) / [`src/storage.cpp`](file:///Users/scottcowie/repos/traintime-cyd/src/storage.cpp) — NVS persistence via ESP32 `Preferences`.
- [`include/train_api.h`](file:///Users/scottcowie/repos/traintime-cyd/include/train_api.h) / [`src/train_api.cpp`](file:///Users/scottcowie/repos/traintime-cyd/src/train_api.cpp) — Dual-source UK rail client (`Huxley2` primary + `RailFeed` automatic fallback) using `ArduinoJson` v7 filtered parsing and calling-point caching.
- [`include/display_ui.h`](file:///Users/scottcowie/repos/traintime-cyd/include/display_ui.h) / [`src/display_ui.cpp`](file:///Users/scottcowie/repos/traintime-cyd/src/display_ui.cpp) — `TFT_eSPI` departure board renderer, double-buffered scrolling ticker & clock sprites, and on-screen QWERTY CRS touch keyboard.
- [`include/web_portal.h`](file:///Users/scottcowie/repos/traintime-cyd/include/web_portal.h) / [`src/web_portal.cpp`](file:///Users/scottcowie/repos/traintime-cyd/src/web_portal.cpp) — Captive portal + local web configuration server.
- [`src/main.cpp`](file:///Users/scottcowie/repos/traintime-cyd/src/main.cpp) — Hardware initialisation, XPT2046 touch polling on `VSPI`, Wi-Fi/NTP management, and main event loop.

---

## Building & Flashing

1. Install [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) (VS Code extension) or the PlatformIO Core CLI (`brew install platformio` on macOS).
2. Connect your **ESP32-2432S028R** board via a data-capable **Micro-USB** cable (upload speed is set to `460800` baud in `platformio.ini` for reliable flashing over the onboard CH340 USB-serial bridge).
3. Build and upload:
   ```bash
   pio run --target upload
   ```
4. (Optional) Open the serial monitor at `115200` baud:
   ```bash
   pio device monitor
   ```

---

## First-Time Setup

1. Power on the CYD. On first boot, it creates a Wi-Fi access point named **`TrainTime-CYD`**.
2. Connect to **`TrainTime-CYD`** from your phone or computer and open **`http://192.168.4.1`** (if the captive portal doesn't pop up automatically).
3. Enter your **2.4 GHz Wi-Fi SSID & Password** and your preferred **3-letter UK Station CRS code** (e.g. `PTH` for Perth, `EDB` for Edinburgh Waverley, `PAD` for London Paddington, `MAN` for Manchester Piccadilly), then click **Save Settings**.
4. Once connected to Wi-Fi, tap **`[SET CRS]`** in the top-right corner of the touchscreen anytime to switch stations on the fly!
