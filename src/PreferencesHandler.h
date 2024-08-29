 /**
 * @class PreferencesHandler
 * @brief Handles low-level operations for storing and retrieving preferences.
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

class PreferencesHandler {
public:
    PreferencesHandler(Logger& logger) : logger(logger) {}
    /**
     * @brief Get the preference key for a given ConfigKey
     * 
     * @param key The ConfigKey to get the preference key for
     * @param sensorIndex The index of the sensor (default is 0)
     * @return std::string The preference key
     * 
     * @note This method handles the complexity of generating unique keys for per-sensor
     * configurations. It appends the sensor index to the preference key if the configuration
     * is sensor-specific (isPerSensor is true in the configMap).
     */
    std::string getPrefKey(ConfigKey key, size_t sensorIndex = 0) const {
        const auto& info = configMap.at(key);
        return info.isPerSensor ? info.prefKey + std::to_string(sensorIndex) : info.prefKey;
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
    void saveToPreferences(ConfigKey key, const T& value, size_t sensorIndex) {
        if constexpr (std::is_same_v<T, int>) {
            preferences.putInt(getPrefKey(key, sensorIndex).c_str(), value);
        } else if constexpr (std::is_same_v<T, float>) {
            preferences.putFloat(getPrefKey(key, sensorIndex).c_str(), value);
        } else if constexpr (std::is_same_v<T, bool>) {
            preferences.putBool(getPrefKey(key, sensorIndex).c_str(), value);
        } else if constexpr (std::is_same_v<T, std::vector<int>>) {
            saveVectorToPreferences(key, value, sensorIndex);
        } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
            saveBoolVectorToPreferences(key, value, sensorIndex);
        } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
            saveVectorToPreferences(key, value, sensorIndex);
        }
        // Add more type handlers here as needed
    }

    /**
     * @brief Save a vector to preferences
     * 
     * @tparam T The type of the vector elements
     * @param key The ConfigKey to save the vector for
     * @param value The vector to save
     * @param sensorIndex The index of the sensor
     * 
     * @note This function serializes the vector into a byte array and stores it in preferences.
     * It also stores the size of the vector separately to allow for correct retrieval later.
     * 
     * @warning This method assumes that the vector elements are trivially copyable. 
     * It may not work correctly for complex types that require deep copying.
     */
    template<typename T>
    void saveVectorToPreferences(ConfigKey key, const std::vector<T>& value, size_t sensorIndex) {
        preferences.putBytes(getPrefKey(key, sensorIndex).c_str(), value.data(), value.size() * sizeof(T));
        preferences.putUInt((getPrefKey(key, sensorIndex) + "_size").c_str(), value.size() * sizeof(T));
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
    void saveBoolVectorToPreferences(ConfigKey key, const std::vector<bool>& value, size_t sensorIndex) {
        std::vector<uint8_t> buffer((value.size() + 7) / 8);
        for (size_t i = 0; i < value.size(); ++i) {
            if (value[i]) {
                buffer[i / 8] |= (1 << (i % 8));
            }
        }
        preferences.putBytes(getPrefKey(key, sensorIndex).c_str(), buffer.data(), buffer.size());
        preferences.putUInt((getPrefKey(key, sensorIndex) + "_size").c_str(), buffer.size());
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
        if constexpr (std::is_same_v<T, int>) {
            return preferences.getInt(getPrefKey(key, sensorIndex).c_str(), defaultValue);
        } else if constexpr (std::is_same_v<T, float>) {
            return preferences.getFloat(getPrefKey(key, sensorIndex).c_str(), defaultValue);
        } else if constexpr (std::is_same_v<T, bool>) {
            return preferences.getBool(getPrefKey(key, sensorIndex).c_str(), defaultValue);
        } else if constexpr (std::is_same_v<T, std::vector<int>>) {
            return loadVectorFromPreferences(key, defaultValue, sensorIndex);
        } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
            return loadBoolVectorFromPreferences(key, defaultValue, sensorIndex);
        } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
            return loadVectorFromPreferences(key, defaultValue, sensorIndex);
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
        size_t size = preferences.getBytes((getPrefKey(key, sensorIndex) + "_size").c_str(), nullptr, 0);
        if (size > 0) {
            vec.resize(size / sizeof(T));
            preferences.getBytes(getPrefKey(key, sensorIndex).c_str(), vec.data(), size);
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
        size_t size = preferences.getBytes((getPrefKey(key, sensorIndex) + "_size").c_str(), nullptr, 0);
        if (size > 0) {
            std::vector<uint8_t> rawData(size);
            preferences.getBytes(getPrefKey(key, sensorIndex).c_str(), rawData.data(), size);
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

private:
    Logger& logger;
    Preferences preferences;
};
#endif // PREFERENCES_HANDLER_H