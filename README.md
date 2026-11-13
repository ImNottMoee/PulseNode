# PulseNode

**IoT pulse oximeter and heart rate monitor.** An ESP32 reads heart rate and blood oxygen (SpO₂), shows them on an OLED screen, and streams them to the cloud, where signed-in users watch live readings, trends and alerts from any phone or computer.

**Try the demo (no account needed):** https://imnottmoee.github.io/PulseNode/?demo
Simulated readings generated in your browser, so you can explore every part of the dashboard even when the device is offline.

**Live dashboard:** https://imnottmoee.github.io/PulseNode/ (account required; shows real-time data from the device when it's powered on)

![PulseNode dashboard in demo mode](docs/dashboard-demo.png)

> **Status:** the full device → cloud → dashboard pipeline is working end to end. The vitals are currently **simulated** on the ESP32 while the MAX30102 sensor is being integrated; the dashboard labels this data "SIMULATED".

> ⚠️ Student portfolio project. **Not a medical device.** Do not use it for medical decisions.

---

## Features

**Device (ESP32 + OLED)**
- Live heart rate, SpO₂ and a scrolling pulse waveform on a 0.96" OLED
- On-device alerts: low SpO₂ (< 92 %), low heart rate (< 50 BPM), high heart rate (> 120 BPM). The screen flashes and shows the alert
- Dual-core firmware: the display runs on one core while cloud uploads run on the other, so network delays never freeze the screen
- Signs in to Firebase with its own device account and renews its access token automatically
- Uploads once per second, plus **immediately** when an alert starts or clears
- Reconnects to Wi-Fi and retries uploads automatically after dropouts

**Cloud (Firebase)**
- Firebase Realtime Database for live readings
- Firebase Authentication for user accounts and the device account
- Locked security rules: only signed-in users can read live data, only the device account can write it, and each user can only access their own profile data

**Web dashboard**
- Name + 6-digit PIN accounts (create account / log in)
- Demo mode with simulated data (`?demo`), so visitors can try it without an account or the device
- Live heart rate and SpO₂ with Normal / Low / High status tags
- Trend charts with alert-threshold lines and 1 min / 5 min / All time ranges
- Flashing alert banner and an alert log with start times and durations
- Session summary: min / average / max, reading count, alerts, time in alert
- Device health: Wi-Fi signal strength, uptime, firmware version, upload time and delivery delay
- Works on phone and desktop (single self-contained HTML file hosted on GitHub Pages)

## How it works

```mermaid
flowchart LR
    S["MAX30102 sensor<br/>(integration in progress)"] -- I2C --> E[ESP32]
    E -- I2C --> O[OLED display]
    E -- "HTTPS, every 1 s<br/>+ instantly on alerts" --> F[(Firebase<br/>Realtime Database)]
    A[Firebase Auth] -. device + user logins .- F
    F -- live stream --> D[Web dashboard<br/>GitHub Pages]
    U[Phone / laptop] --> D
```

1. The ESP32 measures (currently simulates) heart rate and SpO₂ and draws them on the OLED.
2. A background task on the ESP32's second core signs in as the device and writes the latest reading to `devices/pulsenode-01/live` in Firebase once per second.
3. The dashboard signs the user in and subscribes to that path, so new readings appear within about a second.

## Hardware

| Part | Notes |
|---|---|
| ESP32-WROOM-32 dev board (30-pin) | Dual-core MCU with built-in Wi-Fi |
| MAX30102 module (MH-ET LIVE) | Red + IR optical sensor for heart rate and SpO₂ |
| 0.96" SSD1306 OLED, 128×64, I2C | Two-color (yellow header band, blue body) |
| Breadboard and jumper wires | |

### Wiring (shared I2C bus)

| Signal | ESP32 pin | OLED | MAX30102 |
|---|---|---|---|
| 3.3 V | 3V3 → + rail | VCC | VIN |
| Ground | GND → − rail | GND | GND |
| Data (SDA) | GPIO 21 | SDA | SDA |
| Clock (SCL) | GPIO 22 | SCL | SCL |

I2C addresses: OLED `0x3C`, MAX30102 `0x57`. Full wiring notes, including breadboard placement and known module quirks, are in [docs/wiring.md](docs/wiring.md).

## Repository layout

```
index.html                            Web dashboard (served by GitHub Pages)
database.rules.json                   Firebase Realtime Database security rules
firmware/PulseNode/PulseNode.ino      ESP32 firmware (current version: 0.4.0)
firmware/PulseNode/secrets.example.h  Template for Wi-Fi and Firebase credentials
docs/wiring.md                        Wiring table and pin notes
CHANGELOG.md                          Firmware version history
```

## Setup

**Firmware**
1. Install Arduino IDE 2.x, the **esp32 by Espressif Systems** board package, and the **Adafruit SSD1306** library (with Adafruit GFX). The **SparkFun MAX3010x** library is only needed for the upcoming sensor work; firmware 0.4.0 doesn't use it yet.
2. Copy `firmware/PulseNode/secrets.example.h` to `secrets.h` in the same folder and fill in your Wi-Fi details, device account and Firebase API key. `secrets.h` is git-ignored.
3. Board: **ESP32 Dev Module**. Upload, then check the Serial Monitor (115200 baud) for `Device signed in to Firebase` and `HTTP 200`.

**Firebase**
1. Create a project with a Realtime Database and enable **Email/Password** authentication.
2. Add a user for the device (e.g. `device-01@pulsenode.app`) and put its UID into `database.rules.json`.
3. Publish `database.rules.json` as the database rules.

**Dashboard**
1. Put your Firebase web config into `index.html`.
2. Host it anywhere static (this repo uses GitHub Pages) and create an account on the login screen.

## Roadmap

**Done:** OLED display with live vitals, waveform and alerts · Wi-Fi and Firebase uploads (1 s rhythm + instant alerts) · live web dashboard with trends, alert log and device health · user accounts with PINs and locked database rules · GitHub Pages hosting. See [CHANGELOG.md](CHANGELOG.md).

- [ ] **Phase 1: Real sensor.** Solder the MAX30102 and read raw red/IR data; filtering, peak detection, heart rate, SpO₂ via ratio-of-ratios, finger detection and a signal-quality indicator
- [ ] **Phase 2: Sessions and history.** "Start measurement" sessions saved to each user's history in Firebase, plus a history page
- [ ] **Phase 3: Home-screen app.** Installable web app (manifest, icon, full-screen)
- [ ] **Phase 4: Personal alerts.** Per-user alert thresholds and phone notifications
- [ ] **Phase 5: Reliability.** TLS certificate checking instead of `setInsecure()`, shorter timeouts, offline buffering
- [ ] **Phase 6: Validation.** Compare against a commercial pulse oximeter and publish the error statistics here

## Tech

ESP32 (Arduino / FreeRTOS tasks) · I2C · Adafruit SSD1306 · Firebase Realtime Database + Authentication (REST on the device, JS SDK on the web) · HTML/CSS/JavaScript · Chart.js · GitHub Pages
