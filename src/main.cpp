#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

// ---------- CONFIG ----------
constexpr uint8_t LED_PIN      = 21;    // onboard WS2812
constexpr uint8_t LED_BRIGHT   = 20;    // ~8% of 255 to reduce current
constexpr char    AP_SSID[]    = "ESP32S3_Setup";
constexpr unsigned CONNECT_MS  = 10000; // 10 s try before AP fallback
// ----------------------------

// one pixel WS2812
Adafruit_NeoPixel pixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);

bool flashingRed = false;
bool ledOn       = false;
unsigned long lastToggle = 0;

void setPixel(uint8_t r, uint8_t g, uint8_t b) {
    flashingRed = false;
    pixel.setPixelColor(0, pixel.Color(r, g, b));
    pixel.show();
}

void flashRed() { flashingRed = true; }

void ledLoop() {
    if (!flashingRed) return;
    unsigned long now = millis();
    if (now - lastToggle >= 1000) {
        lastToggle = now;
        ledOn = !ledOn;
        pixel.setPixelColor(0, ledOn ? pixel.Color(255,0,0) : 0);
        pixel.show();
    }
}

// ---------- WiFi helpers ----------
bool connectStoredWiFi() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        return false;
    }
    if (!LittleFS.exists("/wifi.json")) return false;

    File f = LittleFS.open("/wifi.json", "r");
    if (!f) return false;
    DynamicJsonDocument doc(128);
    if (deserializeJson(doc, f)) return false;
    String ssid = doc["ssid"].as<String>();
    String pass = doc["pass"].as<String>();
    f.close();

    if (!ssid.length()) return false;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < CONNECT_MS) {
        delay(250);
    }
    return WiFi.status() == WL_CONNECTED;
}

void handleRoot() {
    String html =
        "<form action='/save' method='POST'>"
        "SSID:<input name='ssid'><br>"
        "Password:<input name='pass' type='password'><br>"
        "<input type='submit'></form>";
    server.send(200, "text/html", html);
}

void handleSave() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    DynamicJsonDocument doc(128);
    doc["ssid"] = ssid;
    doc["pass"] = pass;

    File f = LittleFS.open("/wifi.json", "w");
    serializeJson(doc, f);
    f.close();

    server.send(200, "text/plain", "Saved. Rebooting…");
    delay(1000);
    ESP.restart();
}

void startAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
    Serial.printf("AP started. Connect to SSID \"%s\"\n", AP_SSID);
}
// -----------------------------------

void setup() {
    Serial.begin(115200);
    delay(500); // let power settle

    pixel.begin();
    pixel.setBrightness(LED_BRIGHT);
    pixel.show();

    if (connectStoredWiFi()) {
        Serial.printf("Connected to %s, IP: %s\n",
                      WiFi.SSID().c_str(),
                      WiFi.localIP().toString().c_str());
        setPixel(0,255,0);  // steady green
    } else {
        Serial.println("WiFi not configured or failed. Starting AP mode.");
        flashRed();
        startAP();
    }
}

void loop() {
    if (WiFi.getMode() & WIFI_AP) {
        server.handleClient();
    }
    ledLoop();
}
