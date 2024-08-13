#include "setup.h"
#include "ESPUtils.h"
#include "SensorManager.h"
#include "ConfigManager.h"
#include "RelayManager.h"
#include "LCDManager.h"
#include "PublishManager.h"

const std::array<int, 4> MOISTURE_SENSOR_PINS = {34, 35, 36, 39};
const int FLOAT_SWITCH_PIN = 32;  // Adjust as needed
const int RELAY_PINS[RELAY_COUNT] = {13, 12, 14, 27}; // Adjust pin numbers as needed

// Configuration variables
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
ESP32WebServer webServer(80, logger);

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

void setupFloatSwitch() {
    pinMode(FLOAT_SWITCH_PIN, INPUT_PULLUP);
}