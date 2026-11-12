#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "secrets.h"

// ----- Firmware info -----
const char* FW_VERSION = "0.4.0";

// ----- Cloud settings -----
const char* DB_URL    = "https://pulsenode-ac8b7-default-rtdb.firebaseio.com";
const char* DEVICE_ID = "pulsenode-01";
const unsigned long UPLOAD_EVERY_MS = 1000;            // normal rhythm: once per second
const unsigned long TOKEN_LIFE_MS   = 50UL * 60 * 1000; // Firebase tokens last 60 min; renew at 50

// ----- Alert thresholds -----
const int SPO2_LOW = 92;
const int HR_HIGH  = 120;
const int HR_LOW   = 50;

// ----- Display -----
Adafruit_SSD1306 display(128, 64, &Wire, -1);
const int WAVE_TOP = 48, WAVE_H = 15;   // waveform area: rows 48-63
uint8_t wave[128];
float phase = 0;
unsigned long lastVitals = 0, lastFrame = 0, lastBlink = 0;
bool inverted = false;
const char* prevAlert = nullptr;

// ----- Shared between the two cores -----
volatile int bpm = 72, spo2 = 98;
volatile bool cloudOK = false;
const char* volatile alertMsg = nullptr;
volatile unsigned long lastUploadMs = 0;
TaskHandle_t cloudHandle = nullptr;

// ----- Device login (Firebase account for this ESP32) -----
String idToken;
unsigned long tokenTime = 0;

// One simulated heartbeat (p = 0..1): main peak + small "dicrotic" bump
float ppg(float p) {
  float peak = exp(-pow((p - 0.15) / 0.06, 2));
  float bump = 0.35 * exp(-pow((p - 0.40) / 0.07, 2));
  return peak + bump;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect failed, will retry");
  }
}

// Pull a "key":"value" string out of a JSON reply (enough for Firebase's sign-in response)
String jsonString(const String& body, const char* key) {
  int i = body.indexOf(String("\"") + key + "\"");
  if (i < 0) return "";
  i = body.indexOf(':', i);
  i = body.indexOf('"', i);
  if (i < 0) return "";
  int j = body.indexOf('"', i + 1);
  return body.substring(i + 1, j);
}

// Log the ESP32 into its own Firebase account and get a fresh access token
bool signInDevice() {
  WiFiClientSecure c;
  c.setInsecure();   // prototype only
  HTTPClient h;
  h.begin(c, String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") + FIREBASE_API_KEY);
  h.addHeader("Content-Type", "application/json");
  String body = String("{\"email\":\"") + DEVICE_EMAIL +
                "\",\"password\":\"" + DEVICE_PASS +
                "\",\"returnSecureToken\":true}";
  int code = h.POST(body);
  String resp = h.getString();
  h.end();

  if (code == 200) {
    idToken = jsonString(resp, "idToken");
    tokenTime = millis();
    Serial.println("Device signed in to Firebase");
    return idToken.length() > 0;
  }
  Serial.printf("Device sign-in failed: HTTP %d\n%s\n", code, resp.c_str());
  idToken = "";
  return false;
}

// ===== Core 0: uploads readings to Firebase =====
void cloudTask(void* param) {
  WiFiClientSecure client;
  client.setInsecure();     // prototype only: skips HTTPS certificate check
  HTTPClient http;
  http.setReuse(true);      // keep the secure connection open between uploads (much faster)
  String baseUrl = String(DB_URL) + "/devices/" + DEVICE_ID + "/live.json?auth=";

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      cloudOK = false;
      connectWiFi();
    }

    // Log in (or renew the token) before uploading
    if (WiFi.status() == WL_CONNECTED &&
        (idToken.length() == 0 || millis() - tokenTime > TOKEN_LIFE_MS)) {
      if (!signInDevice()) {
        cloudOK = false;
        vTaskDelay(pdMS_TO_TICKS(5000));   // wait, then try again
        continue;
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      const char* a = alertMsg;
      String alertJson = a ? "\"" + String(a) + "\"" : String("null");
      String json = "{\"bpm\":" + String(bpm) +
                    ",\"spo2\":" + String(spo2) +
                    ",\"alert\":" + alertJson +
                    ",\"simulated\":true" +
                    ",\"rssi\":" + String(WiFi.RSSI()) +
                    ",\"uptime\":" + String(millis() / 1000) +
                    ",\"fw\":\"" + FW_VERSION + "\"" +
                    ",\"uploadMs\":" + String(lastUploadMs) +
                    ",\"updatedAt\":{\".sv\":\"timestamp\"}}";

      unsigned long t0 = millis();
      http.begin(client, baseUrl + idToken);
      http.addHeader("Content-Type", "application/json");
      int code = http.PUT(json);
      http.end();
      lastUploadMs = millis() - t0;

      cloudOK = (code == 200);
      if (code == 401) idToken = "";   // token rejected: log in again next loop
      Serial.printf("Upload: BPM %d, SpO2 %d, %lu ms -> HTTP %d\n", bpm, spo2, lastUploadMs, code);
    }

    // Wait for the next upload, but wake up immediately if an alert starts or clears
    unsigned long waitMs = UPLOAD_EVERY_MS > lastUploadMs ? UPLOAD_EVERY_MS - lastUploadMs : 0;
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(waitMs));
  }
}

// ===== Core 1: screen, vitals, alerts =====
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  Wire.setClock(400000);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  for (int i = 0; i < 128; i++) wave[i] = WAVE_TOP + WAVE_H;
  lastFrame = millis();

  // Start the cloud uploader on core 0
  xTaskCreatePinnedToCore(cloudTask, "cloud", 12288, NULL, 1, &cloudHandle, 0);
}

void loop() {
  // ----- Advance the waveform -----
  unsigned long now = millis();
  float dt = (now - lastFrame) / 1000.0;
  lastFrame = now;
  phase += (bpm / 60.0) * dt;
  if (phase >= 1) phase -= 1;

  int y = WAVE_TOP + WAVE_H - (int)(ppg(phase) / 1.1 * WAVE_H);
  memmove(wave, wave + 1, 127);
  wave[127] = y;

  // ----- Simulated vitals (new values once per second) -----
  if (now - lastVitals > 1000) {
    lastVitals = now;
    bpm = 70 + random(0, 7);
    int cycleSec = (now / 1000) % 20;                // every 20 s,
    if (cycleSec >= 14) spo2 = 88 + random(0, 3);    // dip for 6 s (alert test)
    else                spo2 = 97 + random(0, 3);
  }

  // ----- Alerts -----
  const char* msg = nullptr;
  if (spo2 < SPO2_LOW)    msg = "LOW SpO2!";
  else if (bpm > HR_HIGH) msg = "HIGH HR!";
  else if (bpm < HR_LOW)  msg = "LOW HR!";
  alertMsg = msg;

  // Alert started or cleared: tell the cloud task to upload right now
  if (msg != prevAlert) {
    prevAlert = msg;
    if (cloudHandle) xTaskNotifyGive(cloudHandle);
  }

  if (msg) {
    if (now - lastBlink > 500) {
      lastBlink = now;
      inverted = !inverted;
      display.invertDisplay(inverted);
    }
  } else if (inverted) {
    inverted = false;
    display.invertDisplay(false);
  }

  // ----- Draw the screen -----
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Header (yellow band)
  display.setTextSize(1);
  display.setCursor(0, 4);    display.print(msg ? msg : "PulseNode");
  display.setCursor(88, 4);   display.print(cloudOK ? "UP" : "--");
  display.setCursor(110, 4);  display.print("SIM");

  // Labels and numbers (blue area)
  display.setCursor(0, 20);   display.print("BPM");
  display.setCursor(68, 20);  display.print("SpO2 %");
  display.setTextSize(2);
  display.setCursor(0, 30);   display.print(bpm);
  display.setCursor(68, 30);  display.print(spo2);

  // Scrolling pulse waveform
  for (int x = 1; x < 128; x++) {
    display.drawLine(x - 1, wave[x - 1], x, wave[x], SSD1306_WHITE);
  }

  display.display();
}
