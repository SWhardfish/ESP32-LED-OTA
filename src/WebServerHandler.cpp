#include "WebServerHandler.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Update.h>
#include "LEDController.h"

// Declare extern variables
extern LightModeConfig currentMode;
extern WebServer server;

void setupWebServer() {
  // Setup all route handlers
  server.on("/", handleRoot);
  server.on("/config", HTTP_GET, handleConfig);
  server.on("/config", HTTP_POST, handleSaveConfig);
  server.on("/color", HTTP_POST, handleColor);
  server.on("/white", HTTP_POST, handleWhite);
  server.on("/brightness", HTTP_POST, handleBrightness);
  server.on("/state", HTTP_GET, handleState);
  server.on("/log", HTTP_GET, handleLog);
  server.on("/upload", HTTP_POST, [](){}, handleUpload);

  server.begin();
  logEvent("Web server started");
}

void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LED Controller</title>
    <link href="https://cdn.jsdelivr.net/npm/tailwindcss@2.2.19/dist/tailwind.min.css" rel="stylesheet">
    <script src="https://cdn.jsdelivr.net/npm/@simonwep/pickr/dist/pickr.min.js"></script>
    <style>
      .mode-panel {
        border: 1px solid #e2e8f0;
        border-radius: 0.5rem;
        padding: 1rem;
        margin-bottom: 1.5rem;
        background-color: #f8fafc;
      }
      .color-preview {
        width: 50px;
        height: 50px;
        border-radius: 0.25rem;
        display: inline-block;
        vertical-align: middle;
        margin-right: 10px;
      }
    </style>
  </head>
  <body class="bg-gray-50">
    <div class="container mx-auto px-4 py-8">
      <h1 class="text-3xl font-bold text-center mb-8">LED Controller</h1>

      <div class="mode-panel">
        <h2 class="text-xl font-semibold mb-4">Current Mode: <span id="currentMode" class="text-blue-600">Loading...</span></h2>
        <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
          <div class="bg-white p-4 rounded-lg shadow">
            <h3 class="font-medium">Brightness</h3>
            <p id="currentBrightness" class="text-2xl">-</p>
          </div>
          <div class="bg-white p-4 rounded-lg shadow">
            <h3 class="font-medium">Color</h3>
            <div id="currentColor" class="flex items-center">
              <div class="color-preview"></div>
              <span>#FFFFFF</span>
            </div>
          </div>
          <div class="bg-white p-4 rounded-lg shadow">
            <h3 class="font-medium">White</h3>
            <p id="currentWhite" class="text-2xl">-</p>
          </div>
        </div>
      </div>

      <div class="flex justify-center space-x-4 mb-8">
        <button id="highModeBtn" class="px-6 py-3 bg-green-500 text-white rounded-lg shadow hover:bg-green-600 transition">
          Activate High Mode
        </button>
        <button id="lowModeBtn" class="px-6 py-3 bg-blue-500 text-white rounded-lg shadow hover:bg-blue-600 transition">
          Activate Low Mode
        </button>
        <button id="offBtn" class="px-6 py-3 bg-red-500 text-white rounded-lg shadow hover:bg-red-600 transition">
          Turn Off
        </button>
      </div>

      <div class="flex justify-center space-x-4">
        <a href="/config" class="px-6 py-3 bg-yellow-500 text-white rounded-lg shadow hover:bg-yellow-600 transition">
          Configuration
        </a>
        <a href="/log" class="px-6 py-3 bg-purple-500 text-white rounded-lg shadow hover:bg-purple-600 transition">
          Event Log
        </a>
      </div>
    </div>

    <script>
    document.addEventListener('DOMContentLoaded', function() {
      const pickr = Pickr.create({
        el: document.createElement('div'),
        theme: 'classic',
        default: '#ffffff',
        components: {
          preview: false,
          opacity: false,
          hue: true,
          interaction: {
            hex: true,
            rgba: true,
            input: true,
            save: true
          }
        },
        strings: {
          save: 'Apply'
        }
      });

      // Fetch current state
      function updateState() {
        fetch('/state')
          .then(response => response.json())
          .then(data => {
            document.getElementById('currentMode').textContent =
              data.mode === 'high' ? 'High' : data.mode === 'low' ? 'Low' : 'Off';

            document.getElementById('currentBrightness').textContent =
              data.brightness + '%';

            if (data.color) {
              const colorPreview = document.querySelector('#currentColor .color-preview');
              colorPreview.style.backgroundColor = data.color;
              document.querySelector('#currentColor span').textContent = data.color;
            }

            document.getElementById('currentWhite').textContent =
              data.white + '%';
          });
      }

      // Set up button handlers
      document.getElementById('highModeBtn').addEventListener('click', () => {
        fetch('/mode', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ mode: 'high' })
        }).then(updateState);
      });

      document.getElementById('lowModeBtn').addEventListener('click', () => {
        fetch('/mode', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ mode: 'low' })
        }).then(updateState);
      });

      document.getElementById('offBtn').addEventListener('click', () => {
        fetch('/mode', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ mode: 'off' })
        }).then(updateState);
      });

      // Initial update
      updateState();
      setInterval(updateState, 5000);
    });
    </script>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleConfig() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configuration</title>
    <link href="https://cdn.jsdelivr.net/npm/tailwindcss@2.2.19/dist/tailwind.min.css" rel="stylesheet">
  </head>
  <body class="bg-gray-50">
    <div class="container mx-auto px-4 py-8">
      <h1 class="text-3xl font-bold text-center mb-8">Configuration</h1>

      <form method="POST" action="/config" class="max-w-3xl mx-auto bg-white p-6 rounded-lg shadow">
        <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
          <!-- High Mode Settings -->
          <div class="border border-gray-200 p-4 rounded-lg">
            <h2 class="text-xl font-semibold mb-4 text-center">High Mode Settings</h2>

            <div class="mb-4">
              <label class="block text-gray-700 mb-2">Brightness</label>
              <input type="range" name="highBrightness" min="0" max="255" value=")rawliteral" +
              String(scheduleConfig.highMode.brightness) + R"rawliteral(" class="w-full">
              <div class="text-right">)rawliteral" + String(scheduleConfig.highMode.brightness) + R"rawliteral(</div>
            </div>

            <div class="mb-4">
              <label class="block text-gray-700 mb-2">Color</label>
              <input type="color" name="highColor" value="#)rawliteral" +
              String(scheduleConfig.highMode.color, HEX) + R"rawliteral(" class="w-full p-1 border rounded">
            </div>

            <div class="mb-4">
              <label class="flex items-center">
                <input type="checkbox" name="highColorEnabled" class="mr-2" )rawliteral" +
                (scheduleConfig.highMode.colorEnabled ? "checked" : "") + R"rawliteral(>
                <span>Enable Color</span>
              </label>
            </div>

            <div class="mb-4">
              <label class="block text-gray-700 mb-2">White Level</label>
              <input type="range" name="highWhite" min="0" max="255" value=")rawliteral" +
              String(scheduleConfig.highMode.white) + R"rawliteral(" class="w-full">
              <div class="text-right">)rawliteral" + String(scheduleConfig.highMode.white) + R"rawliteral(</div>
            </div>

            <div class="mb-4">
              <label class="flex items-center">
                <input type="checkbox" name="highWhiteEnabled" class="mr-2" )rawliteral" +
                (scheduleConfig.highMode.whiteEnabled ? "checked" : "") + R"rawliteral(>
                <span>Enable White</span>
              </label>
            </div>
          </div>

          <!-- Low Mode Settings -->
          <div class="border border-gray-200 p-4 rounded-lg">
            <h2 class="text-xl font-semibold mb-4 text-center">Low Mode Settings</h2>

            <div class="mb-4">
              <label class="block text-gray-700 mb-2">Brightness</label>
              <input type="range" name="lowBrightness" min="0" max="255" value=")rawliteral" +
              String(scheduleConfig.lowMode.brightness) + R"rawliteral(" class="w-full">
              <div class="text-right">)rawliteral" + String(scheduleConfig.lowMode.brightness) + R"rawliteral(</div>
            </div>

            <div class="mb-4">
              <label class="block text-gray-700 mb-2">Color</label>
              <input type="color" name="lowColor" value="#)rawliteral" +
              String(scheduleConfig.lowMode.color, HEX) + R"rawliteral(" class="w-full p-1 border rounded">
            </div>

            <div class="mb-4">
              <label class="flex items-center">
                <input type="checkbox" name="lowColorEnabled" class="mr-2" )rawliteral" +
                (scheduleConfig.lowMode.colorEnabled ? "checked" : "") + R"rawliteral(>
                <span>Enable Color</span>
              </label>
            </div>

            <div class="mb-4">
              <label class="block text-gray-700 mb-2">White Level</label>
              <input type="range" name="lowWhite" min="0" max="255" value=")rawliteral" +
              String(scheduleConfig.lowMode.white) + R"rawliteral(" class="w-full">
              <div class="text-right">)rawliteral" + String(scheduleConfig.lowMode.white) + R"rawliteral(</div>
            </div>

            <div class="mb-4">
              <label class="flex items-center">
                <input type="checkbox" name="lowWhiteEnabled" class="mr-2" )rawliteral" +
                (scheduleConfig.lowMode.whiteEnabled ? "checked" : "") + R"rawliteral(>
                <span>Enable White</span>
              </label>
            </div>
          </div>
        </div>

        <!-- Timing Settings -->
        <div class="mt-6 border-t pt-6">
          <h2 class="text-xl font-semibold mb-4 text-center">Timing Settings</h2>

          <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
            <div>
              <label class="block text-gray-700 mb-2">Morning On Time</label>
              <input type="time" name="morningOnTime" class="w-full p-2 border rounded"
                value=")rawliteral" +
                (String(scheduleConfig.morningOnHour).length() < 2 ? "0" + String(scheduleConfig.morningOnHour) : String(scheduleConfig.morningOnHour)) + ":" +
                (String(scheduleConfig.morningOnMinute).length() < 2 ? "0" + String(scheduleConfig.morningOnMinute) : String(scheduleConfig.morningOnMinute)) +
                R"rawliteral(">
            </div>

            <div>
              <label class="block text-gray-700 mb-2">Morning Off Time</label>
              <input type="time" name="morningOffTime" class="w-full p-2 border rounded"
                value=")rawliteral" +
                (String(scheduleConfig.morningOffHour).length() < 2 ? "0" + String(scheduleConfig.morningOffHour) : String(scheduleConfig.morningOffHour)) + ":" +
                (String(scheduleConfig.morningOffMinute).length() < 2 ? "0" + String(scheduleConfig.morningOffMinute) : String(scheduleConfig.morningOffMinute)) +
                R"rawliteral(">
            </div>

            <div>
              <label class="block text-gray-700 mb-2">Sunset Offset (min)</label>
              <input type="number" name="sunsetOffset" class="w-full p-2 border rounded"
                value=")rawliteral" +
                String(scheduleConfig.sunsetOffsetHour * 60 + scheduleConfig.sunsetOffsetMinute) +
                R"rawliteral(" min="-120" max="120" step="15">
            </div>

            <div>
              <label class="block text-gray-700 mb-2">Evening Off Time</label>
              <input type="time" name="eveningOffTime" class="w-full p-2 border rounded"
                value=")rawliteral" +
                (String(scheduleConfig.eveningOffHour).length() < 2 ? "0" + String(scheduleConfig.eveningOffHour) : String(scheduleConfig.eveningOffHour)) + ":" +
                (String(scheduleConfig.eveningOffMinute).length() < 2 ? "0" + String(scheduleConfig.eveningOffMinute) : String(scheduleConfig.eveningOffMinute)) +
                R"rawliteral(">
            </div>

            <div>
              <label class="block text-gray-700 mb-2">Fade In (ms)</label>
              <input type="number" name="fadeDurationOn" class="w-full p-2 border rounded"
                value=")rawliteral" + String(scheduleConfig.fadeDurationOn) + R"rawliteral(">
            </div>

            <div>
              <label class="block text-gray-700 mb-2">Fade Out (ms)</label>
              <input type="number" name="fadeDurationOff" class="w-full p-2 border rounded"
                value=")rawliteral" + String(scheduleConfig.fadeDurationOff) + R"rawliteral(">
            </div>

            <div>
              <label class="block text-gray-700 mb-2">Motion Hold (ms)</label>
              <input type="number" name="holdTime" class="w-full p-2 border rounded"
                value=")rawliteral" + String(scheduleConfig.holdTime) + R"rawliteral(">
            </div>
          </div>
        </div>

        <div class="mt-8 flex justify-center">
          <button type="submit" class="px-6 py-3 bg-green-500 text-white rounded-lg shadow hover:bg-green-600 transition">
            Save Configuration
          </button>
        </div>
      </form>

      <div class="mt-6 text-center">
        <a href="/" class="text-blue-500 hover:underline">Back to Home</a>
      </div>
    </div>

    <script>
    document.addEventListener('DOMContentLoaded', function() {
      // Update range value displays
      document.querySelectorAll('input[type="range"]').forEach(input => {
        const display = input.nextElementSibling;
        display.textContent = input.value;

        input.addEventListener('input', function() {
          display.textContent = this.value;
        });
      });
    });
    </script>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleSaveConfig() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  // Save High Mode settings
  scheduleConfig.highMode.brightness = server.arg("highBrightness").toInt();
  scheduleConfig.highMode.color = strtol(server.arg("highColor").substring(1).c_str(), NULL, 16);
  scheduleConfig.highMode.white = server.arg("highWhite").toInt();
  scheduleConfig.highMode.colorEnabled = server.hasArg("highColorEnabled");
  scheduleConfig.highMode.whiteEnabled = server.hasArg("highWhiteEnabled");

  // Save Low Mode settings
  scheduleConfig.lowMode.brightness = server.arg("lowBrightness").toInt();
  scheduleConfig.lowMode.color = strtol(server.arg("lowColor").substring(1).c_str(), NULL, 16);
  scheduleConfig.lowMode.white = server.arg("lowWhite").toInt();
  scheduleConfig.lowMode.colorEnabled = server.hasArg("lowColorEnabled");
  scheduleConfig.lowMode.whiteEnabled = server.hasArg("lowWhiteEnabled");

  // Parse time settings
  String morningOnStr = server.arg("morningOnTime");
  scheduleConfig.morningOnHour = morningOnStr.substring(0, 2).toInt();
  scheduleConfig.morningOnMinute = morningOnStr.substring(3, 5).toInt();

  String morningOffStr = server.arg("morningOffTime");
  scheduleConfig.morningOffHour = morningOffStr.substring(0, 2).toInt();
  scheduleConfig.morningOffMinute = morningOffStr.substring(3, 5).toInt();

  String eveningOffStr = server.arg("eveningOffTime");
  scheduleConfig.eveningOffHour = eveningOffStr.substring(0, 2).toInt();
  scheduleConfig.eveningOffMinute = eveningOffStr.substring(3, 5).toInt();

  // Parse sunset offset
  int sunsetOffsetMinutes = server.arg("sunsetOffset").toInt();
  scheduleConfig.sunsetOffsetHour = sunsetOffsetMinutes / 60;
  scheduleConfig.sunsetOffsetMinute = sunsetOffsetMinutes % 60;

  // Parse timing parameters
  scheduleConfig.fadeDurationOn = server.arg("fadeDurationOn").toInt();
  scheduleConfig.fadeDurationOff = server.arg("fadeDurationOff").toInt();
  scheduleConfig.holdTime = server.arg("holdTime").toInt();

  // Save configuration
  saveConfig();

  server.send(200, "text/plain", "Configuration saved. Rebooting...");
  delay(1000);
  ESP.restart();
}

void handleColor() {
  if (server.hasArg("plain") == false) {
    server.send(400, "text/plain", "Body not received");
    return;
  }

  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    server.send(400, "text/plain", "Failed to parse request");
    return;
  }

  String color = doc["color"].as<String>();
  uint32_t rgb = strtol(color.substring(1).c_str(), NULL, 16);

  LightModeConfig tempMode = currentMode;
  tempMode.color = rgb;
  tempMode.colorEnabled = true;
  setLEDs(tempMode);

  server.send(200, "text/plain", "Color set to " + color);
}

void handleWhite() {
  LightModeConfig tempMode = currentMode;
  tempMode.white = 255;
  tempMode.whiteEnabled = true;
  setLEDs(tempMode);

  server.send(200, "text/plain", "White LEDs activated");
}

void handleBrightness() {
  if (server.hasArg("plain") == false) {
    server.send(400, "text/plain", "Body not received");
    return;
  }

  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    server.send(400, "text/plain", "Failed to parse request");
    return;
  }

  int brightness = doc["brightness"];
  LightModeConfig tempMode = currentMode;
  tempMode.brightness = map(brightness, 0, 100, 0, 255);
  setLEDs(tempMode);

  server.send(200, "text/plain", "Brightness set to " + String(brightness));
}

void handleState() {
  DynamicJsonDocument doc(256);

  if (currentMode.brightness == 0) {
    doc["mode"] = "off";
  } else if (currentMode.brightness == scheduleConfig.highMode.brightness &&
             currentMode.color == scheduleConfig.highMode.color &&
             currentMode.white == scheduleConfig.highMode.white) {
    doc["mode"] = "high";
  } else {
    doc["mode"] = "low";
  }

  doc["brightness"] = map(currentMode.brightness, 0, 255, 0, 100);
  doc["color"] = "#" + String(currentMode.color, HEX);
  doc["white"] = map(currentMode.white, 0, 255, 0, 100);

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleLog() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Event Log</title>
    <link href="https://cdn.jsdelivr.net/npm/tailwindcss@2.2.19/dist/tailwind.min.css" rel="stylesheet">
  </head>
  <body class="bg-gray-50">
    <div class="container mx-auto px-4 py-8">
      <h1 class="text-3xl font-bold text-center mb-8">Event Log</h1>

      <div class="bg-white rounded-lg shadow overflow-hidden">
        <ul class="divide-y divide-gray-200">
  )rawliteral";

  for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
    int index = (logIndex - 1 - i + LOG_BUFFER_SIZE) % LOG_BUFFER_SIZE;
    if (logBuffer[index].length()) {
      html += "<li class=\"p-4 hover:bg-gray-50\">" + logBuffer[index] + "</li>";
    }
  }

  html += R"rawliteral(
        </ul>
      </div>

      <div class="mt-6 text-center">
        <a href="/" class="text-blue-500 hover:underline">Back to Home</a>
      </div>
    </div>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleUpload() {
  HTTPUpload& upload = server.upload();
  static File updateFile;

  if (upload.status == UPLOAD_FILE_START) {
    logEvent("Firmware upload started: " + upload.filename);
    updateFile = LittleFS.open("/update.bin", "w");
    if (!updateFile) {
      logEvent("Failed to create update file");
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (updateFile) {
      updateFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (updateFile) {
      updateFile.close();
      logEvent("Firmware upload complete: " + String(upload.totalSize) + " bytes");

      server.send(200, "text/html",
        "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Upload Complete</title></head>"
        "<body><h1>Upload Complete</h1><p>Firmware update will be applied on reboot.</p>"
        "<p><a href=\"/\">Return to home</a></p></body></html>");

      delay(1000);
      ESP.restart();
    } else {
      server.send(500, "text/plain", "Update failed");
    }
  }
}