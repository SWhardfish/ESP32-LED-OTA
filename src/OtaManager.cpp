#include "OtaManager.h"
#include <ArduinoOTA.h>
#include <Update.h>
#include <FS.h>
#include <LittleFS.h>

OtaManager::OtaManager(WebServer* srv) : _server(srv) {}

void OtaManager::begin() {
  // init ArduinoOTA
  setupArduinoOta();
  // mount LittleFS for web files
  if(!LittleFS.begin()){
    Serial.println("LittleFS mount failed");
  }
  // web update endpoint
  setupWebUpdate();
}

void OtaManager::setupArduinoOta() {
  ArduinoOTA.onStart([]() {
    Serial.println("ArduinoOTA start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nArduinoOTA end");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("OTA Error[%u]\n", err);
  });
  ArduinoOTA.begin();
}

void OtaManager::setupWebUpdate() {
  // returns upload form or can be integrated into index.html
  _server->on("/update", HTTP_POST, [this]() {
    _server->sendHeader("Connection", "close");
    _server->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    // restart handled by uploader
  }, [this]() { // file upload handler (onUpload)
    HTTPUpload& upload = _server->upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
}

void OtaManager::handle() {
  ArduinoOTA.handle();
}
