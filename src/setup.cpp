#include "setup.h"
#include "ESP_Utils.h"
#include "SensorManager.h"
#include "ConfigManager.h"

const std::array<int, 4> MOISTURE_SENSOR_PINS = {34, 35, 36, 39};
const int FLOAT_SWITCH_PIN = 32;  // Adjust as needed
const int RELAY_PINS[RELAY_COUNT] = {13, 12, 14, 27}; // Adjust pin numbers as needed

bool relayStates[RELAY_COUNT] = {false, false, false, false};
unsigned long lastHeapMonitorTime = 0;
unsigned long lastLCDUpdateTime = 0;
unsigned long lastSensorPublishTime = 0;
unsigned long lastWateringCheckTime = 0;

// Default values (used only if no value is found in NVM)
const unsigned long DEFAULT_TELEMETRY_INTERVAL = 60000; // 1 minute
const float DEFAULT_TEMPERATURE_OFFSET = 0.0;
const float DEFAULT_MOISTURE_THRESHOLD = 20.0;
const unsigned long DEFAULT_ACTIVATION_PERIOD = 10000; // 10 seconds

// Configuration variables
float temperatureOffset = 0.0;    // Default temperature offset

const ESPMQTTManager<LOGGER_BUFFER_SIZE>::Config mqttConfig = {
    .server = MQTT_SERVER,
    .port = MQTT_PORT,
    .username = MQTT_USERNAME,
    .password = MQTT_PASSWORD,
    .rootCA = root_ca,
    .clientCert = client_cert,
    .clientKey = client_key
};

ESPLogger<LOGGER_BUFFER_SIZE> logger;
ESPMQTTManager<LOGGER_BUFFER_SIZE> mqttManager(logger, mqttConfig);
ESPTelemetry<LOGGER_BUFFER_SIZE> espTelemetry(mqttClient, logger, 60000, "esp8266/telemetry");
ESPTimeSetup<LOGGER_BUFFER_SIZE> timeSetup(logger, "pool.ntp.org", 0, 3600);
ESP32WebServer<LOGGER_BUFFER_SIZE> webServer(80, logger);
SemaphoreHandle_t relaySemaphore;

// Initialize devices
Adafruit_BMP085 bmp;
LiquidCrystal_I2C lcd(0x27, 16, 2);
Preferences preferences;
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(80);
ConfigManager configManager;
SensorManager sensorManager(bmp, MOISTURE_SENSOR_PINS, FLOAT_SWITCH_PIN, configManager);
SensorConfig sensorConfigs[4];

void setup_wifi() {
  logger.log("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    logger.log(".");
  }
}
void setupRelays() {
    for (int i = 0; i < RELAY_COUNT; i++) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], HIGH); // Relays are low activated
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
    if (!bmp.begin()) {
        logger.log("Could not find a valid BMP085 sensor, check wiring!");
        while (1) {}
    }
    lcd.init();
    lcd.backlight();
}
void setupConfigManager() {
    configManager.begin();
    
    // ConfigManager has already loaded cached values, so we can use them directly
    float temperatureOffset = configManager.getTemperatureOffset();
    unsigned long telemetryInterval = configManager.getTelemetryInterval();
    espTelemetry.setInterval(telemetryInterval);
    // No need to manually set default values for sensor configs,
    // as they're handled in ConfigManager's loadCachedValues()
}

void setupMoistureSensors() {
    for (int i = 0; i < 4; i++) {
        pinMode(MOISTURE_SENSOR_PINS[i], INPUT);
    }
}

void setupFloatSwitch() {
    pinMode(FLOAT_SWITCH_PIN, INPUT_PULLUP);
}