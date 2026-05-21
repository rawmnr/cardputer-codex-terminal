#include "M5Cardputer.h"

M5CardputerClass M5Cardputer;
M5CardputerClass M5;
WiFiClass WiFi;
SerialMock Serial;
SDClass SD;
MDNSClass MDNS;
SPIClass SPI;

wl_status_t WiFiClass::status() { return WL_CONNECTED; }
