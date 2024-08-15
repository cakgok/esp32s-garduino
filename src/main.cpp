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
//#define DISABLE_SERIAL_PRINT

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

ESPLogger logger;
ESPMQTTManager mqttManager(&logger, mqttConfig);
ESPTelemetry espTelemetry(mqttManager, &logger, "esp8266/telemetry");
ESPTimeSetup timeSetup(&logger, "pool.ntp.org", 0, 3600);

// Initialize devices
Adafruit_BMP085 bmp;
LiquidCrystal_I2C lcd(0x27, 16, 2);
Preferences preferences;
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(80);
ConfigManager configManager;
SensorManager sensorManager(configManager);
RelayManager relayManager(configManager, sensorManager);
LCDManager lcdManager(lcd, sensorManager, configManager);
SensorPublishTask publishTask(sensorManager, mqttManager, configManager);
ESP32WebServer webServer(80, logger, relayManager, sensorManager, configManager);

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
void setupSensors() {
    Wire.begin(SDA_PIN, SCL_PIN);
    lcd.init();
    lcd.backlight();
}

void setupRelayPins() {
  for (int i = 0; i < RELAY_COUNT; i++) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], HIGH);
    }
}

void setup() {
    Serial.begin(115200);                 
    logger.setLogLevel(LogLevel::INFO);
    setup_wifi();
    setupOTA(&logger);
    timeSetup.setup();
    mqttManager.setup();
    setupLittleFS();
    setupRelayPins();
    sensorManager.setupFloatSwitch(FLOAT_SWITCH_PIN);
    sensorManager.setupSensors(SDA_PIN, SCL_PIN);
    sensorManager.setupMoistureSensors(MOISTURE_SENSOR_PINS);
    sensorManager.startSensorTask();
    relayManager.initRelayControl();
    webServer.begin();
    lcdManager.start();
    publishTask.start();

    logger.log("Setup complete");   
}

void loop() {
  ArduinoOTA.handle();
  mqttManager.loop();
  delay(10);
}

