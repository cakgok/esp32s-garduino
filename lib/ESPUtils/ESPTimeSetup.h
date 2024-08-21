#ifndef ESP_TIME_SETUP_H
#define ESP_TIME_SETUP_H

#include <time.h>
#include "ESPLogger.h"  
#define TAG "Time"

class ESPTimeSetup {
public:
    ESPTimeSetup(const char* ntpServer = "pool.ntp.org", 
                 long gmtOffset_sec = 0, 
                 int daylightOffset_sec = 3600)
        : logger(Logger::instance()),  // Get the logger instance
          ntpServer(ntpServer), 
          gmtOffset_sec(gmtOffset_sec), 
          daylightOffset_sec(daylightOffset_sec) {}

    void setup() {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        
        struct tm timeinfo;
        if(!getLocalTime(&timeinfo)){
            logger.log("TAG", Logger::Level::ERROR, "Failed to obtain time");
            return;
        }
        logger.log("TAG", Logger::Level::INFO, "Got time from NTP");
    }

    void setNTPServer(const char* server) {
        ntpServer = server;
    }

    void setTimeOffsets(long gmtOffset, int daylightOffset) {
        gmtOffset_sec = gmtOffset;
        daylightOffset_sec = daylightOffset;
    }

private:
    Logger& logger;  // Reference to the logger instance
    const char* ntpServer;
    long gmtOffset_sec;
    int daylightOffset_sec;
};

#endif // ESP_TIME_SETUP_H