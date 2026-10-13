#pragma once

// Optional local overrides; excluded from git to keep STA credentials local.
#if __has_include("wifi_config.local.h")
#include "wifi_config.local.h"
#endif
#ifndef LED_STA_SSID
#define LED_STA_SSID ""
#endif
#ifndef LED_STA_PASSWORD
#define LED_STA_PASSWORD ""
#endif
#ifndef LED_AP_SSID
#define LED_AP_SSID "ESP32S3-LED-Controller"
#endif
#ifndef LED_AP_PASSWORD
#define LED_AP_PASSWORD "ledpanel123"
#endif
