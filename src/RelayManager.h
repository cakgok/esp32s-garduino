#include <array>
#include <mutex>
#include "esp_timer.h"
#include "ConfigManager.h"
#include "SensorManager.h"
#include "setup.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class RelayManager {
public:
    RelayManager(ConfigManager& configManager, SensorManager& sensorManager)
        : configManager(configManager), sensorManager(sensorManager) {}

    void startTask(const char* name, uint32_t stackSize, UBaseType_t priority) {
        xTaskCreate(
            taskFunction,
            name,
            stackSize,
            this,
            priority,
            nullptr
        );
    }

    void activateRelay(size_t index) {
        if (index >= RELAY_COUNT) return;
        if (sensorManager.getSensorData().waterLevel == false) return;
        std::lock_guard<std::mutex> lock(relayMutex);

        // Check if any relay is already active
        for (size_t i = 0; i < RELAY_COUNT; ++i) {
            if (relayStates[i]) {
                // A relay is already active, so we can't activate another one
                return;
            }
        }

        // Activate the specified relay
        relayStates[index] = true;
        lastWateringTime[index] = esp_timer_get_time();
        setRelayHardwareState(index, true);

        // Schedule deactivation
        auto config = configManager.getSensorConfig(index);
        scheduleDeactivation(index, config.activationPeriod);
    }

private:
    ConfigManager& configManager;
    SensorManager& sensorManager;
    std::array<int64_t, RELAY_COUNT> lastWateringTime;
    std::array<bool, RELAY_COUNT> relayStates = {false};
    std::mutex relayMutex;

    void deactivateRelay(size_t index) {
        if (index >= RELAY_COUNT) return;
        
        std::lock_guard<std::mutex> lock(relayMutex);
        
        relayStates[index] = false;
        setRelayHardwareState(index, false);
    }

    void setRelayHardwareState(size_t index, bool state) {
        digitalWrite(RELAY_PINS[index], state ? LOW : HIGH);
    }

    struct DeactivationParams {
        RelayManager* manager;
        size_t index;
    };

    void scheduleDeactivation(size_t index, uint32_t duration) {
        DeactivationParams* params = new DeactivationParams{this, index};
        xTaskCreate(
            deactivationTask,
            "DeactivateRelay",
            2048,
            params,
            1,
            nullptr
        );
    }

    static void deactivationTask(void* pvParameters) {
        DeactivationParams* params = static_cast<DeactivationParams*>(pvParameters);
        RelayManager* self = params->manager;
        size_t index = params->index;
        
        vTaskDelay(pdMS_TO_TICKS(self->configManager.getSensorConfig(index).activationPeriod));
        self->deactivateRelay(index);
        
        delete params;
        vTaskDelete(nullptr);
    }
    
    static void taskFunction(void* pvParameters) {
        RelayManager* self = static_cast<RelayManager*>(pvParameters);
        while (true) {
            self->controlWatering();
            // Poll ConfigManager for the latest interval
            vTaskDelay(pdMS_TO_TICKS(self->configManager.getSensorUpdateInterval()));
        }
    }

    void controlWatering() {
        int64_t currentTime = esp_timer_get_time();
        SensorData sensorData = sensorManager.getSensorData();

        for (size_t i = 0; i < RELAY_COUNT; ++i) {
            auto config = configManager.getSensorConfig(i);
            int64_t timeSinceLastWatering = currentTime - lastWateringTime[i];

            if (timeSinceLastWatering >= config.wateringInterval &&
                sensorData.moisture[i] < config.threshold) {
                activateRelay(i);
            }
        }
    }
};