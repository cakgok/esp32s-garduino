#ifndef ESP_LOGGER_H
#define ESP_LOGGER_H

#include <array>
#include <string>
#include <string_view>
#include <functional>
#include <esp_log.h>
#include <mutex>
#include <atomic>
#include <cstdarg>
#include <vector>
#include <sstream>
#include <type_traits>

#ifdef ENABLE_SERIAL_PRINT
#include <Arduino.h>
#endif
#define ENABLE_SERIAL_PRINT // Enable serial printing

class Logger {
public:
    enum class Level { DEBUG, INFO, WARNING, ERROR };

    static constexpr size_t MAX_LOGS = 100;
    static constexpr size_t LOG_SIZE = 512;
    static constexpr std::string_view DEFAULT_TAG = "DEFAULT";
    static constexpr std::string_view OVERFLOW_MSG = " [LOG OVERFLOW]";

    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    void setCallback(std::function<void(std::string_view, Level, std::string_view)> cb) {
        std::lock_guard<std::mutex> lock(logMutex);
        callback = std::move(cb);
    }

    void addLogObserver(std::function<void(std::string_view, Level, std::string_view)> observer) {
        std::lock_guard<std::mutex> lock(logMutex);
        observers.push_back(std::move(observer));
    }

    void setFilterLevel(Level level) {
        filterLevel.store(level, std::memory_order_relaxed);
    }

    // Helper type trait to detect if first argument is a format string
    template<typename T, typename... Args>
    struct is_format_string : std::false_type {};

    template<typename... Args>
    struct is_format_string<const char*, Args...> : std::true_type {};


    // this approach assumes that if the first argument (after tag and level) is a const char*, it's a format string
    // If you need to concatenate a const char* without treating it as a format string, 
    // wrap it in a std::string or std::string_view.
    template<typename... Args>
    void log(std::string_view tag, Level level, Args&&... args) {
        if (level >= filterLevel.load(std::memory_order_relaxed)) {
            if constexpr (is_format_string<std::decay_t<Args>...>::value) {
                char message[LOG_SIZE];
                snprintf(message, sizeof(message), std::forward<Args>(args)...);
                addLog(tag, level, message);
            } else {
                std::ostringstream oss;
                (oss << ... << std::forward<Args>(args));
                addLog(tag, level, oss.str());
            }
        }
    }

    // Overloads for different argument combinations
    template<typename... Args>
    void log(Level level, Args&&... args) {
        log(DEFAULT_TAG, level, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void log(Args&&... args) {
        log(DEFAULT_TAG, Level::INFO, std::forward<Args>(args)...);
    }

    [[nodiscard]] std::array<std::string_view, MAX_LOGS> getLogs() const {
        std::lock_guard<std::mutex> lock(logMutex);
        std::array<std::string_view, MAX_LOGS> result;
        size_t currentCount = count.load(std::memory_order_relaxed);
        size_t currentHead = head.load(std::memory_order_relaxed);
        size_t index = (currentHead - currentCount + MAX_LOGS) % MAX_LOGS;
        for (size_t i = 0; i < currentCount; i++) {
            result[i] = std::string_view(&buffer[index * LOG_SIZE], strnlen(&buffer[index * LOG_SIZE], LOG_SIZE));
            index = (index + 1) % MAX_LOGS;
        }
        return result;
    }

    struct LogEntry {
        std::string tag;
        Level level;
        std::string message;
    };

    static bool startsWith(const std::string& str, const std::string& prefix) {
        return str.size() >= prefix.size() && 
               str.compare(0, prefix.size(), prefix) == 0;
    }
    std::vector<LogEntry> getChronologicalLogs() const {
        std::lock_guard<std::mutex> lock(logMutex);
        std::vector<LogEntry> result;
        size_t currentCount = count.load(std::memory_order_relaxed);
        size_t currentHead = head.load(std::memory_order_relaxed);
        
        if (currentCount == 0) {
            return result;
        }
        
        result.reserve(currentCount);  // Optimize for performance
        
        size_t startIndex = (currentHead - currentCount + MAX_LOGS) % MAX_LOGS;
        for (size_t i = 0; i < currentCount; i++) {
            size_t index = (startIndex + i) % MAX_LOGS;
            std::string_view logEntry(&buffer[index * LOG_SIZE], strnlen(&buffer[index * LOG_SIZE], LOG_SIZE));
            
            // Parse the log entry
            size_t tagEnd = logEntry.find(']');
            if (tagEnd != std::string_view::npos) {
                std::string tag(logEntry.substr(1, tagEnd - 1));
                std::string message(logEntry.substr(tagEnd + 2));
                
                // Determine log level
                Level level = Level::INFO;
                if (startsWith(message, "ERROR:")) level = Level::ERROR;
                else if (startsWith(message, "WARNING:")) level = Level::WARNING;
                else if (startsWith(message, "DEBUG:")) level = Level::DEBUG;
                
                result.push_back({std::move(tag), level, std::move(message)});
            }
        }
        return result;
    }

private:
    std::array<char, MAX_LOGS * LOG_SIZE> buffer;
    std::atomic<size_t> head{0};
    std::atomic<size_t> count{0};
    std::function<void(std::string_view, Level, std::string_view)> callback;
    std::vector<std::function<void(std::string_view, Level, std::string_view)>> observers;
    mutable std::mutex logMutex;
    std::atomic<Level> filterLevel{Level::DEBUG};

    Logger() {
        esp_log_set_vprintf([](const char* fmt, va_list args) {
            char temp[LOG_SIZE];
            int len = vsnprintf(temp, sizeof(temp), fmt, args);
            if (len < 0) {
                Logger::instance().addLog(DEFAULT_TAG, Level::ERROR, "vsnprintf error in esp_log_set_vprintf");
                return 0;
            }
            if (static_cast<size_t>(len) >= LOG_SIZE) {
                len = LOG_SIZE - 1;
            }
            std::string_view tag = DEFAULT_TAG;
            Level level = Level::INFO;
            // Extract tag and level from the formatted string if possible
            if (len > 1) {
                switch (temp[0]) {
                    case 'E': level = Level::ERROR; break;
                    case 'W': level = Level::WARNING; break;
                    case 'I': level = Level::INFO; break;
                    case 'D': level = Level::DEBUG; break;
                    default: break;
                }
                if (level != Level::INFO) {
                    tag = std::string_view(&temp[2]);
                    Logger::instance().addLog(tag, level, std::string_view(&temp[2], len - 2));
                } else {
                    Logger::instance().addLog(tag, level, std::string_view(temp, len));
                }
            }
            return len;
        });
    }

    void addLog(std::string_view tag, Level level, std::string_view message) {
        if (level >= filterLevel.load(std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lock(logMutex);
            
            size_t currentHead = head.load(std::memory_order_relaxed);
            char* entry = &buffer[currentHead * LOG_SIZE];
            
            std::string formatted = '[' + std::string(tag) + "] " + std::string(message);
            
            if (formatted.size() >= LOG_SIZE) {
                formatted.resize(LOG_SIZE - 1);
                formatted += OVERFLOW_MSG;
            }
            
            strncpy(entry, formatted.c_str(), LOG_SIZE);
            entry[LOG_SIZE - 1] = '\0';

            head.store((currentHead + 1) % MAX_LOGS, std::memory_order_relaxed);
            if (count.load(std::memory_order_relaxed) < MAX_LOGS) {
                count.fetch_add(1, std::memory_order_relaxed);
            }

            if (callback) {
                callback(tag, level, std::string_view(entry, strnlen(entry, LOG_SIZE)));
            }

            for (const auto& observer : observers) {
                observer(tag, level, std::string_view(entry, strnlen(entry, LOG_SIZE)));
            }

            #ifdef ENABLE_SERIAL_PRINT
            Serial.println(formatted.c_str());
            #endif
        }
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};
using LogLevel = Logger::Level;

#endif // ESP_LOGGER_H