#include "ESPLogger.h"
#include <cstdarg>
#include <cstring>
#include <algorithm>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {}

void Logger::setCallback(std::function<void(std::string_view, Level, std::string_view)> cb) {
    std::lock_guard<std::mutex> lock(logMutex);
    callback = std::move(cb);
}

void Logger::addLogObserver(std::function<void(std::string_view, Level, std::string_view)> observer) {
    std::lock_guard<std::mutex> lock(logMutex);
    observers.push_back(std::move(observer));
}

void Logger::setFilterLevel(Level level) {
    filterLevel.store(level, std::memory_order_relaxed);
}

bool Logger::getNextLog(LogEntry& entry) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (count.load(std::memory_order_relaxed) == 0) {
        return false;
    }

    size_t currentTail = tail.load(std::memory_order_relaxed);
    memcpy(&entry, &buffer[currentTail], sizeof(LogEntry));
    tail.store((currentTail + 1) % MAX_LOGS, std::memory_order_relaxed);
    count.fetch_sub(1, std::memory_order_relaxed);
    return true;
}

String Logger::getNextLogJson() {
    LogEntry entry;
    if (getNextLog(entry)) {
        JsonDocument doc;
        doc["tag"] = entry.tag;
        doc["level"] = static_cast<int>(entry.level);
        doc["message"] = entry.message;

        String jsonString;
        serializeJson(doc, jsonString);
        return jsonString;
    }
    return String(""); // Empty string if no more logs
}

bool Logger::peekNextLog(LogEntry& entry, size_t offset) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (count.load(std::memory_order_relaxed) == 0) {
        return false;
    }

    size_t index = (firstLogIndex + offset) % MAX_LOGS;
    memcpy(&entry, &buffer[index], sizeof(LogEntry));
    return true;
}

String Logger::peekNextLogJson(size_t offset) {
    LogEntry entry;
    if (peekNextLog(entry, offset)) {
        JsonDocument doc;
        doc["tag"] = entry.tag;
        doc["level"] = static_cast<int>(entry.level);
        doc["message"] = entry.message;

        String jsonString;
        serializeJson(doc, jsonString);
        return jsonString;
    }
    return String(""); // Empty string if no more logs
}

size_t Logger::getValidLogCount() const {
    return std::min(count.load(std::memory_order_relaxed), MAX_LOGS);
}

size_t Logger::getLogCount() const {
    return count.load(std::memory_order_relaxed);
}

void Logger::addLog(std::string_view tag, Level level, const char* message) {
    if (level >= filterLevel.load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lock(logMutex);
        
        size_t currentHead = head.load(std::memory_order_relaxed);
        LogEntry& entry = buffer[currentHead];
        
        // Copy tag (with null termination)
        strncpy(entry.tag, tag.data(), TAG_SIZE - 1);
        entry.tag[TAG_SIZE - 1] = '\0';
        entry.level = level;

        // Copy message with potential overflow handling
        size_t messageLen = strlen(message);
        if (messageLen >= LOG_SIZE) {
            // Message is too long, truncate and append overflow message
            strncpy(entry.message, message, LOG_SIZE - OVERFLOW_MSG.length() - 1);
            strcpy(entry.message + LOG_SIZE - OVERFLOW_MSG.length() - 1, OVERFLOW_MSG.data());
        } else {
            // Message fits, copy as is
            strcpy(entry.message, message);
        }
        entry.message[LOG_SIZE - 1] = '\0';

        head.store((currentHead + 1) % MAX_LOGS, std::memory_order_relaxed);
        size_t currentCount = count.load(std::memory_order_relaxed);
        if (currentCount < MAX_LOGS) {
            count.store(currentCount + 1, std::memory_order_relaxed);
        } else {
            // Buffer is full, move firstLogIndex
            firstLogIndex.store((firstLogIndex.load(std::memory_order_relaxed) + 1) % MAX_LOGS, std::memory_order_relaxed);
        }

        if (callback) {
            callback(entry.tag, entry.level, entry.message);
        }

        for (const auto& observer : observers) {
            observer(entry.tag, entry.level, entry.message);
        }

        #ifdef ENABLE_SERIAL_PRINT
        const char* levelStr;
        switch (level) {
            case Level::DEBUG: levelStr = "DEBUG"; break;
            case Level::INFO: levelStr = "INFO"; break;
            case Level::WARNING: levelStr = "WARNING"; break;
            case Level::ERROR: levelStr = "ERROR"; break;
            default: levelStr = "UNKNOWN"; break;
        }
        Serial.printf("[%s] %s: %s\n", entry.tag, levelStr, entry.message);
        #endif
    }
}