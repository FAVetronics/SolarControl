# SolarControl

ESP32-based solar inverter controller for a Deye hybrid inverter. Runs autonomously on the local network, polling the inverter over Modbus RS485 and making hourly decisions about battery charging, PV selling, and heat pump defrost protection — driven by real-time Danish electricity spot prices and DMI weather data.

## What it does

**Every 200 ms** — reads five blocks of Modbus holding registers from the Deye inverter (production, consumption, battery power, SoC, grid power, totals).

**Every hour** — updates historical data in NVS flash (hourly consumption, production, charge rate, outdoor temperature), fetches today's NordPool spot prices, and runs the three control decisions below.

**Every 15 minutes** — checks whether the heat pump condensate needs freeze protection and drives a Shelly relay accordingly.

**Always** — serves a local web dashboard on port 80 and a JSON API at `/values` and `/settings`.

### Battery charge scheduling

Decides which hours today to charge from PV. Identifies the remaining solar window (sunrise/sunset via `SolarCalculator`, cross-checked against yesterday's actual production data), estimates the time needed to reach full charge, and enables charging only during the cheapest hours inside that window. If fewer PV hours remain than the charge estimate requires, it charges immediately.

### Selling suppression

Suppresses PV export when the spot payout (after feed-in tariffs) is negative or below 0.01 DKK/kWh, avoiding paying to give power away.

### Defrost control

Fetches `temp_dry` from DMI station 06068 every 15 minutes. Activates a Shelly relay (heating element) when the temperature is between −2 °C and +2 °C to prevent condensate from freezing on the heat pump.

## Hardware

| Component | Detail |
|---|---|
| MCU | ESP32 (static IP 192.168.1.101) or ESP32-S3 (192.168.1.102) |
| Inverter | Deye hybrid, Modbus RTU over RS485 (auto direction control) |
| Defrost relay | Shelly Plus 1 (GET) or Shelly Plug S (POST) at 192.168.2.123 |
| PV array | 36 panels, ~68 m², system efficiency ≈ 16% |
| Battery | 2 × 8 800 Wh, 48 V nominal, max charge/discharge 68 A |

## Software structure

```
src/
  main.cpp       — setup/loop, all control logic, NVS persistence
  web.cpp        — WiFi, async web server, price/DMI/irradiance fetches, relay control
  web.h          — DMI endpoint config, query string constants
  Inverter.cpp   — Modbus batch reads and register accessors
  Inverter.h     — register map constants, public inverter API
  userData.h     — all site-specific constants (location, battery spec, tariffs, WiFi)
  LED.cpp/h      — status LED
```

## External APIs

| API | Purpose |
|---|---|
| [billigkwh.dk](https://billigkwh.dk) | NordPool spot prices for DK1 (HTTPS, Ikast El Net tariff) |
| [opendataapi.dmi.dk](https://opendataapi.dmi.dk) | Outdoor temperature from DMI station 06068 (no API key required) |
| DMI metObs (irradiance) | `radia_glob` from station 06068 — 10-minute solar irradiance for production forecasting |

## Configuration

All site-specific settings live in `src/userData.h`:

- **WiFi SSID/password** — hardcoded in `web.cpp`
- **Location** — latitude/longitude for sunrise/sunset calculation
- **Battery spec** — capacity, charge/discharge rates, wear cost
- **Tariffs** — feed-in tariffs, `elafgift`, commercial plant flag (strips VAT)
- **Defrost relay** — IP address, HTTP method (GET/POST)
- **Price source** — toggle `use_billigkwh` / `use_elprisenligenu`

## Build

Built with [PlatformIO](https://platformio.org/). Key library dependencies:

- `ModbusMaster` — Modbus RTU
- `ESPAsyncWebServer-esphome` + `AsyncTCP-esphome` — async HTTP server
- `ArduinoJson` v6 — JSON parsing
- `NTPClient` — time sync (europe.pool.ntp.org)
- `SolarCalculator` — sunrise/sunset
- `ArduinoNvs` — NVS flash storage

## Web interface

| Endpoint | Content |
|---|---|
| `/` | Live dashboard — production, consumption, battery SoC, outdoor temp, current payout |
| `/values` | JSON — real-time inverter readings |
| `/settings` | JSON — inverter battery configuration read from Modbus |

## Notes

- Daylight saving time is adjusted automatically using EU rules (clocks forward last Sunday of March at 02:00 CET, back last Sunday of October at 03:00 CEST). The offset is re-evaluated every hour and on startup.
- Grid-charge-for-arbitrage logic exists in `main.cpp` but is wrapped in `#ifdef OLD_STUF` and not currently active.
- The `elprisenligenu` price source path has a hardcoded date in the URL and is not production-ready.
- The SSL certificate pinned in `UpdateNordPoolPrices` expires **March 12, 2027**.
