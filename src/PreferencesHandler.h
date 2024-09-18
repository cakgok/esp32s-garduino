 /**
 * @class PreferencesHandler
 * @brief Handles low-level operations for storing and retrieving preferences->
 * 
 * @warning This class is not thread-safe on its own. It is designed to be used
 * exclusively through the ConfigManager class, which provides the necessary
 * synchronization. Do not use this class directly in a multi-threaded context.
*/

#ifndef PREFERENCES_HANDLER_H
#define PREFERENCES_HANDLER_H

#include <Preferences.h>
#include "ESPLogger.h"
#include <cstring>
#include "ConfigTypes.h"
#include <nvs_flash.h>

class PreferencesHandler {
public:
    PreferencesHandler() : logger(Logger::instance()), preferences(nullptr) {}
    void setPreferences(Preferences* prefs) {
        preferences = prefs;
    }

    /**
     * @brief Get the preference key for a given ConfigKey
     * 
     * @param key The ConfigKey to get the preference key for
     * @param sensorIndex The index of the sensor (default is 0)
     * @return std::string The preference key
     * 
     * @note This method handles the complexity of generating unique keys for per-sensor
     * configurations. It appends the sensor index to the preference key if the configuration
     * is sensor-specific.
     */
    std::string getPrefKey(ConfigKey key, size_t sensorIndex = 0) const {
        const auto& info = configMap.at(key);
        if (info.confType == "sensorConf") {
            return info.prefKey + std::to_string(sensorIndex);
        }      
        return info.prefKey;    
    }

    /**
     * @brief Save a value to preferences
     * 
     * @tparam T The type of the value to save
     * @param key The ConfigKey to save the value for
     * @param value The value to save
     * @param sensorIndex The index of the sensor (default is 0)
     * 
     * @note This template function uses compile-time type checking (std::is_same_v)
     * to determine the appropriate storage method for different types. This approach
     * allows for type-safe preference storage without runtime overhead.
     * 
     * @warning When adding new types, ensure to update this function to handle them correctly.
     */
    template<typename T>
    bool saveToPreferences(ConfigKey key, const T& value, size_t sensorIndex) {

        if (preferences == nullptr) {
            logger.log("PreferencesHandler", LogLevel::ERROR, "Preferences not initialized");
            return false;
        }

        Logger::instance().log("PreferencesHandler", LogLevel::DEBUG, "Attempting to save key: %d, sensorIndex: %d", static_cast<int>(key), sensorIndex);
        
        std::string prefKey = getPrefKey(key, sensorIndex);
        Logger::instance().log("PreferencesHandler", LogLevel::DEBUG, "Generated preference key: %s", prefKey.c_str());

        bool result = false;
        if constexpr (std::is_same_v<T, int>) {
            result = preferences->putInt(prefKey.c_str(), value);
        } else if constexpr (std::is_same_v<T, float>) {
            result = preferences->putFloat(prefKey.c_str(), value);
        } else if constexpr (std::is_same_v<T, bool>) {
            result = preferences->putBool(prefKey.c_str(), value);
        } else if constexpr (std::is_same_v<T, std::vector<int>>) {
            result = saveVectorToPreferences(key, value, sensorIndex);
        } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
            result = saveBoolVectorToPreferences(key, value, sensorIndex);
        }
        // Add more type handlers here as needed

        if (result) {
            Logger::instance().log("PreferencesHandler", LogLevel::INFO, "Successfully saved key: %s:", prefKey.c_str());
        } else {
            Logger::instance().log("PreferencesHandler", LogLevel::ERROR, "Failed to save key: %s:", prefKey.c_str());
        }
        return result;
    }

    /**
     * @brief Save a vector to preferences
     * 
     * @tparam T The type of the vector elements
     * @param key The ConfigKey to save the vector for
     * @param value The vector to save
     * @param sensorIndex The index of the sensor
     * 
     * @note This function serializes the vector into a byte array and stores it in preferences->
     * It also stores the size of the vector separately to allow for correct retrieval later.
     * 
     * @warning This method assumes that the vector elements are trivially copyable. 
     * It may not work correctly for complex types that require deep copying.
     */
    template<typename T>
    bool saveVectorToPreferences(ConfigKey key, const std::vector<T>& value, size_t sensorIndex) {
            // Store the actual vector data
        bool bytesSuccess = preferences->putBytes(getPrefKey(key, sensorIndex).c_str(), value.data(), value.size() * sizeof(T));
            // Store the number of elements in the vector
        bool sizeSuccess = preferences->putUInt((getPrefKey(key, sensorIndex) + "_size").c_str(), value.size());
        return bytesSuccess && sizeSuccess;
    }


    /**
     * @brief Save a vector of booleans to preferences
     * 
     * @param key The ConfigKey to save the vector for
     * @param value The vector of booleans to save
     * @param sensorIndex The index of the sensor
     * 
     * @note This function uses bit manipulation to compress the boolean vector into a byte array.
     * Each byte stores 8 boolean values, with each bit representing one boolean.
     * This approach significantly reduces the storage space required for boolean vectors.
     * 
     * @warning The size of the stored data is in bytes, not the number of booleans.
     * When retrieving, you'll need to expand this back into individual boolean values.
     */
    bool saveBoolVectorToPreferences(ConfigKey key, const std::vector<bool>& value, size_t sensorIndex) {
        std::vector<uint8_t> buffer((value.size() + 7) / 8);
        for (size_t i = 0; i < value.size(); ++i) {
            if (value[i]) {
                buffer[i / 8] |= (1 << (i % 8));
            }
        }
        bool bytesSuccess = preferences->putBytes(getPrefKey(key, sensorIndex).c_str(), buffer.data(), buffer.size());
        bool sizeSuccess = preferences->putUInt((getPrefKey(key, sensorIndex) + "_size").c_str(), value.size());
        return bytesSuccess && sizeSuccess;
    }


    /**
     * @brief Load a value from preferences
     * 
     * @tparam T The type of the value to load
     * @param key The ConfigKey to load the value for
     * @param defaultValue The default value to return if the key is not found
     * @param sensorIndex The index of the sensor (default is 0)
     * @return T The loaded value
     * 
     * @note This function uses the same compile-time type checking as saveToPreferences
     * to determine the appropriate retrieval method for different types.
     * 
     * @warning When adding new types to saveToPreferences, ensure to update this function
     * to handle them correctly as well.
     */
    template<typename T>
    T loadFromPreferences(ConfigKey key, const T& defaultValue, size_t sensorIndex) {
        if constexpr (std::is_same_v<T, int> || std::is_same_v<T, unsigned int>) {
            return preferences->getInt(getPrefKey(key, sensorIndex).c_str(), defaultValue);
        } else if constexpr (std::is_same_v<T, float>) {
            return preferences->getFloat(getPrefKey(key, sensorIndex).c_str(), defaultValue);
        } else if constexpr (std::is_same_v<T, bool>) {
            return preferences->getBool(getPrefKey(key, sensorIndex).c_str(), defaultValue);
        } else if constexpr (std::is_same_v<T, std::vector<int>>) {
            return loadVectorFromPreferences(key, defaultValue, sensorIndex);
        } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
            return loadBoolVectorFromPreferences(key, defaultValue, sensorIndex);
        } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
            return loadVectorFromPreferences(key, defaultValue, sensorIndex);
        } else  {
            // For any other type, just return the default value
            return defaultValue;
        }
    }

    /**
     * @brief Load a vector from preferences
     * 
     * @tparam T The type of the vector elements
     * @param key The ConfigKey to load the vector for
     * @param defaultValue The default vector to return if the key is not found
     * @param sensorIndex The index of the sensor
     * @return std::vector<T> The loaded vector
     * 
     * @note This function first retrieves the size of the stored data, then allocates
     * a vector of the appropriate size and loads the data into it.
     * 
     * @warning This method assumes that the vector elements are trivially copyable.
     * It may not work correctly for complex types that require deep copying.
     */
    template<typename T>
    std::vector<T> loadVectorFromPreferences(ConfigKey key, const std::vector<T>& defaultValue, size_t sensorIndex) {
        std::vector<T> vec;
        size_t size = preferences->getUInt((getPrefKey(key, sensorIndex) + "_size").c_str(), 0);
        if (size > 0) {
            vec.resize(size);
            preferences->getBytes(getPrefKey(key, sensorIndex).c_str(), vec.data(), size * sizeof(T));
        } else {
            vec = defaultValue;
        }
        return vec;
    }

    /**
     * @brief Load a vector of booleans from preferences
     * 
     * @param key The ConfigKey to load the vector for
     * @param defaultValue The default vector to return if the key is not found
     * @param sensorIndex The index of the sensor
     * @return std::vector<bool> The loaded vector of booleans
     * 
     * @note This function retrieves the compressed boolean data stored as a byte array,
     * then expands it back into individual boolean values. Each bit in the byte array
     * represents one boolean value.
     * 
     * @warning The size stored in preferences is the number of bytes in the compressed
     * representation, not the number of boolean values. This function handles the 
     * expansion back to individual booleans.
     */
    std::vector<bool> loadBoolVectorFromPreferences(ConfigKey key, const std::vector<bool>& defaultValue, size_t sensorIndex) {
        std::vector<bool> vec;

        /** FIX ME
        *   @warning getPrefkey+size gives the wrong key value
        *            for sure size should be a single key, and we should grab the pref keys accordingly.
        */          
        size_t size = preferences->getBytes((getPrefKey(key, sensorIndex) + "_size").c_str(), nullptr, 0);
        if (size > 0) {
            std::vector<uint8_t> rawData(size);
            preferences->getBytes(getPrefKey(key, sensorIndex).c_str(), rawData.data(), size);
            vec.reserve(size * 8);
            for (uint8_t byte : rawData) {
                for (int j = 0; j < 8; ++j) {
                    vec.push_back((byte & (1 << j)) != 0);
                }
            }
        } else {
            vec = defaultValue;
        }
        return vec;
    }

    void removeFromPreferences(ConfigKey key, size_t sensorIndex) {
        preferences->remove(getPrefKey(key, sensorIndex).c_str());
        preferences->remove((getPrefKey(key, sensorIndex) + "_size").c_str());
    }

    bool checkNVSSpace() {
        nvs_stats_t nvs_stats;
        esp_err_t err = nvs_get_stats(NULL, &nvs_stats);
        if (err == ESP_OK) {
            logger.log("ConfigManager", LogLevel::INFO, "NVS: Used entries = %d, Free entries = %d, Total entries = %d",
                    nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);
            return (nvs_stats.free_entries > 0);
        } else {
            logger.log("ConfigManager", LogLevel::ERROR, "Failed to get NVS statistics");
            return false;
        }
    }

private:
    Logger& logger;
    Preferences* preferences;
};
#endif // PREFERENCES_HANDLER_H