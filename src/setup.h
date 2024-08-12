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
#include "ESP_Utils.h"
#include "secrets.h"
#include "webserver.h"
#include "SensorManager.h"

#define LCD_UPDATE_INTERVAL 5000    // 5 seconds
#define SENSOR_PUBLISH_INTERVAL 60000    // 1 minute
#define WATERING_CHECK_INTERVAL 300000   // 5 minutes
#define SDA_PIN 21
#define SCL_PIN 22
#define RELAY_COUNT 4

// Add the SensorConfig struct definition
struct SensorConfig {
    float threshold = 0;
    unsigned long activationPeriod = 0;
    unsigned long lastWateringTime = 0;
};

constexpr size_t LOGGER_BUFFER_SIZE = 100;

extern Adafruit_BMP085 bmp;
extern LiquidCrystal_I2C lcd;
extern Preferences preferences;
extern WiFiClientSecure espClient;
extern PubSubClient mqttClient;
extern AsyncWebServer server;

extern ESPLogger<LOGGER_BUFFER_SIZE> logger;
extern ESPTelemetry<LOGGER_BUFFER_SIZE> espTelemetry;
extern ESPTimeSetup<LOGGER_BUFFER_SIZE> timeSetup;
extern ESP32WebServer<LOGGER_BUFFER_SIZE> webServer;
extern SemaphoreHandle_t relaySemaphore;
extern ESPMQTTManager<LOGGER_BUFFER_SIZE> mqttManager;

extern const std::array<int, 4> MOISTURE_SENSOR_PINS;
extern const int FLOAT_SWITCH_PIN;  
extern const int RELAY_PINS[RELAY_COUNT];
extern bool relayStates[RELAY_COUNT];
extern unsigned long relayTimers[RELAY_COUNT];

// New variable to store last heap monitoring time
extern const unsigned long HEAP_MONITOR_INTERVAL;
extern unsigned long lastHeapMonitorTime;
extern unsigned long lastLCDUpdateTime;
extern unsigned long lastSensorPublishTime;
extern unsigned long lastWateringCheckTime;

// Configuration variables
extern float seaLevelPressure; 
extern float temperatureOffset;   
extern SensorManager sensorManager;
extern SensorConfig sensorConfigs[4];

// Declare functions
void setup_wifi();
void setupRelays();
void setupLittleFS();
void setupSensors();
void setupPreferences();
void setupMoistureSensors();
void setupFloatSwitch();

#endif // SETUP_H