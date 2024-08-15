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
#include "freertos/semphr.h"
#include "esp_log.h" // Add this for logging

#define TAG "RelayManager" // Define a tag for logging

class RelayManager {
public:
    RelayManager(ConfigManager& configManager, SensorManager& sensorManager)
        : configManager(configManager), sensorManager(sensorManager) {
        ESP_LOGI(TAG, "RelayManager initialized");
    }

    SemaphoreHandle_t relaySemaphore;

    void initRelayControl() {
        relaySemaphore = xSemaphoreCreateBinary();
        xSemaphoreGive(relaySemaphore); // Initialize as available
        ESP_LOGI(TAG, "Relay control initialized");
    }

    void activateRelay(size_t index) {
        std::lock_guard<std::mutex> lock(relayStatesMutex);
        if (index >= RELAY_COUNT) {
            ESP_LOGE(TAG, "Invalid relay index: %d", index);
            return;
        }
        if (sensorManager.getSensorData().waterLevel == false) {
            ESP_LOGW(TAG, "Water level too low, cannot activate relay %d", index);
            return;
        }

        if (xSemaphoreTake(relaySemaphore, pdMS_TO_TICKS(10000)) == pdTRUE) {
            relayStates[index] = true;
            lastWateringTime[index] = esp_timer_get_time();
            setRelayHardwareState(index, true);
            ESP_LOGI(TAG, "Relay %d activated", index);

            auto config = configManager.getSensorConfig(index);
            scheduleDeactivation(index, config.activationPeriod);
        } else {
            ESP_LOGW(TAG, "Failed to acquire semaphore for relay %d", index);
        }
    }

    void deactivateRelay(size_t index) {
        std::lock_guard<std::mutex> lock(relayStatesMutex);

        if (index >= RELAY_COUNT) {
            ESP_LOGE(TAG, "Invalid relay index: %d", index);
            return;
        }
        if (deactivationTasks[index] != nullptr) {
            vTaskDelete(deactivationTasks[index]);
            deactivationTasks[index] = nullptr;
        }

        relayStates[index] = false;
        setRelayHardwareState(index, false);
        xSemaphoreGive(relaySemaphore);
        ESP_LOGI(TAG, "Relay %d deactivated", index);
    }

    bool isRelayActive(size_t index) {
        std::lock_guard<std::mutex> lock(relayStatesMutex);
        if (index >= RELAY_COUNT) {
            ESP_LOGE(TAG, "Invalid relay index: %d", index);
            return false;
        }
        return relayStates[index];
    }

    std::array<bool, RELAY_COUNT> getRelayStates() {
        std::lock_guard<std::mutex> lock(relayStatesMutex);
        return relayStates;
    }


private:
    ConfigManager& configManager;
    SensorManager& sensorManager;
    std::array<int64_t, RELAY_COUNT> lastWateringTime = {0};
    std::array<bool, RELAY_COUNT> relayStates = {false};
    std::array<TaskHandle_t, RELAY_COUNT> deactivationTasks = {nullptr};
    std::mutex relayStatesMutex;

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
            // Task creation failed, clean up and handle the error
            delete params;
            ESP_LOGE(TAG, "Failed to create deactivation task for relay %d", index);
            // Handle the error (e.g., try to deactivate immediately or retry)
        }

        ESP_LOGI(TAG, "Deactivation scheduled for relay %d after %d ms", index, duration);
    }

    static void deactivationTask(void* pvParameters) {
        DeactivationParams* params = static_cast<DeactivationParams*>(pvParameters);
        RelayManager* self = params->manager;
        size_t index = params->index;
        
        vTaskDelay(pdMS_TO_TICKS(self->configManager.getSensorConfig(index).activationPeriod));
        if (self->isRelayActive(index)) {
            self->deactivateRelay(index);
        }        
        self->deactivationTasks[index] = nullptr;
        delete params;
        vTaskDelete(nullptr);
    }
    
    static void taskFunction(void* pvParameters) {
        RelayManager* self = static_cast<RelayManager*>(pvParameters);
        while (true) {
            self->controlWatering();
            vTaskDelay(pdMS_TO_TICKS(self->configManager.getSensorUpdateInterval()));
        }
    }

    void controlWatering() {
        int64_t currentTime = esp_timer_get_time();
        SensorData sensorData = sensorManager.getSensorData();

        for (size_t i = 0; i < RELAY_COUNT; ++i) {
            auto config = configManager.getSensorConfig(i);
            int64_t timeSinceLastWatering = currentTime - lastWateringTime[i];

            ESP_LOGD(TAG, "Relay %d: Time since last watering: %lld ms", i, timeSinceLastWatering / 1000);
            ESP_LOGD(TAG, "Relay %d: Moisture level: %d, Threshold: %d", i, sensorData.moisture[i], config.threshold);

            if (timeSinceLastWatering >= config.wateringInterval &&
                sensorData.moisture[i] < config.threshold) {
                ESP_LOGI(TAG, "Activating relay %d due to low moisture", i);
                activateRelay(i);
            }
        }
    }
};

#endif // RELAY_MANAGER_H