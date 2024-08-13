#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <LiquidCrystal_I2C.h>
#include "SensorManager.h"
#include "ConfigManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

class LCDManager {
private:
    LiquidCrystal_I2C& lcd;
    SensorManager& sensorManager;
    ConfigManager& configManager;
    TimerHandle_t timer;
    int currentDisplay;

    static void timerCallback(TimerHandle_t xTimer) {
        LCDManager* self = static_cast<LCDManager*>(pvTimerGetTimerID(xTimer));
        self->updateDisplay();
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
                lcd.setCursor(0, 0);
                lcd.print("Moist1: " + String(data.moisture[0], 1) + "%");
                lcd.setCursor(0, 1);
                lcd.print("Moist2: " + String(data.moisture[1], 1) + "%");
                break;
            case 2:
                lcd.setCursor(0, 0);
                lcd.print("Moist3: " + String(data.moisture[2], 1) + "%");
                lcd.setCursor(0, 1);
                lcd.print("Moist4: " + String(data.moisture[3], 1) + "%");
                break;
        }

        currentDisplay = (currentDisplay + 1) % 3;

        // Update the timer interval for the next update
        uint32_t newInterval = configManager.getLCDUpdateInterval();
        xTimerChangePeriod(timer, pdMS_TO_TICKS(newInterval), 0);
    }

public:
    LCDManager(LiquidCrystal_I2C& lcd, SensorManager& sm, ConfigManager& cm)
        : lcd(lcd), sensorManager(sm), configManager(cm), currentDisplay(0) {
        
        timer = xTimerCreate(
            "LCDUpdateTimer",
            pdMS_TO_TICKS(configManager.getLCDUpdateInterval()),
            pdFALSE,  // Auto-reload set to false
            this,
            timerCallback
        );
    }

    void start() {
        xTimerStart(timer, 0);
    }

    void stop() {
        xTimerStop(timer, 0);
    }

    ~LCDManager() {
        xTimerDelete(timer, 0);
    }
};

#endif