#ifndef OTA_SETUP_H
#define OTA_SETUP_H

#include <ArduinoOTA.h>
#include "ESPLogger.h"

template<size_t BUFFER_SIZE>
void setupOTA(ESPLogger<BUFFER_SIZE>* logger = nullptr) {
    ArduinoOTA.onStart([logger]() {
        if (logger) {
            String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
            logger->log("Start updating %s", type.c_str());
            logger->log("Free Heap: %d", ESP.getFreeHeap());
        }
    });

    ArduinoOTA.onProgress([logger](unsigned int progress, unsigned int total) {
        if (logger) {
            logger->log("OTA Progress: %u%%", (progress / (total / 100)));
        }
    });

    ArduinoOTA.onError([logger](ota_error_t error) {
        if (logger) {
            logger->log("OTA Error[%u]", error);
            if (error == OTA_AUTH_ERROR) logger->log("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) logger->log("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) logger->log("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) logger->log("Receive Failed");
            else if (error == OTA_END_ERROR) logger->log("End Failed");
        }
    });

    ArduinoOTA.onEnd([logger]() {
        if (logger) {
            logger->log("OTA update finished");
        }
    });

    ArduinoOTA.begin();
}

#endif // OTA_SETUP_H