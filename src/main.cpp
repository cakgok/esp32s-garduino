#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <WiFi.h>
#include <SPI.h>
#include "FS.h"
#include "LittleFS.h"
#include "ESP_Utils.h"
#include "secrets.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "setup.h"

#define DISABLE_SERIAL_PRINT

int currentLCDSensor = 0;

void setup() {
  Serial.begin(115200);                 //do i even need this?
  logger.setLogLevel(LogLevel::INFO);
  setupRelays();
  setup_wifi();
  setupOTA(&logger);
  setupLittleFS();
  setupPreferences();
  setupSensors();

  webServer.begin();
  timeSetup.setup();
  mqttManager.setup();
  relaySemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(relaySemaphore); // Initialize as available, we don't want to activate more than one relay at a time
  logger.log("Setup complete");   // Mayeb init another semaphore for the sensorData, so we don't read/write at the same time and cause a race condition
}

void checkRelayTimers() {
    unsigned long currentMillis = millis();
    for (int i = 0; i < RELAY_COUNT; i++) {
        if (relayStates[i] && relayTimers[i] > 0 && currentMillis >= relayTimers[i]) {
            if (xSemaphoreTake(relaySemaphore, portMAX_DELAY) == pdTRUE) {
                relayStates[i] = false;
                digitalWrite(RELAY_PINS[i], HIGH);  // Turn off relay
                relayTimers[i] = 0;
                xSemaphoreGive(relaySemaphore);
                logger.log("Relay %d turned off automatically", i);
            }
        }
    }
}

void publishSensorData(unsigned long interval) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastSensorPublishTime >= interval) {
        lastSensorPublishTime = currentMillis;
        SensorData data = sensorManager.getSensorData();
        JsonDocument doc;
        
        for (int i = 0; i < 4; i++) {
            doc["moisture" + String(i)] = data.moisture[i];
        }
        doc["temperature"] = data.temperature;
        doc["pressure"] = data.pressure;
        doc["waterLevel"] = data.waterLevel;

        String payload;
        serializeJson(doc, payload);
        
        if (mqttManager.publish("esp32/sensor_data", payload.c_str())) {
            logger.log("Sensor data published to MQTT");
        } else {
            logger.log("Failed to publish sensor data to MQTT");
        }
    }
}

void activateRelay(int relayIndex, unsigned long duration) {
    if (xSemaphoreTake(relaySemaphore, portMAX_DELAY) == pdTRUE) {
        digitalWrite(RELAY_PINS[relayIndex], LOW);  // Turn on relay
        relayStates[relayIndex] = true;
        relayTimers[relayIndex] = millis() + duration;
        xSemaphoreGive(relaySemaphore);
        logger.log("Activated relay %d for %lu ms", relayIndex, duration);
    }
}

void controlWatering(unsigned long interval) {
    static unsigned long lastWateringCheckTime = 0;
    unsigned long currentMillis = millis();
    if (currentMillis - lastWateringCheckTime >= interval) {
        lastWateringCheckTime = currentMillis;

        SensorData data = sensorManager.getSensorData();
        
        for (int i = 0; i < 4; i++) {
            if (data.moisture[i] < sensorConfigs[i].threshold && 
                (currentMillis - sensorConfigs[i].lastWateringTime) > 12 * 60 * 60 * 1000 && 
                data.waterLevel) {
                
                activateRelay(i, sensorConfigs[i].activationPeriod);
                sensorConfigs[i].lastWateringTime = currentMillis;
            }
        }
    }
}

void updateLCDDisplay(unsigned long interval) {
    static unsigned long lastLCDUpdateTime = 0;
    static int currentLCDSensor = 0;
    
    unsigned long currentMillis = millis();
    if (currentMillis - lastLCDUpdateTime >= interval) {
        lastLCDUpdateTime = currentMillis;

        SensorData data = sensorManager.getSensorData();

        lcd.clear();
        lcd.setCursor(0, 0);
        
        switch (currentLCDSensor) {
            case 0:
            case 1:
            case 2:
            case 3: {
                lcd.print("Moisture ");
                lcd.print(currentLCDSensor + 1);
                lcd.print(": ");
                lcd.print(data.moisture[currentLCDSensor], 1);
                lcd.print("%");
                break;
            }
            case 4: {
                lcd.print("Temp: ");
                lcd.print(data.temperature, 1);
                lcd.print(" C");
                break;
            }
            case 5: {
                lcd.print("Pressure: ");
                lcd.print(data.pressure, 1);
                lcd.print(" hPa");
                break;
            }
        }
        currentLCDSensor = (currentLCDSensor + 1) % 6;  // Cycle through 6 sensors
    }
}

//add a freeRTOS task that takes sensor reading, use a mutex to prevent race conditions(or a semaphore as we already have in sensorManager get())
//the task will work outside of the main loop.

void loop() {
  ArduinoOTA.handle();
  mqttManager.loop();
  updateLCDDisplay(LCD_UPDATE_INTERVAL);
  publishSensorData(SENSOR_PUBLISH_INTERVAL);
  controlWatering(WATERING_CHECK_INTERVAL);
  checkRelayTimers();
}

