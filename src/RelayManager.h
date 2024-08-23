// Improvements
// Use of std::map for lastWateringTime and deactivationTimers: using std::array instead, considering we have a fixed number of relays.
// Ensure that timers are always deleted when no longer needed to prevent memory leaks.
// The activeRelayIndex is accessed from multiple threads without synchronization. This leads to deadlocks in many cases, should be carefully considered.

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
#include <functional>


class ESP32WebServer; // Forward declaration
class RelayManager {
public:
    RelayManager(const ConfigManager& configManager, SensorManager& sensorManager)
        : logger(Logger::instance()), configManager(configManager), sensorManager(sensorManager), 
          activeRelayIndex(-1) {}

    using NotifyClientsCallback = std::function<void()>;
    
    void setNotifyClientsCallback(NotifyClientsCallback callback) {
        notifyClientsCallback = std::move(callback);
    }

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
		
        cancelScheduledDeactivation(relayIndex);
        const auto& hwConfig = configManager.getCachedHardwareConfig();
        int relayPin = hwConfig.relayPins[relayIndex];

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
        const auto& actDuration = configManager.getCachedSensorConfig(relayIndex).activationPeriod;
		//now scheduele a deactivation here, 
		scheduleDeactivation(relayIndex, configManager.getCachedSensorConfig(relayIndex).activationPeriod);
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
    int activeRelayIndex;
    Logger& logger;
    std::array<bool, ConfigConstants::RELAY_COUNT> relayStates = {false};
    NotifyClientsCallback notifyClientsCallback;

    std::map<int, esp_timer_handle_t> deactivationTimers;
    std::mutex relayMutex;

    void scheduleDeactivation(int relayIndex, int64_t delayMs) {
        esp_timer_handle_t timerHandle;

        esp_timer_create_args_t timerArgs = {
            .callback = &RelayManager::deactivateRelayCallback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "deactivation_timer"
        };

        esp_timer_create(&timerArgs, &timerHandle);
        deactivationTimers[relayIndex] = timerHandle;
        esp_timer_start_once(timerHandle, delayMs * 1000);

        logger.log("RelayManager", LogLevel::DEBUG, "Scheduled deactivation for relay %d in %lld ms", relayIndex, delayMs);
    }

    void cancelScheduledDeactivation(int relayIndex) {
        auto it = deactivationTimers.find(relayIndex);
        if (it != deactivationTimers.end()) {
            esp_timer_stop(it->second);
            esp_timer_delete(it->second);
            deactivationTimers.erase(it);

            logger.log("RelayManager", LogLevel::DEBUG, "Canceled scheduled deactivation for relay %d", relayIndex);
        }
    }

    static void deactivateRelayCallback(void* arg) {
        RelayManager* self = static_cast<RelayManager*>(arg);
        std::lock_guard<std::mutex> lock(self->relayMutex);

        int relayIndex = self->getActiveRelayIndex();
        if (relayIndex != -1) {
            self->deactivateRelayInternal(relayIndex);
        }
    }


    bool deactivateRelayInternal(int relayIndex) {
		cancelScheduledDeactivation(relayIndex);
		//check if a deactivation task for the relay is currently in order, and if there is one clear it. 
		
        if (!relayStates[relayIndex]) {
            logger.log("RelayManager", LogLevel::INFO, "Relay %d is already inactive", relayIndex);
            return true;
        }

        const auto& hwConfig = configManager.getCachedHardwareConfig();
        int relayPin = hwConfig.relayPins[relayIndex];
    
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
        if (notifyClientsCallback) {
            notifyClientsCallback();
        }    
    }

    static constexpr TickType_t RELAY_CHECK_INTERVAL = pdMS_TO_TICKS(5 * 60 * 1000);  // 5 minutes
    static constexpr TickType_t INITIAL_DELAY = pdMS_TO_TICKS(10 * 60 * 1000);  // 10 minutes

    int getActiveRelayIndex() {
        return activeRelayIndex;
    }

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
