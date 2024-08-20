#ifndef RELAY_MANAGER_H
#define RELAY_MANAGER_H

#include <map>
#include <mutex>
#include "esp_timer.h"
#include "ConfigManager.h"
#include "SensorManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ESPLogger.h"

#define TAG "RelayManager"

class RelayManager {
public:
   RelayManager(ConfigManager& configManager, SensorManager& sensorManager)
        : logger(Logger::instance()), configManager(configManager), sensorManager(sensorManager), activeRelay(-1) {
        auto hwConfig = configManager.getHardwareConfig();
        for (int pin : hwConfig.relayPins) {
            pinMode(pin, INPUT);
            digitalWrite(pin, HIGH);  // Assuming relays are active LOW
            pinMode(pin, OUTPUT);
            relayStates[pin] = false; // Initialize relay states
        }
        logger.log(LogLevel::INFO, "RelayManager initialized");
    }

    
    bool activateRelay(int relayPin) {
        std::lock_guard<std::mutex> lock(relayMutex);
        auto hwConfig = configManager.getHardwareConfig();
        auto it = std::find(hwConfig.relayPins.begin(), hwConfig.relayPins.end(), relayPin);
        if (it == hwConfig.relayPins.end()) {
            logger.log(LogLevel::ERROR, "Invalid relay pin: %d", relayPin);
            return false;
        }
        if (!sensorManager.getSensorData().waterLevel) {
            logger.log(LogLevel::WARNING, "Water level too low, cannot activate relay on pin %d", relayPin);
            return false;
        }

        // Deactivate currently active relay if any
        if (activeRelay != -1 && activeRelay != relayPin) {
            deactivateRelayInternal(activeRelay);
        }

        // Activate the requested relay
        activeRelay = relayPin;
        relayStates[relayPin] = true;
        lastWateringTime[relayPin] = esp_timer_get_time();
        setRelayHardwareState(relayPin, true);
        logger.log(LogLevel::INFO, "Relay on pin %d activated", relayPin);

        // Find the corresponding SensorConfig for this relay
        auto configs = configManager.getEnabledSensorConfigs();
        auto configIt = std::find_if(configs.begin(), configs.end(),
                               [relayPin](const ConfigManager::SensorConfig& cfg) { return cfg.relayPin == relayPin; });
        if (configIt != configs.end()) {
            scheduleDeactivation(relayPin, configIt->activationPeriod);
        } else {
            logger.log(LogLevel::WARNING, "No matching sensor config found for relay pin %d", relayPin);
        }
        return true;
    }

    bool deactivateRelay(int relayPin) {
        std::lock_guard<std::mutex> lock(relayMutex);
        return deactivateRelayInternal(relayPin);
    }

    bool isRelayActive(int relayPin) {
        std::lock_guard<std::mutex> lock(relayMutex);
        return relayStates[relayPin];
    }

    std::map<int, bool> getRelayStates() {
        std::lock_guard<std::mutex> lock(relayMutex);
        return relayStates;
    }

private:
    ConfigManager& configManager;
    SensorManager& sensorManager;
    std::map<int, int64_t> lastWateringTime;
    std::map<int, bool> relayStates;
    std::map<int, TaskHandle_t> deactivationTasks;
    std::mutex relayMutex;
    int activeRelay;
    Logger& logger;

    bool deactivateRelayInternal(int relayPin) {
        auto it = deactivationTasks.find(relayPin);
        if (it != deactivationTasks.end() && it->second != nullptr) {
            vTaskDelete(it->second);
            deactivationTasks.erase(it);
        }

        relayStates[relayPin] = false;
        setRelayHardwareState(relayPin, false);
        ESP_LOGI(TAG, "Relay on pin %d deactivated", relayPin);

        if (activeRelay == relayPin) {
            activeRelay = -1;
        }

        return true;
    }

    void setRelayHardwareState(int relayPin, bool state) {
        digitalWrite(relayPin, state ? LOW : HIGH);
        ESP_LOGI(TAG, "Relay on pin %d hardware state set to %s", relayPin, state ? "ON" : "OFF");
    }

    struct DeactivationParams {
        RelayManager* manager;
        int relayPin;
    };

    void scheduleDeactivation(int relayPin, uint32_t duration) {
       DeactivationParams* params = new DeactivationParams{this, relayPin};
       TaskHandle_t taskHandle;
       BaseType_t taskCreated = xTaskCreate(
            deactivationTask,
            "DeactivateRelay",
            4096,
            params,
            1,
            &taskHandle
        );

        if (taskCreated != pdPASS) {
            delete params;
            ESP_LOGE(TAG, "Failed to create deactivation task for relay on pin %d", relayPin);
        } else {
            deactivationTasks[relayPin] = taskHandle;
        }

        ESP_LOGI(TAG, "Deactivation scheduled for relay on pin %d after %d ms", relayPin, duration);
    }

    static void deactivationTask(void* pvParameters) {
        DeactivationParams* params = static_cast<DeactivationParams*>(pvParameters);
        RelayManager* self = params->manager;
        int relayPin = params->relayPin;
        
        auto configs = self->configManager.getEnabledSensorConfigs();
        auto it = std::find_if(configs.begin(), configs.end(),
                               [relayPin](const ConfigManager::SensorConfig& cfg) { return cfg.relayPin == relayPin; });
        if (it != configs.end()) {
            vTaskDelay(pdMS_TO_TICKS(it->activationPeriod));
        }
        self->deactivateRelay(relayPin);
        
        self->deactivationTasks.erase(relayPin);
        delete params;
        vTaskDelete(nullptr);
    }

    void controlWatering() {
        std::lock_guard<std::mutex> lock(relayMutex);
        int64_t currentTime = esp_timer_get_time();
        SensorData sensorData = sensorManager.getSensorData();

        auto configs = configManager.getEnabledSensorConfigs();
        for (const auto& config : configs) {
            if (config.sensorEnabled && config.relayEnabled) {
                int64_t timeSinceLastWatering = currentTime - lastWateringTime[config.relayPin];

                ESP_LOGD(TAG, "Relay on pin %d: Time since last watering: %lld ms", config.relayPin, timeSinceLastWatering / 1000);
                ESP_LOGD(TAG, "Sensor on pin %d: Moisture level: %.2f, Threshold: %.2f", config.sensorPin, sensorData.moisture[config.sensorPin], config.threshold);

                if (timeSinceLastWatering >= config.wateringInterval &&
                    sensorData.moisture[config.sensorPin] < config.threshold &&
                    activeRelay == -1) {
                    ESP_LOGI(TAG, "Activating relay on pin %d due to low moisture", config.relayPin);
                    activateRelay(config.relayPin);
                    break;  // Only activate one relay at a time
                }
            }
        }
    }

public:
    void startControlTask() {
        xTaskCreate(
            [](void* pvParameters) {
                RelayManager* self = static_cast<RelayManager*>(pvParameters);
                while (true) {
                    self->controlWatering();
                    vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
                }
            },
            "WateringControl",
            4096,
            this,
            1,
            nullptr
        );
    }
};

#endif // RELAY_MANAGER_H