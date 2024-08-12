#ifndef ESP_LOGGER_H
#define ESP_LOGGER_H

#include <Arduino.h>
#include <CircularBuffer.hpp>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

template<size_t BUFFER_SIZE = 50>
class ESPLogger {
public:
    ESPLogger() : currentLogLevel(LogLevel::INFO) {}

    void log(const char* format, ...) {
        va_list args;
        va_start(args, format);
        logInternal(LogLevel::INFO, format, args);
        va_end(args);
    }

    void log(LogLevel level, const char* format, ...) {
        if (level < currentLogLevel) return;
        va_list args;
        va_start(args, format);
        logInternal(level, format, args);
        va_end(args);
    }

    void setLogLevel(LogLevel level) { currentLogLevel = level; }
    String getLogAt(size_t index) { return index < logBuffer.size() ? logBuffer[index] : ""; }
    size_t getLogSize() { return logBuffer.size(); }

    String getLogsHTML() {
    String html = "<ul class='log-list'>";
    for (size_t i = 0; i < logBuffer.size(); i++) {
        html += "<li>" + getLogAt(i) + "</li>";
    }
    html += "</ul>";
    return html;
}

private:
    CircularBuffer<String, BUFFER_SIZE> logBuffer;
    LogLevel currentLogLevel;

    void logInternal(LogLevel level, const char* format, va_list args) {
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        
        String message = String(getLevelString(level)) + ": " + String(buffer);
        logBuffer.push(message);
        
        #ifndef DISABLE_SERIAL_PRINT
        Serial.println(message);
        #endif
    }

    const char* getLevelString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};

#endif // ESP_LOGGER_H