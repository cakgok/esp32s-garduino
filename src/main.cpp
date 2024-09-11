#define ENABLE_SERIAL_PRINT

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
#include "RelayManager.h"
#include "globals.h"
#include "PreferencesHandler.h"

const ESPMQTTManager::Config mqttConfig = {
    .server = MQTT_SERVER,
    .port = MQTT_PORT,
    .username = MQTT_USERNAME,
    .password = MQTT_PASSWORD,
    .rootCA = root_ca,
    .clientCert = client_cert,
    .clientKey = client_key,
    .clientID = "plant-friend"
};

Logger& logger = Logger::instance();
WiFiWrapper wifi(WIFI_SSID, WIFI_PASSWORD);
ESPMQTTManager mqttManager(mqttConfig);
ESPTelemetry espTelemetry(mqttManager, "plant-friend/telemetry");
ESPTimeSetup timeSetup("pool.ntp.org", 0, 3600);
OTAManager otaManager;

Adafruit_BMP085 bmp;
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
PreferencesHandler prefsHandler;
ConfigManager* configManager = nullptr;
SensorManager* sensorManager = nullptr;
RelayManager* relayManager = nullptr;
LCDManager* lcdManager = nullptr;
PublishManager* publishManager = nullptr;
ESP32WebServer* webServer = nullptr;

void setupLittleFS() {
  if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
      logger.log("Main", LogLevel::ERROR, "LittleFS Mount Failed");
      return;
  }
  logger.log("Main",LogLevel::INFO, "LittleFS mounted successfully");
}

void setup() {
  Serial.begin(115200);
  logger.setFilterLevel(Logger::Level::DEBUG);
  wifi.setHostname("plant-friend");
  wifi.begin();
  wifi.setupMDNS("plant-friend");

  configManager = new ConfigManager(prefsHandler);
  configManager->begin("cfg");
  configManager->initializeConfigurations();

  sensorManager = new SensorManager(*configManager);
  relayManager = new RelayManager(*configManager, *sensorManager);
  lcdManager = new LCDManager(lcd, *sensorManager, *configManager);
  webServer = new ESP32WebServer(80, *relayManager, *sensorManager, *configManager);

  otaManager.begin();
  timeSetup.begin();
  mqttManager.begin();

  setupLittleFS();

  relayManager->init();
  sensorManager->setupFloatSwitch();
  sensorManager->setupSensors();
  sensorManager->startSensorTask();
  webServer->begin();
  lcdManager->start();
  
  publishManager = new PublishManager(*sensorManager, mqttManager, *configManager);
  publishManager->start();

  // 
  espTelemetry.addCustomData("publishManager_telemetry_stack_hwm", []() -> UBaseType_t {
        return uxTaskGetStackHighWaterMark(publishManager->getTelemetryTaskHandle());
  });

  espTelemetry.addCustomData("publishManager_stack_hwm", []() -> UBaseType_t {
        return uxTaskGetStackHighWaterMark(publishManager->getSensorTaskHandle());
  });

  espTelemetry.addCustomData("sensor_task_stack_hwm", []() -> UBaseType_t {
        return uxTaskGetStackHighWaterMark(sensorManager->getTaskHandle());
  });


  logger.log("Main", LogLevel::INFO, "Setup complete");   
}

void loop() {}

