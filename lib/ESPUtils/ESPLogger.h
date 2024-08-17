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

class Logger {
public:
    enum class Level { DEBUG, INFO, WARNING, ERROR };

    using LogCallback = std::function<void(std::string_view, Level, std::string_view)>;

    static constexpr size_t MAX_LOGS = 100;
    static constexpr size_t LOG_SIZE = 512;
    static constexpr std::string_view DEFAULT_TAG = "DEFAULT";
    static constexpr std::string_view OVERFLOW_MSG = " [LOG OVERFLOW]";

    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    void setCallback(LogCallback cb) {
        std::lock_guard<std::mutex> lock(logMutex);
        callback = std::move(cb);
    }

    void setFilterLevel(Level level) {
        filterLevel.store(level, std::memory_order_relaxed);
    }

    void log(std::string_view tag, Level level, const char* fmt, ...) {
        if (level >= filterLevel.load(std::memory_order_relaxed)) {
            va_list args;
            va_start(args, fmt);
            char message[LOG_SIZE];
            vsnprintf(message, sizeof(message), fmt, args);
            va_end(args);
            addLog(tag, level, message);
        }
    }

    void log(Level level, const char* fmt, ...) {
        if (level >= filterLevel.load(std::memory_order_relaxed)) {
            va_list args;
            va_start(args, fmt);
            char message[LOG_SIZE];
            vsnprintf(message, sizeof(message), fmt, args);
            va_end(args);
            addLog(DEFAULT_TAG, level, message);
        }
    }

    void log(const char* fmt, ...) {
        if (Level::INFO >= filterLevel.load(std::memory_order_relaxed)) {
            va_list args;
            va_start(args, fmt);
            char message[LOG_SIZE];
            vsnprintf(message, sizeof(message), fmt, args);
            va_end(args);
            addLog(DEFAULT_TAG, Level::INFO, message);
        }
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

private:
    std::array<char, MAX_LOGS * LOG_SIZE> buffer;
    std::atomic<size_t> head{0};
    std::atomic<size_t> count{0};
    LogCallback callback;
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
        }
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};

#endif // ESP_LOGGER_H