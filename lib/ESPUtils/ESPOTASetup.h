#ifndef OTA_SETUP_H
#define OTA_SETUP_H

#include <ArduinoOTA.h>
#include "ESPLogger.h"  // Make sure this is the correct path to your new Logger class

inline void setupOTA() {
    Logger& logger = Logger::instance();  // Get the logger instance

    ArduinoOTA.onStart([&logger]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        logger.log(Logger::Level::INFO, "Start updating {}", type.c_str());
        logger.log(Logger::Level::INFO, "Free Heap: {}", ESP.getFreeHeap());
    });

    ArduinoOTA.onProgress([&logger](unsigned int progress, unsigned int total) {
        logger.log(Logger::Level::INFO, "OTA Progress: {}%", (progress / (total / 100)));
    });

    ArduinoOTA.onError([&logger](ota_error_t error) {
        logger.log(Logger::Level::ERROR, "OTA Error[{}]", error);
        switch (error) {
            case OTA_AUTH_ERROR: logger.log(Logger::Level::ERROR, "Auth Failed"); break;
            case OTA_BEGIN_ERROR: logger.log(Logger::Level::ERROR, "Begin Failed"); break;
            case OTA_CONNECT_ERROR: logger.log(Logger::Level::ERROR, "Connect Failed"); break;
            case OTA_RECEIVE_ERROR: logger.log(Logger::Level::ERROR, "Receive Failed"); break;
            case OTA_END_ERROR: logger.log(Logger::Level::ERROR, "End Failed"); break;
        }
    });

    ArduinoOTA.onEnd([&logger]() {
        logger.log(Logger::Level::INFO, "OTA update finished");
    });

    ArduinoOTA.begin();
}

#endif // OTA_SETUP_H