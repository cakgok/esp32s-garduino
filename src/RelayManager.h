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

class RelayManager {
public:
    RelayManager(const ConfigManager& configManager, SensorManager& sensorManager)
        : logger(Logger::instance()), configManager(configManager), sensorManager(sensorManager), activeRelayIndex(-1) {
        const auto& hwConfig = configManager.getCachedHardwareConfig();
        for (size_t i = 0; i < hwConfig.relayPins.size(); ++i) {
            int pin = hwConfig.relayPins[i];
            pinMode(pin, INPUT);
            digitalWrite(pin, HIGH);  // Assuming relays are active LOW
            pinMode(pin, OUTPUT);
            relayStates[i] = false; // Initialize relay states
            logger.log("RelayManager", LogLevel::DEBUG, "Initialized relay %d on pin %d", i, pin);
        }
        logger.log("RelayManager", LogLevel::INFO, "RelayManager initialized with %d relays", hwConfig.relayPins.size());
    }
    
    bool activateRelay(int relayIndex) {
        std::lock_guard<std::mutex> lock(relayMutex);
        const auto& hwConfig = configManager.getCachedHardwareConfig();
        
        if (relayIndex < 0 || relayIndex >= static_cast<int>(hwConfig.relayPins.size())) {
            logger.log("RelayManager", LogLevel::ERROR, "Invalid relay index: %d", relayIndex);
            return false;
        }

        int relayPin = hwConfig.relayPins[relayIndex];

        if (!sensorManager.getSensorData().waterLevel) {
            logger.log("RelayManager", LogLevel::WARNING, "Water level too low, cannot activate relay %d", relayIndex);
            return false;
        }

        // Deactivate currently active relay if any
        if (activeRelayIndex != -1 && activeRelayIndex != relayIndex) {
            logger.log("RelayManager", LogLevel::INFO, "Deactivating currently active relay %d before activating relay %d", activeRelayIndex, relayIndex);
            deactivateRelayInternal(activeRelayIndex);
        }

        // Activate the requested relay
        activeRelayIndex = relayIndex;
        relayStates[relayIndex] = true;
        lastWateringTime[relayIndex] = esp_timer_get_time();
        setRelayHardwareState(relayPin, true);
        logger.log("RelayManager", LogLevel::INFO, "Relay %d activated (pin %d)", relayIndex, relayPin);

        // Get the corresponding SensorConfig for this relay
        const auto& sensorConfig = configManager.getCachedSensorConfig(relayIndex);
        scheduleDeactivation(relayIndex, sensorConfig.activationPeriod);

        return true;
    }

    bool deactivateRelay(int relayIndex) {
        std::lock_guard<std::mutex> lock(relayMutex);
        return deactivateRelayInternal(relayIndex);
    }

    bool isRelayActive(int relayIndex) {
        std::lock_guard<std::mutex> lock(relayMutex);
        logger.log("RelayManager", LogLevel::DEBUG, "Checking if relay %d is active: %s", relayIndex, relayStates[relayIndex] ? "true" : "false");
        return relayStates[relayIndex];
    }

    bool getRelayState(size_t index) const {
        if (index >= ConfigConstants::RELAY_COUNT) return false;
        return relayStates[index];
    }
    
private:
    const ConfigManager& configManager;
    SensorManager& sensorManager;
    std::map<int, int64_t> lastWateringTime;
    std::map<int, TaskHandle_t> deactivationTasks;
    std::mutex relayMutex;
    int activeRelayIndex;
    Logger& logger;
    std::array<bool, ConfigConstants::RELAY_COUNT> relayStates = {false};

    bool deactivateRelayInternal(int relayIndex) {
        const auto& hwConfig = configManager.getCachedHardwareConfig();
        int relayPin = hwConfig.relayPins[relayIndex];

        auto it = deactivationTasks.find(relayIndex);
        if (it != deactivationTasks.end() && it->second != nullptr) {
            logger.log("RelayManager", LogLevel::DEBUG, "Deleting deactivation task for relay %d", relayIndex);
            vTaskDelete(it->second);
            deactivationTasks.erase(it);
        }

        relayStates[relayIndex] = false;
        setRelayHardwareState(relayPin, false);
        logger.log("RelayManager", LogLevel::INFO, "Relay %d deactivated (pin %d)", relayIndex, relayPin);

        if (activeRelayIndex == relayIndex) {
            activeRelayIndex = -1;
            logger.log("RelayManager", LogLevel::DEBUG, "Cleared active relay index");
        }

        return true;
    }

    void setRelayHardwareState(int relayPin, bool state) {
        digitalWrite(relayPin, state ? LOW : HIGH);
        logger.log("RelayManager", LogLevel::DEBUG, "Relay on pin %d hardware state set to %s", relayPin, state ? "ON" : "OFF");
    }

    struct DeactivationParams {
        RelayManager* manager;
        int relayIndex;
    };

    void scheduleDeactivation(int relayIndex, uint32_t duration) {
        DeactivationParams* params = new DeactivationParams{this, relayIndex};
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
            logger.log("RelayManager", LogLevel::ERROR, "Failed to create deactivation task for relay %d", relayIndex);
        } else {
            deactivationTasks[relayIndex] = taskHandle;
            logger.log("RelayManager", LogLevel::INFO, "Deactivation scheduled for relay %d after %d ms", relayIndex, duration);
        }
    }

    static void deactivationTask(void* pvParameters) {
        DeactivationParams* params = static_cast<DeactivationParams*>(pvParameters);
        RelayManager* self = params->manager;
        int relayIndex = params->relayIndex;
        
        const auto& sensorConfig = self->configManager.getCachedSensorConfig(relayIndex);
        self->logger.log("RelayManager", LogLevel::DEBUG, "Deactivation task started for relay %d, waiting for %d ms", relayIndex, sensorConfig.activationPeriod);
        vTaskDelay(pdMS_TO_TICKS(sensorConfig.activationPeriod));
        
        self->logger.log("RelayManager", LogLevel::DEBUG, "Deactivation time reached for relay %d, deactivating", relayIndex);
        self->deactivateRelay(relayIndex);
        
        self->deactivationTasks.erase(relayIndex);
        delete params;
        vTaskDelete(nullptr);
    }

    // void controlWatering() {
    //     std::lock_guard<std::mutex> lock(relayMutex);
    //     int64_t currentTime = esp_timer_get_time();
    //     const SensorData& sensorData = sensorManager.getSensorData();    
    //     const auto& hwConfig = configManager.getCachedHardwareConfig();

    //     logger.log("RelayManager", LogLevel::DEBUG, "Starting watering control cycle");

    //     for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; ++i) {
    //         const auto& config = configManager.getCachedSensorConfig(i);
    //         if (config.sensorEnabled && config.relayEnabled) {
    //             int relayIndex = i;  // The index is now directly correlated with the config

    //             int64_t timeSinceLastWatering = currentTime - lastWateringTime[relayIndex];

    //             auto moistureIt = sensorData.moisture.find(config.sensorPin);
    //             if (moistureIt != sensorData.moisture.end()) {
    //                 logger.log("RelayManager", LogLevel::DEBUG, "Checking relay %d: moisture = %f, threshold = %f, time since last watering = %lld ms",
    //                            relayIndex, moistureIt->second, config.threshold, timeSinceLastWatering / 1000);

    //                 if (timeSinceLastWatering >= config.wateringInterval &&
    //                     moistureIt->second < config.threshold &&
    //                     activeRelayIndex == -1) {
    //                     logger.log("RelayManager", LogLevel::INFO, "Activating relay %d due to low moisture on sensor pin %d", relayIndex, config.sensorPin);
    //                     activateRelay(relayIndex);
    //                     break;  // Only activate one relay at a time
    //                 }
    //             } else {
    //                 logger.log("RelayManager", LogLevel::WARNING, "No moisture data found for sensor pin %d", config.sensorPin);
    //             }
    //         }
    //     }

    //     logger.log("RelayManager", LogLevel::DEBUG, "Finished watering control cycle");
    // }

// public:
//     void startControlTask() {
//         logger.log("RelayManager", LogLevel::INFO, "Starting watering control task");
//         xTaskCreate(
//             [](void* pvParameters) {
//                 RelayManager* self = static_cast<RelayManager*>(pvParameters);
//                 while (true) {
//                     self->controlWatering();
//                     vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
//                 }
//             },
//             "WateringControl",
//             4096,
//             this,
//             1,
//             nullptr
//         );
//     }
};

#endif // RELAY_MANAGER_H