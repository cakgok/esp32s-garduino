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
        : logger(Logger::instance()), configManager(configManager), sensorManager(sensorManager), 
          activeRelayIndex(-1), deactivationTask(nullptr), deactivationInProgress(false) {}
    
    void init() {
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

        const auto& hwConfig = configManager.getCachedHardwareConfig();
        int relayPin = hwConfig.relayPins[relayIndex];

        while (deactivationInProgress.load(std::memory_order_acquire)) {
            vTaskDelay(pdMS_TO_TICKS(1));  // Small delay to prevent busy-waiting
        }

        if (activeRelayIndex == relayIndex) {
            logger.log("RelayManager", LogLevel::INFO, "Relay %d is already active", relayIndex);
            return true;
        }
                
        if (relayIndex < 0 || relayIndex >= static_cast<int>(hwConfig.relayPins.size())) {
            logger.log("RelayManager", LogLevel::ERROR, "Invalid relay index: %d", relayIndex);
            return false;
        }

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
        return deactivateRelayInternal(relayIndex);
    }

    bool isRelayActive(int relayIndex) {
        logger.log("RelayManager", LogLevel::DEBUG, "Checking if relay %d is active: %s", relayIndex, relayStates[relayIndex] ? "true" : "false");
        return relayStates[relayIndex];
    }

    bool getRelayState(size_t index) const {
        if (index >= ConfigConstants::RELAY_COUNT) return false;
        return relayStates[index];
    }

    void startControlWateringTask() {
        xTaskCreate(
            controlWateringTaskWrapper,
            "WateringControl",
            4096,
            this,
            1,
            nullptr
        );
        logger.log("RelayManager", LogLevel::INFO, "Watering control task started");
    }
    
private:
    const ConfigManager& configManager;
    SensorManager& sensorManager;
    std::map<int, int64_t> lastWateringTime;
    TaskHandle_t deactivationTask;
    int activeRelayIndex;
    Logger& logger;
    std::array<bool, ConfigConstants::RELAY_COUNT> relayStates = {false};
    std::atomic<bool> deactivationInProgress;
    std::atomic<bool> stopDeactivationTask;

    bool deactivateRelayInternal(int relayIndex) {

        if (!relayStates[relayIndex]) {
            logger.log("RelayManager", LogLevel::INFO, "Relay %d is already inactive", relayIndex);
            return true;
        }

        deactivationInProgress.store(true, std::memory_order_release);

        const auto& hwConfig = configManager.getCachedHardwareConfig();
        int relayPin = hwConfig.relayPins[relayIndex];
        if (deactivationTask != nullptr) {
            stopDeactivationTask.store(true, std::memory_order_release);
            vTaskDelay(pdMS_TO_TICKS(10));  // Give the task a moment to stop
            vTaskDelete(deactivationTask);
            deactivationTask = nullptr;
        }

        relayStates[relayIndex] = false;
        setRelayHardwareState(relayPin, false);
        logger.log("RelayManager", LogLevel::INFO, "Relay %d deactivated (pin %d)", relayIndex, relayPin);

        if (activeRelayIndex == relayIndex) {
            activeRelayIndex = -1;
            logger.log("RelayManager", LogLevel::DEBUG, "Cleared active relay index");
        }

        deactivationInProgress.store(false, std::memory_order_release);
        return true;
    }

    void setRelayHardwareState(int relayPin, bool state) {
        digitalWrite(relayPin, state ? LOW : HIGH);
        logger.log("RelayManager", LogLevel::DEBUG, "Relay on pin %d hardware state set to %s", relayPin, state ? "ON" : "OFF");
    }

    struct DeactivationParams {
        RelayManager* manager;
        int relayIndex;
        std::atomic<bool>* stopFlag;
    };

    static void deactivationTaskFunction(void* pvParameters) {
        DeactivationParams* params = static_cast<DeactivationParams*>(pvParameters);
        RelayManager* self = params->manager;
        int relayIndex = params->relayIndex;
        std::atomic<bool>* stopFlag = params->stopFlag;
        
        const auto& sensorConfig = self->configManager.getCachedSensorConfig(relayIndex);
        self->logger.log("RelayManager", LogLevel::DEBUG, "Deactivation task started for relay %d, waiting for %d ms", relayIndex, sensorConfig.activationPeriod);
        
        TickType_t delayInterval = pdMS_TO_TICKS(100);  // Check every 100ms
        TickType_t totalDelay = pdMS_TO_TICKS(sensorConfig.activationPeriod);
        
        while (totalDelay > 0) {
            if (stopFlag->load(std::memory_order_acquire)) {
                self->logger.log("RelayManager", LogLevel::DEBUG, "Deactivation task for relay %d stopped prematurely", relayIndex);
                break;
            }
            
            vTaskDelay(delayInterval);
            totalDelay -= delayInterval;
        }
        
        if (!stopFlag->load(std::memory_order_acquire)) {
            self->logger.log("RelayManager", LogLevel::DEBUG, "Deactivation time reached for relay %d, deactivating", relayIndex);
            self->deactivationInProgress.store(true, std::memory_order_release);
            self->deactivateRelay(relayIndex);
            self->deactivationInProgress.store(false, std::memory_order_release);
        }
        
        delete params;
        vTaskDelete(nullptr);
    }

    void scheduleDeactivation(int relayIndex, uint32_t duration) {
        stopDeactivationTask.store(false, std::memory_order_release);
        DeactivationParams* params = new DeactivationParams{this, relayIndex, &stopDeactivationTask};
        BaseType_t taskCreated = xTaskCreate(
            deactivationTaskFunction,
            "DeactivateRelay",
            4096,
            params,
            1,
            nullptr 
        );

        if (taskCreated != pdPASS) {
            delete params;
            logger.log("RelayManager", LogLevel::ERROR, "Failed to create deactivation task for relay %d", relayIndex);
        } else {
            logger.log("RelayManager", LogLevel::INFO, "Deactivation scheduled for relay %d after %d ms", relayIndex, duration);
        }
    }

    static constexpr TickType_t RELAY_CHECK_INTERVAL = pdMS_TO_TICKS(5 * 60 * 1000);  // 5 minutes
    static constexpr TickType_t INITIAL_DELAY = pdMS_TO_TICKS(10 * 60 * 1000);  // 10 minutes

    void controlWateringTask() {
        // Initial delay
        vTaskDelay(INITIAL_DELAY);

        while (true) {
            const SensorData& sensorData = sensorManager.getSensorData();
            const auto& hwConfig = configManager.getCachedHardwareConfig();

            // Check water level first
            if (sensorData.waterLevel) {
                int64_t currentTime = esp_timer_get_time();

                for (size_t i = 0; i < ConfigConstants::RELAY_COUNT; ++i) {
                    const auto& config = configManager.getCachedSensorConfig(i);
                    
                    // Skip if relay or sensor is disabled
                    if (!config.relayEnabled || !config.sensorEnabled) {
                        continue;
                    }

                    // Check if enough time has passed since last watering
                    int64_t timeSinceLastWatering = currentTime - lastWateringTime[i];
                    if (timeSinceLastWatering < config.wateringInterval) {
                        continue;
                    }

                    // Check moisture level
                    if (sensorData.moisture[i] < config.threshold) {
                        // All conditions met, activate the relay
                        logger.log("RelayManager", LogLevel::INFO, "Activating relay %d due to low moisture", i);
                        activateRelay(i);
                        break;  // Exit the loop after activating a relay
                    }
                }
            } else {
                logger.log("RelayManager", LogLevel::WARNING, "Water level too low, skipping relay checks");
            }

            // Always wait for the full interval before the next cycle
            vTaskDelay(RELAY_CHECK_INTERVAL);
        }
    }

    static void controlWateringTaskWrapper(void* pvParameters) {
        RelayManager* self = static_cast<RelayManager*>(pvParameters);
        self->controlWateringTask();
    }
};

#endif // RELAY_MANAGER_H
