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

#define ENABLE_SERIAL_PRINT // Enable serial printing

const ESPMQTTManager::Config mqttConfig = {
    .server = MQTT_SERVER,
    .port = MQTT_PORT,
    .username = MQTT_USERNAME,
    .password = MQTT_PASSWORD,
    .rootCA = root_ca,
    .clientCert = client_cert,
    .clientKey = client_key,
    .clientID = "MyESP32Client"
};

Logger& logger = Logger::instance();
ESPMQTTManager mqttManager(mqttConfig);
ESPTelemetry espTelemetry(mqttManager, "esp32/telemetry");
ESPTimeSetup timeSetup("pool.ntp.org", 0, 3600);

// Initialize devices
Adafruit_BMP085 bmp;
LiquidCrystal_I2C lcd(0x27, 16, 2);
//Preferences preferences;
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
ConfigManager* configManager = nullptr;
SensorManager* sensorManager = nullptr;
RelayManager* relayManager = nullptr;
LCDManager* lcdManager = nullptr;
SensorPublishTask* publishTask = nullptr;
ESP32WebServer* webServer = nullptr;
//ESP32WebServer* webServerPtr = nullptr;  // Declare the pointer at global scope

void setup_wifi() {
  logger.log("Main", LogLevel::INFO, "Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    logger.log("Main", LogLevel::INFO, "Waiting for connection...");
  }
}
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
    setup_wifi();
    delay(100);
    configManager = new ConfigManager();
    configManager->begin("cfg");
    configManager->initializeConfigurations();
    delay(200);
    // Initialize other managers
    sensorManager = new SensorManager(*configManager);
    relayManager = new RelayManager(*configManager, *sensorManager);
    lcdManager = new LCDManager(lcd, *sensorManager, *configManager);
    webServer = new ESP32WebServer(80, *relayManager, *sensorManager, *configManager);
    // Initialize the pointer
    //webServerPtr = &webServer;  
    // Set up the logger observer
    logger.addLogObserver([](std::string_view tag, Logger::Level level, std::string_view message) {
        if (webServer) {  // Check if the pointer is valid
            webServer->handleLogMessage(tag, level, message);
        } 
    });   
    setupOTA();
    timeSetup.setup();
    delay(500);
    mqttManager.setup();
    delay(1000);
    setupLittleFS();
    relayManager->init();
    sensorManager->setupFloatSwitch();
    sensorManager->setupSensors();
    sensorManager->startSensorTask();
    webServer->begin();
    //lcdManager.start();
    delay(1000);
    publishTask = new SensorPublishTask(*sensorManager, mqttManager, *configManager);
    logger.log("Main", LogLevel::INFO, "Setup complete");   
}

void loop() {
  ArduinoOTA.handle();
  mqttManager.loop();
  delay(100);
}

