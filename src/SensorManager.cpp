#include "SensorManager.h"

SensorManager::SensorManager(ConfigManager& configManager)
    : configManager(configManager), logger(Logger::instance()), sensorTaskHandle(nullptr) {}

void SensorManager::sensorTaskFunction(void* pvParameters) {
    SensorManager* manager = static_cast<SensorManager*>(pvParameters);
    while (true) {
        manager->updateSensorData();
        uint32_t minInterval = UINT32_MAX;
        for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; ++i) {
            const auto& sensorConfig = manager->configManager.getCachedSensorConfig(i);
            if (sensorConfig.sensorEnabled && sensorConfig.activationPeriod < minInterval) {
                minInterval = sensorConfig.activationPeriod;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(minInterval == UINT32_MAX ? 1000 : minInterval));
    }
}

void SensorManager::startSensorTask() {
    if (sensorTaskHandle == nullptr) {
        xTaskCreatePinnedToCore(
            sensorTaskFunction,
            "SensorTask",
            4096,
            this,
            1,
            &sensorTaskHandle,
            1
        );
        logger.log("SensorManager", LogLevel::INFO, "Sensor task started");
    } else {
        logger.log("SensorManager", LogLevel::WARNING, "Sensor task already running");
    }
}

void SensorManager::setupFloatSwitch() {
    const auto& hwConfig = configManager.getCachedHardwareConfig();
    floatSwitchPin = hwConfig.floatSwitchPin;
    pinMode(floatSwitchPin, INPUT_PULLUP);
    logger.log("SensorManager", LogLevel::INFO, "Float switch setup on pin %d", floatSwitchPin);
}

void SensorManager::setupSensors() {
    const auto& hwConfig = configManager.getCachedHardwareConfig();
    Wire.begin(hwConfig.sdaPin, hwConfig.sclPin);
    logger.log("SensorManager", LogLevel::INFO, "I2C initialized on SDA: %d, SCL: %d", hwConfig.sdaPin, hwConfig.sclPin);

    if (!bmp.begin()) {
        logger.log("SensorManager", LogLevel::ERROR, "Could not find a valid BMP085 sensor, check wiring!");
        while (1) {}
    }
    logger.log("SensorManager", LogLevel::INFO, "BMP085 sensor initialized");

    for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; ++i) {
        const auto& sensorConfig = configManager.getCachedSensorConfig(i);
        if (sensorConfig.sensorEnabled) {
            pinMode(sensorConfig.sensorPin, INPUT);
            logger.log("SensorManager", LogLevel::INFO, "Moisture sensor %zu enabled on pin %d", i, sensorConfig.sensorPin);
        }
    }
}

void SensorManager::updateSensorData() {
    std::unique_lock<std::shared_mutex> lock(dataMutex);

    for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; ++i) {
        const auto& sensorConfig = configManager.getCachedSensorConfig(i);
        if (sensorConfig.sensorEnabled) {
            data.moisture[i] = readMoistureSensor(sensorConfig.sensorPin);
        }
    }

    data.temperature = bmp.readTemperature();
    const auto& swConfig = configManager.getCachedSoftwareConfig();
    data.temperature += swConfig.tempOffset;
    data.pressure = bmp.readPressure() / 100.0F;
    data.waterLevel = checkWaterLevel();

    logger.log("SensorManager", LogLevel::DEBUG, "Sensor data updated: Temp: %.2f°C, Pressure: %.2f hPa, Water Level: %s", 
               data.temperature, data.pressure, data.waterLevel ? "OK" : "Low");
}

const SensorData& SensorManager::getSensorData() const {
    std::shared_lock<std::shared_mutex> lock(dataMutex);
    return data;
}

float SensorManager::readMoistureSensor(int sensorPin) {
    const int SAMPLES = 10;
    float sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        sum += analogRead(sensorPin);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    float average = sum / SAMPLES;
    float moisturePercentage = map(average, 0, 4095, 0, 100);
    logger.log("SensorManager", LogLevel::DEBUG, "Moisture sensor on pin %d read: %.2f%%", sensorPin, moisturePercentage);
    return moisturePercentage;
}

bool SensorManager::checkWaterLevel() {
    digitalWrite(floatSwitchPin, HIGH);
    vTaskDelay(pdMS_TO_TICKS(10));
    bool waterLevel = digitalRead(floatSwitchPin);
    digitalWrite(floatSwitchPin, LOW);
    logger.log("SensorManager", LogLevel::DEBUG, "Water level check: %s", waterLevel ? "OK" : "Low");
    return waterLevel;
}