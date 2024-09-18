#ifndef ESP_TIME_SETUP_H
#define ESP_TIME_SETUP_H

#include <time.h>
#include "ESPLogger.h"

class ESPTimeSetup {
public:
    ESPTimeSetup(const char* ntpServer = "pool.ntp.org", 
                 long gmtOffset_sec = 0, 
                 int daylightOffset_sec = 3600);

    bool begin(uint32_t timeout_ms = 10000);
    void setNTPServer(const char* server);
    void setTimeOffsets(long gmtOffset, int daylightOffset);
    bool isTimeInitialized() const;
    String getFormattedTime(const char* format = "%Y-%m-%d %H:%M:%S");
    time_t getCurrentTime();

private:
    Logger& logger;
    const char* ntpServer;
    long gmtOffset_sec;
    int daylightOffset_sec;
    bool timeInitialized;
    struct tm timeinfo;
};

#endif // ESP_TIME_SETUP_H