#pragma once

#include <WiFi.h>
#include <DNSServer.h>
#include "config.h"

extern DNSServer dnsServer;

bool loadWiFiConfig();
bool connectWiFi();
void startConfigPortal();
bool saveConfig();