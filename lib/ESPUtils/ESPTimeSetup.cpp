#include "ESPTimeSetup.h"
#include <Arduino.h>

ESPTimeSetup::ESPTimeSetup(const char* ntpServer, long gmtOffset_sec, int daylightOffset_sec)
    : logger(Logger::instance()),
      ntpServer(ntpServer), 
      gmtOffset_sec(gmtOffset_sec), 
      daylightOffset_sec(daylightOffset_sec),
      timeInitialized(false) {}

bool ESPTimeSetup::begin(uint32_t timeout_ms) {
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

void ESPTimeSetup::setNTPServer(const char* server) {
    ntpServer = server;
    logger.log("TimeSetup", Logger::Level::INFO, "NTP server set to: {}", server);
}

void ESPTimeSetup::setTimeOffsets(long gmtOffset, int daylightOffset) {
    gmtOffset_sec = gmtOffset;
    daylightOffset_sec = daylightOffset;
    logger.log("TimeSetup", Logger::Level::INFO, "Time offsets updated. GMT: {}s, DST: {}s", gmtOffset, daylightOffset);
}

bool ESPTimeSetup::isTimeInitialized() const {
    return timeInitialized;
}

String ESPTimeSetup::getFormattedTime(const char* format) {
    if (!timeInitialized) {
        return "Time not initialized";
    }
    char timeString[64];
    strftime(timeString, sizeof(timeString), format, &timeinfo);
    return String(timeString);
}

time_t ESPTimeSetup::getCurrentTime() {
    if (!timeInitialized) {
        return 0;
    }
    time_t now;
    time(&now);
    return now;
}