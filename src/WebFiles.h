#pragma once
#include <Arduino.h>

// Minimal helper to return the contents of index.html that will be used
// when not using LittleFS (you can substitute by serving the file from LittleFS)
namespace WebFiles {
  String index_html();
}
