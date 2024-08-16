#ifndef RELAY_MANAGER_H
#define RELAY_MANAGER_H

#include <array>
#include <mutex>
#include "esp_timer.h"
#include "ConfigManager.h"
#include "SensorManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "globals.h"
#include "esp_log.h"

#define TAG "RelayManager"

class RelayManager {
public:
    RelayManager(ConfigManager& configManager, SensorManager& sensorManager)
        : configManager(configManager), sensorManager(sensorManager), activeRelay(-1) {
        ESP_LOGI(TAG, "RelayManager initialized");
    }

    bool activateRelay(size_t index) {
        std::lock_guard<std::mutex> lock(relayMutex);
        if (index >= RELAY_COUNT) {
            ESP_LOGE(TAG, "Invalid relay index: %d", index);
            return false;
        }
        if (sensorManager.getSensorData().waterLevel == false) {
            ESP_LOGW(TAG, "Water level too low, cannot activate relay %d", index);
            return false;
        }

        // Deactivate currently active relay if any
        if (activeRelay != -1 && activeRelay != index) {
            deactivateRelayInternal(activeRelay);
        }

        // Activate the requested relay
        activeRelay = index;
        relayStates[index] = true;
        lastWateringTime[index] = esp_timer_get_time();
        setRelayHardwareState(index, true);
        ESP_LOGI(TAG, "Relay %d activated", index);

        auto config = configManager.getSensorConfig(index);
        scheduleDeactivation(index, config.activationPeriod);
        return true;
    }

    bool deactivateRelay(size_t index) {
        std::lock_guard<std::mutex> lock(relayMutex);
        return deactivateRelayInternal(index);
    }

    bool isRelayActive(size_t index) {
        std::lock_guard<std::mutex> lock(relayMutex);
        if (index >= RELAY_COUNT) {
            ESP_LOGE(TAG, "Invalid relay index: %d", index);
            return false;
        }
        return relayStates[index];
    }

    std::array<bool, RELAY_COUNT> getRelayStates() {
        std::lock_guard<std::mutex> lock(relayMutex);
        return relayStates;
    }

private:
    ConfigManager& configManager;
    SensorManager& sensorManager;
    std::array<int64_t, RELAY_COUNT> lastWateringTime = {0};
    std::array<bool, RELAY_COUNT> relayStates = {false};
    std::array<TaskHandle_t, RELAY_COUNT> deactivationTasks = {nullptr};
    std::mutex relayMutex;
    int activeRelay;

    bool deactivateRelayInternal(size_t index) {
        if (index >= RELAY_COUNT) {
            ESP_LOGE(TAG, "Invalid relay index: %d", index);
            return false;
        }
        if (deactivationTasks[index] != nullptr) {
            vTaskDelete(deactivationTasks[index]);
            deactivationTasks[index] = nullptr;
        }

        relayStates[index] = false;
        setRelayHardwareState(index, false);
        ESP_LOGI(TAG, "Relay %d deactivated", index);

        if (activeRelay == index) {
            activeRelay = -1;
        }

        return true;
    }

    void setRelayHardwareState(size_t index, bool state) {
        digitalWrite(RELAY_PINS[index], state ? LOW : HIGH);
        ESP_LOGI(TAG, "Relay %d hardware state set to %s", index, state ? "ON" : "OFF");
    }

    struct DeactivationParams {
        RelayManager* manager;
        size_t index;
    };

    void scheduleDeactivation(size_t index, uint32_t duration) {
       DeactivationParams* params = new DeactivationParams{this, index};
       BaseType_t taskCreated = xTaskCreate(
            deactivationTask,
            "DeactivateRelay",
            4096,
            params,
            1,
            &deactivationTasks[index]
        );

        if (taskCreated != pdPASS) {
            delete params;
            ESP_LOGE(TAG, "Failed to create deactivation task for relay %d", index);
        }

        ESP_LOGI(TAG, "Deactivation scheduled for relay %d after %d ms", index, duration);
    }

    static void deactivationTask(void* pvParameters) {
        DeactivationParams* params = static_cast<DeactivationParams*>(pvParameters);
        RelayManager* self = params->manager;
        size_t index = params->index;
        
        vTaskDelay(pdMS_TO_TICKS(self->configManager.getSensorConfig(index).activationPeriod));
        self->deactivateRelay(index);
        
        self->deactivationTasks[index] = nullptr;
        delete params;
        vTaskDelete(nullptr);
    }
    
    void controlWatering() {
        std::lock_guard<std::mutex> lock(relayMutex);
        int64_t currentTime = esp_timer_get_time();
        SensorData sensorData = sensorManager.getSensorData();

        for (size_t i = 0; i < RELAY_COUNT; ++i) {
            auto config = configManager.getSensorConfig(i);
            int64_t timeSinceLastWatering = currentTime - lastWateringTime[i];

            ESP_LOGD(TAG, "Relay %d: Time since last watering: %lld ms", i, timeSinceLastWatering / 1000);
            ESP_LOGD(TAG, "Relay %d: Moisture level: %d, Threshold: %d", i, sensorData.moisture[i], config.threshold);

            if (timeSinceLastWatering >= config.wateringInterval &&
                sensorData.moisture[i] < config.threshold &&
                activeRelay == -1) {
                ESP_LOGI(TAG, "Activating relay %d due to low moisture", i);
                activateRelay(i);
                break;  // Only activate one relay at a time
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
                    vTaskDelay(pdMS_TO_TICKS(self->configManager.getSensorUpdateInterval()));
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