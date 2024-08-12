#ifndef ESP_TIME_SETUP_H
#define ESP_TIME_SETUP_H

#include <Arduino.h>
#include <time.h>
#include "ESPLogger.h"

template<size_t BUFFER_SIZE = 50>
class ESPTimeSetup {
public:
    ESPTimeSetup(ESPLogger<BUFFER_SIZE>& logger, 
                 const char* ntpServer = "pool.ntp.org", 
                 long gmtOffset_sec = 0, 
                 int daylightOffset_sec = 3600)
        : logger(logger), 
          ntpServer(ntpServer), 
          gmtOffset_sec(gmtOffset_sec), 
          daylightOffset_sec(daylightOffset_sec) {}

    void setup() {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        
        struct tm timeinfo;
        if(!getLocalTime(&timeinfo)){
            logger.log(LogLevel::ERROR, "Failed to obtain time");
            return;
        }
        logger.log(LogLevel::INFO, "Got time from NTP");
    }

    void setNTPServer(const char* server) {
        ntpServer = server;
    }

    void setTimeOffsets(long gmtOffset, int daylightOffset) {
        gmtOffset_sec = gmtOffset;
        daylightOffset_sec = daylightOffset;
    }

private:
    ESPLogger<BUFFER_SIZE>& logger;
    const char* ntpServer;
    long gmtOffset_sec;
    int daylightOffset_sec;
};

#endif // ESP_TIME_SETUP_H