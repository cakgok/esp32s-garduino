#ifndef ESP_TIME_SETUP_H
#define ESP_TIME_SETUP_H

#include <time.h>
#include "ESPLogger.h"

class ESPTimeSetup {
public:
    ESPTimeSetup(const char* ntpServer = "pool.ntp.org", 
                 long gmtOffset_sec = 0, 
                 int daylightOffset_sec = 3600)
        : logger(Logger::instance()),
          ntpServer(ntpServer), 
          gmtOffset_sec(gmtOffset_sec), 
          daylightOffset_sec(daylightOffset_sec),
          timeInitialized(false) {}

    bool begin(uint32_t timeout_ms = 10000) {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        
        uint32_t start = millis();
        while (!getLocalTime(&timeinfo) && (millis() - start < timeout_ms)) {
            delay(100);
        }

        if (getLocalTime(&timeinfo)) {
            logger.log("TimeSetup", Logger::Level::INFO, "Time synchronized with NTP server");
            timeInitialized = true;
            return true;
        } else {
            logger.log("TimeSetup", Logger::Level::ERROR, "Failed to obtain time from NTP server");
            return false;
        }
    }

    void setNTPServer(const char* server) {
        ntpServer = server;
        logger.log("TimeSetup", Logger::Level::INFO, "NTP server set to: {}", server);
    }

    void setTimeOffsets(long gmtOffset, int daylightOffset) {
        gmtOffset_sec = gmtOffset;
        daylightOffset_sec = daylightOffset;
        logger.log("TimeSetup", Logger::Level::INFO, "Time offsets updated. GMT: {}s, DST: {}s", gmtOffset, daylightOffset);
    }

    bool isTimeInitialized() const {
        return timeInitialized;
    }

    String getFormattedTime(const char* format = "%Y-%m-%d %H:%M:%S") {
        if (!timeInitialized) {
            return "Time not initialized";
        }
        char timeString[64];
        strftime(timeString, sizeof(timeString), format, &timeinfo);
        return String(timeString);
    }

    time_t getCurrentTime() {
        if (!timeInitialized) {
            return 0;
        }
        time_t now;
        time(&now);
        return now;
    }

private:
    Logger& logger;
    const char* ntpServer;
    long gmtOffset_sec;
    int daylightOffset_sec;
    bool timeInitialized;
    struct tm timeinfo;
};

#endif // ESP_TIME_SETUP_H