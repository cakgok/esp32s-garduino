#ifndef ESP_OTA_SETUP_H
#define ESP_OTA_SETUP_H

#include <ArduinoOTA.h>
#include "ESPLogger.h"

class OTAManager {
public:
    OTAManager();
    ~OTAManager();
    void begin(const char* hostname = nullptr, const char* password = nullptr);
    void end();

private:
    TaskHandle_t otaTaskHandle = nullptr;
    std::atomic<bool> isOtaInProgress{false};
    static void otaTask(void* parameter);
};



#endif // ESP_OTA_SETUP_H