#ifndef ESP_OTA_SETUP_H
#define ESP_OTA_SETUP_H

#include <ArduinoOTA.h>
#include "ESPLogger.h"

class OTAManager {
public:
    OTAManager() = default;
    ~OTAManager() {
        if (otaTaskHandle != nullptr) {
            vTaskDelete(otaTaskHandle);
        }
    }

    void begin(const char* hostname = nullptr, const char* password = nullptr) {
        if (hostname) {
            ArduinoOTA.setHostname(hostname);
        }

        if (password) {
            ArduinoOTA.setPassword(password);
        }

        ArduinoOTA.onStart([this] {
            isOtaInProgress = true;
            String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
            Logger::instance().log("OTAManager", Logger::Level::INFO, "Start updating %s", type.c_str());
            Logger::instance().log("OTAManager", Logger::Level::INFO, "Free Heap: %d", ESP.getFreeHeap());
        });

        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            static unsigned int lastPercentage = 0;
            unsigned int percentage = (progress / (total / 100));
            
            if (percentage % 10 == 0 && percentage != lastPercentage) {
                Logger::instance().log("OTAManager", Logger::Level::INFO, "OTA Progress: %u%%", percentage);
                lastPercentage = percentage;
            }
        });

        ArduinoOTA.onError([this](ota_error_t error) {
            isOtaInProgress = false;
            Logger::instance().log("OTAManager", Logger::Level::ERROR, "OTA Error[%u]", error);
            switch (error) {
                case OTA_AUTH_ERROR: Logger::instance().log("OTAManager", Logger::Level::ERROR, "Auth Failed"); break;
                case OTA_BEGIN_ERROR: Logger::instance().log("OTAManager", Logger::Level::ERROR, "Begin Failed"); break;
                case OTA_CONNECT_ERROR: Logger::instance().log("OTAManager", Logger::Level::ERROR, "Connect Failed"); break;
                case OTA_RECEIVE_ERROR: Logger::instance().log("OTAManager", Logger::Level::ERROR, "Receive Failed"); break;
                case OTA_END_ERROR: Logger::instance().log("OTAManager", Logger::Level::ERROR, "End Failed"); break;
            }
        });

        ArduinoOTA.onEnd([this] {
            isOtaInProgress = false;
            Logger::instance().log("OTAManager", Logger::Level::INFO, "OTA update finished successfully");
        });

        ArduinoOTA.begin();
        Logger::instance().log("OTAManager", Logger::Level::INFO, "OTA Manager initialized");

        xTaskCreate(
            otaTask,          // Function that implements the task
            "OTA_Task",       // Text name for the task
            4096,             // Stack size in words
            this,             // Parameter passed into the task
            1,                // Priority at which the task is created
            &otaTaskHandle    // Used to pass out the created task's handle
        );
    }

private:
    TaskHandle_t otaTaskHandle = nullptr;
    bool isOtaInProgress = false;

    static void otaTask(void* parameter) {
        OTAManager* otaManager = static_cast<OTAManager*>(parameter);
        const TickType_t xShortDelay = pdMS_TO_TICKS(50);
        const TickType_t xLongDelay = pdMS_TO_TICKS(500);

        for (;;) {
            ArduinoOTA.handle();
            
            // Use the flag to determine the appropriate delay
            TickType_t xDelay = otaManager->isOtaInProgress ? xShortDelay : xLongDelay;

            vTaskDelay(xDelay);
        }
    }
};

#endif // ESP_OTA_SETUP_H