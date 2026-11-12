# Changelog

Firmware versions for `firmware/PulseNode/PulseNode.ino`. Dashboard and cloud changes that shipped alongside each version are listed with it.

## 0.4.0: Device login and locked database
- The ESP32 signs in to Firebase with its own device account (`device-01@pulsenode.app`) over the Identity Toolkit REST API and sends its access token with every upload.
- Renews the token every 50 minutes, and logs in again immediately if an upload is rejected with HTTP 401.
- Credentials moved to `secrets.h` (git-ignored), with `secrets.example.h` as a template.
- Dashboard: name + 6-digit PIN accounts (create account / log in / log out) using Firebase Authentication; live data now read through the Firebase JS SDK.
- Database rules locked: only signed-in users can read live data, only the device account can write it, and each user's `users/<uid>` data is private to them.

## 0.3.0: Faster uploads, instant alerts, device health
- Uploads every 1 s instead of every 2 s.
- Uploads immediately when an alert starts or clears (FreeRTOS task notification), instead of waiting for the next cycle.
- Keeps the HTTPS connection open between uploads (`setReuse`), cutting typical upload time to about 80–90 ms.
- Sends device health with each reading: Wi-Fi RSSI, uptime, firmware version, and the previous upload's duration.
- Dashboard: device health card, update rate and delivery-delay readouts; time-range buttons (1 min / 5 min / All) that rescale the trend charts.

## 0.2.0: Wi-Fi and Firebase cloud
- Connects to Wi-Fi and reconnects automatically after dropouts.
- Uploads heart rate, SpO₂, alert state and a server timestamp to Firebase Realtime Database at `devices/pulsenode-01/live`.
- Dual-core design: display and vitals on core 1, cloud uploads on core 0, so network delays never freeze the screen.
- OLED header shows `UP` when uploads are succeeding.
- First web dashboard: live numbers, alert banner and trend charts, hosted on GitHub Pages.

## 0.1.0: OLED display and simulated vitals
- SSD1306 OLED on the shared I2C bus (SDA GPIO21, SCL GPIO22).
- Simulated heart rate and SpO₂ with a scrolling pulse waveform.
- On-device alerts for low SpO₂ (< 92 %), low heart rate (< 50 BPM) and high heart rate (> 120 BPM); the screen flashes and shows the alert.
- Simulation dips SpO₂ for 6 s every 20 s to exercise the alert path.
- Layout fitted to the two-color display: header in the yellow band, data in the blue area.
