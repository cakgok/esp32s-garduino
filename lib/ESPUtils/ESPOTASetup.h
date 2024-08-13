#ifndef OTA_SETUP_H
#define OTA_SETUP_H

#include <ArduinoOTA.h>
#include "ESPLogger.h"

inline void setupOTA(ESPLogger* logger = nullptr) {
    ArduinoOTA.onStart([logger]() {
        if (logger) {
            String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
            logger->log(LogLevel::INFO, "Start updating %s", type.c_str());
            logger->log(LogLevel::INFO, "Free Heap: %d", ESP.getFreeHeap());
        }
    });

    ArduinoOTA.onProgress([logger](unsigned int progress, unsigned int total) {
        if (logger) {
            logger->log(LogLevel::INFO, "OTA Progress: %u%%", (progress / (total / 100)));
        }
    });

    ArduinoOTA.onError([logger](ota_error_t error) {
        if (logger) {
            logger->log(LogLevel::ERROR, "OTA Error[%u]", error);
            if (error == OTA_AUTH_ERROR) logger->log(LogLevel::ERROR, "Auth Failed");
            else if (error == OTA_BEGIN_ERROR) logger->log(LogLevel::ERROR, "Begin Failed");
            else if (error == OTA_CONNECT_ERROR) logger->log(LogLevel::ERROR, "Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) logger->log(LogLevel::ERROR, "Receive Failed");
            else if (error == OTA_END_ERROR) logger->log(LogLevel::ERROR, "End Failed");
        }
    });

    ArduinoOTA.onEnd([logger]() {
        if (logger) {
            logger->log(LogLevel::INFO, "OTA update finished");
        }
    });

    ArduinoOTA.begin();
}

#endif // OTA_SETUP_H