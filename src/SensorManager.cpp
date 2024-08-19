#include "SensorManager.h"

void SensorManager::sensorTaskFunction(void* pvParameters) {
    SensorManager* manager = static_cast<SensorManager*>(pvParameters);
    while (true) {
        manager->updateSensorData();
        auto sensorConfigs = manager->configManager.getEnabledSensorConfigs();
        if (!sensorConfigs.empty()) {
            uint32_t currentInterval = sensorConfigs[0].activationPeriod;
            vTaskDelay(pdMS_TO_TICKS(currentInterval));
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000)); // Default delay if no sensors are enabled
        }
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
    }
}

void SensorManager::setupFloatSwitch() {
    auto hwConfig = configManager.getHardwareConfig();
    floatSwitchPin = hwConfig.floatSwitchPin;
    pinMode(floatSwitchPin, INPUT_PULLUP);
}

void SensorManager::setupSensors() {
    auto hwConfig = configManager.getHardwareConfig();
    Wire.begin(hwConfig.sdaPin, hwConfig.sclPin);
    if (!bmp.begin()) {
        while (1) {}
    }

    // Setup moisture sensors
    auto sensorConfigs = configManager.getEnabledSensorConfigs();
    for (const auto& config : sensorConfigs) {
        if (config.sensorEnabled) {
            pinMode(config.sensorPin, INPUT);
        }
    }
}

void SensorManager::updateSensorData() {
    std::lock_guard<std::mutex> lock(dataMutex);

    auto sensorConfigs = configManager.getEnabledSensorConfigs();
    for (const auto& config : sensorConfigs) {
        if (config.sensorEnabled) {
            data.moisture[config.sensorPin] = readMoistureSensor(config.sensorPin);
        }
    }

    data.temperature = bmp.readTemperature();
    auto tempOffset = configManager.getValue(ConfigManager::ConfigKey::TEMP_OFFSET);
    if (std::holds_alternative<float>(tempOffset)) {
        data.temperature += std::get<float>(tempOffset);
    }
    data.pressure = bmp.readPressure() / 100.0F;
    data.waterLevel = checkWaterLevel();
}

SensorData SensorManager::getSensorData() {
    std::lock_guard<std::mutex> lock(dataMutex);
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
    return map(average, 0, 4095, 0, 100);
}

bool SensorManager::checkWaterLevel() {
    digitalWrite(floatSwitchPin, HIGH);
    vTaskDelay(pdMS_TO_TICKS(10));
    bool waterLevel = digitalRead(floatSwitchPin);
    digitalWrite(floatSwitchPin, LOW);
    return waterLevel;
}