#pragma once
// ESP32-2432S028R ("Cheap Yellow Display") pin map and app constants.
//
//  Display  ILI9341/ST7789  HSPI   SCLK14 MOSI13 MISO12 CS15 DC2 BL21  (set in platformio.ini)
//  Touch    XPT2046         bit-banged: CLK25 MOSI32 MISO39 CS33 IRQ36
//  microSD  TF slot         VSPI   SCK18 MISO19 MOSI23 CS5
//
// Touch is bit-banged so it never has to share/re-pin the VSPI bus used by the SD card.

#define TOUCH_CLK 25
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CS 33
#define TOUCH_IRQ 36

#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5


// Portrait. 0 = USB connector at the bottom on most boards; use 2 to flip.
#ifndef DISPLAY_ROTATION
#define DISPLAY_ROTATION 0
#endif

#define SCREEN_W 240
#define SCREEN_H 320

// Storage layout on the SD card
#define CAL_ROOT "/calendar"
#define CATALOG_PATH "/calendar/catalog.json"

#ifndef CATALOG_URL_DEFAULT
#define CATALOG_URL_DEFAULT "https://raw.githubusercontent.com/rybbit/cyd-pinup-calendar/main/content/catalog.json"
#endif
#ifndef TZ_DEFAULT
#define TZ_DEFAULT "EST5EDT,M3.2.0,M11.1.0"
#endif

#define WIFI_CONF_PATH "/config/wifi.conf"
#define MAX_IMAGE_BYTES (120 * 1024)
