# Plant Irrigation System

## Overview
This project is an automated plant irrigation system built using an ESP32 microcontroller. It provides a sophisticated solution for monitoring and maintaining optimal conditions for plant growth, combining sensor data with configurable parameters to ensure precise care for your plants.

## Features

### 1. Automated Watering System
- **Soil Moisture Monitoring**: Continuous tracking of soil moisture levels for each plant.
- **Customizable Thresholds**: Set specific moisture thresholds for each plant or plant type.
- **Smart Watering**: Automatically activates water pumps when moisture levels fall below set thresholds.
- **Watering Duration Control**: Configurable watering durations to prevent overwatering.

### 2. Environmental Monitoring
- **Temperature Tracking**: Real-time temperature monitoring using the BMP085 sensor.
- **Pressure Monitoring**: Atmospheric pressure measurement for comprehensive environmental data.
- **Data Logging**: Continuous logging of environmental conditions for trend analysis.

### 3. Water Management
- **Water Level Detection**: Utilizes a float switch to monitor the water reservoir level.
- **Low Water Alerts**: Sends notifications when water levels are low to prevent pump damage.
- **Multiple Pump Support**: Can control multiple water pumps for different plant zones.

### 4. Web-Based Control Interface
- **Real-Time Dashboard**: View current sensor readings, system status, and relay states.
- **Manual Control**: Ability to manually activate/deactivate pumps through the web interface.
- **Configuration Management**: Easy-to-use interface for adjusting system parameters.
- **Responsive Design**: Mobile-friendly interface for monitoring on-the-go.

[Insert web interface screenshot here]

### 5. Advanced Configuration Options
- **Per-Plant Settings**: Customize moisture thresholds, watering durations, and intervals for each plant.
- **System-Wide Parameters**: Configure global settings like update intervals and sensor offsets.
- **Flexible Pin Mapping**: Easily change pin assignments for sensors and relays through the config interface.
- **MQTT push frequency**: Update frequencies for sensors and telemetry publisihing intervals
- You can also disable soil moisture monitoring or automatic watering system per plant.

[Insert web interface screenshot here]

### 8. Logging System
- **MQTT Integration**: Publish sensor data and ~~subscribe to control commands~~ via MQTT.
- **Comprehensive Logging**: Detailed system logs including sensor readings, relay activations, and errors.
- **Web-Accessible Logs**: View logs directly through the web interface for easy troubleshooting.

### 9. Expandability and Flexibility
- **Modular Design**: Easily add more sensors or relays to the system.
- **Customizable Logic**: Modify watering logic to accommodate different plant types or environmental conditions.

### 10. Fail-Safe Mechanisms
- **Pump Protection**: Prevents pump activation when water levels are low.
- **Overwatering Prevention**: Limits watering frequency and duration to protect plants.
- **Overcurrent Protection**: Prevents activation of more than one relay, in order to prevent brownouts due to 5V/1Amps supplies.

  ## Hardware Requirements
- ESP32 development board
- Soil moisture sensors (quantity based on system size)
- BMP085 temperature and pressure sensor
- Relay module for controlling water pumps
- Float switch for water level detection
- I2C LCD display (optional)
- Water pump(s)
- Power supply suitable for your setup

## Software Dependencies
- Arduino core for ESP32
- AsyncTCP
- ESPAsyncWebServer
- ArduinoJson
- Adafruit BMP085 Library
- LiquidCrystal_I2C
- ESP-Arudino-Utils, utility library

## Setup Instructions
1. Copy `secrets_prototype.h` to `secrets.h` and fill in your WiFi and MQTT credentials
2. Adjust the `ConfigConstants` in `globals.h` if needed
3. Flash the device

## Note: The firmware **is not** optimzied for battery usage.

## Future Improvements (TODO)

We're constantly looking to improve and expand the capabilities of our Plant Irrigation System. Here are some planned enhancements and areas for future development:

1. **ULP (Ultra Low Power) Optimization**
   - Implement deep sleep modes for ESP32 to reduce power consumption
   - Utilize ESP32's ULP coprocessor for sensor reading during main processor sleep
   - Optimize wake-up intervals based on plant watering needs and environmental conditions

2. **Enhanced MQTT Control**
   - Expand MQTT command set for comprehensive remote control
   - Implement MQTT-based configuration updates
   - Add support for MQTT-based firmware updates

3. **External ADC Support**
   - Add compatibility with external ADC modules (e.g., ADS1115) for improved sensor reading accuracy
   - Implement auto-detection of external ADC presence
   - Create a flexible configuration system for mapping sensors to internal or external ADC channels

4. **Advanced Watering Algorithms**
   - Implement machine learning algorithms for predictive watering schedules
   - Integrate weather forecast data to optimize watering plans
   - Develop plant-specific watering profiles

5. **Expanded Sensor Support**
   - Add support for additional sensor types (e.g., light sensors, EC sensors)
   - Implement a plug-and-play system for easy sensor addition and configuration
   - Implement a cablibration mechnasim for the soil mositure sensors

 
