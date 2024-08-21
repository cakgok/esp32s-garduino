#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <LiquidCrystal_I2C.h>
#include "SensorManager.h"
#include "ConfigManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ESPLogger.h"
#define TAG "LCDManager"

class LCDManager {
private:
    LiquidCrystal_I2C& lcd;
    SensorManager& sensorManager;
    ConfigManager& configManager;
    TaskHandle_t taskHandle;
    int currentDisplay;
    static constexpr uint32_t STARTUP_DELAY_MS = 5000; // 5 second delay

    static void taskFunction(void* pvParameters) {
        LCDManager* self = static_cast<LCDManager*>(pvParameters);
        self->runTask();
    }

    void runTask() {
        Logger::instance().log("TAG", Logger::Level::INFO, "Waiting before starting LCD update task...");
        vTaskDelay(pdMS_TO_TICKS(STARTUP_DELAY_MS));
        Logger::instance().log("TAG", Logger::Level::INFO, "Starting LCD update task");
        
        while (true) {
            updateDisplay();
            const auto& softwareConfig = configManager.getCachedSoftwareConfig();
            vTaskDelay(pdMS_TO_TICKS(softwareConfig.lcdUpdateInterval));
        }
    }

    void updateDisplay() {
        SensorData data = sensorManager.getSensorData();
        lcd.clear();

        switch (currentDisplay) {
            case 0:
                lcd.setCursor(0, 0);
                lcd.print("Temp: " + String(data.temperature, 1) + "C");
                lcd.setCursor(0, 1);
                lcd.print("Press: " + String(data.pressure, 1) + "hPa");
                break;
            case 1:
                displayMoistureData(0, 1);
                break;
            case 2:
                displayMoistureData(2, 3);
                break;
        }

        currentDisplay = (currentDisplay + 1) % 3;
    }

    void displayMoistureData(int sensor1, int sensor2) {
        lcd.setCursor(0, 0);
        lcd.print("Moist" + String(sensor1 + 1) + ": " + getMoistureDisplay(sensor1));
        lcd.setCursor(0, 1);
        lcd.print("Moist" + String(sensor2 + 1) + ": " + getMoistureDisplay(sensor2));
    }

    String getMoistureDisplay(int sensorIndex) {
        const auto& sensorConfig = configManager.getCachedSensorConfig(sensorIndex);
        if (sensorConfig.sensorEnabled) {
            const auto& moistureData = sensorManager.getSensorData().moisture;
            if (sensorConfig.sensorPin < moistureData.size()) {
                return String(moistureData[sensorConfig.sensorPin], 1) + "%";
            } else {
                return "N/A";
            }
        } else {
            return "Disabled";
        }
    }
public:
    LCDManager(LiquidCrystal_I2C& lcd, SensorManager& sm, ConfigManager& cm)
        : lcd(lcd), sensorManager(sm), configManager(cm), taskHandle(NULL), currentDisplay(0) {}

    void start() {
        xTaskCreate(
            taskFunction,
            "LCDUpdateTask",
            4096,  // Stack size (adjust as needed)
            this,
            1,  // Priority
            &taskHandle
        );
    }

    void stop() {
        if (taskHandle != NULL) {
            vTaskDelete(taskHandle);
            taskHandle = NULL;
        }
    }

    ~LCDManager() {
        stop();
    }
};

#endif // LCD_MANAGER_H