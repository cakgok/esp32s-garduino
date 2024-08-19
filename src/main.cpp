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
Preferences preferences;
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
ConfigManager configManager;
SensorManager sensorManager(configManager);
RelayManager relayManager(configManager, sensorManager);
LCDManager lcdManager(lcd, sensorManager, configManager);
SensorPublishTask publishTask(sensorManager, mqttManager, configManager);
ESP32WebServer webServer(80, relayManager, sensorManager, configManager);
ESP32WebServer* webServerPtr = nullptr;  // Declare the pointer at global scope

// Initialize config
ConfigManager::HardwareConfig hwConfig;
ConfigManager::SensorConfig sensorConfigs[ConfigConstants::RELAY_COUNT];
//ConfigManager::RelayConfig relayConfigs[ConfigConstants::RELAY_COUNT];
//ConfigManager::Config config;


void setup_wifi() {
  logger.log("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    logger.log(".");
  }
}
void setupLittleFS() {
    if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
        logger.log("LittleFS Mount Failed");
        return;
    }
    logger.log("LittleFS mounted successfully");
}
void setupI2C() {
    Wire.begin(hwConfig.sdaPin, hwConfig.sclPin);
    lcd.init();
    lcd.backlight();
}
void setupRelayPins() {
    for (int i = 0; i < ConfigConstants::RELAY_COUNT; i++) {
        pinMode(hwConfig.relayPins[i], INPUT);
        digitalWrite(hwConfig.relayPins[i], HIGH);
        pinMode(hwConfig.relayPins[i], OUTPUT);
    }
}

void setup() {
    Serial.begin(115200);                 
    logger.setFilterLevel(Logger::Level::INFO);
    setup_wifi();
    setupI2C();
    delay(100);
    configManager.begin("irrigation-config");
    // Initialize the pointer
    webServerPtr = &webServer;  
    // Set up the logger observer
    logger.addLogObserver([](std::string_view tag, Logger::Level level, std::string_view message) {
        if (webServerPtr) {  // Check if the pointer is valid
            webServerPtr->handleLogMessage(tag, level, message);
        } 
    });   
    setupOTA();
    timeSetup.setup();
    delay(500);
    mqttManager.setup();
    delay(100);
    setupLittleFS();
    setupRelayPins();
    sensorManager.setupFloatSwitch();
    sensorManager.setupSensors();
    sensorManager.startSensorTask();
    webServer.begin();
    lcdManager.start();
    delay(1000);
    publishTask.start();
    logger.log("Setup complete");   
}

void loop() {
  ArduinoOTA.handle();
  mqttManager.loop();
  delay(100);
}

