#ifndef SETUP_H
#define SETUP_H

#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <WiFi.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "FS.h"
#include "LittleFS.h"
#include "ESPUtils.h"
#include "secrets.h"
#include "webserver.h"
#include "SensorManager.h"
#include "LCDManager.h"
#include "PublishManager.h"

#define SDA_PIN 21
#define SCL_PIN 22
#define RELAY_COUNT 4

extern Adafruit_BMP085 bmp;
extern LiquidCrystal_I2C lcd;
extern Preferences preferences;
extern WiFiClientSecure espClient;
extern PubSubClient mqttClient;
extern AsyncWebServer server;

extern ESPLogger logger;
extern ESPTelemetry espTelemetry;
extern ESPTimeSetup timeSetup;
extern ESP32WebServer webServer;
extern ESPMQTTManager mqttManager;

extern const std::array<int, 4> MOISTURE_SENSOR_PINS;
extern const int FLOAT_SWITCH_PIN;  
extern const int RELAY_PINS[RELAY_COUNT];
extern bool relayStates[RELAY_COUNT];

extern SensorManager sensorManager;
extern LCDManager lcdManager;
extern SensorPublishTask publishTask;

// Declare functions
void setup_wifi();
void setupLittleFS();
void setupFloatSwitch();

#endif // SETUP_H