# ESP32 IoT Dashboard with MQTT

A Smart Home IoT system based on ESP32, featuring real-time sensor monitoring, remote device control via MQTT, and a web dashboard hosted on GitHub Pages.

## Main Features

### Real-time Sensor Monitoring

* **DHT22**: Measures temperature and humidity
* **BH1750**: Measures light intensity (Lux)
* **Motion Sensor**: Detects motion

### Device Control

* **Smart Fan**: Adjustable speed control via PWM (0–100%)
* **Timer**: Schedule devices to turn on/off at specified times
* **Status Feedback**: Provides confirmation of control commands

### Multi-Device Synchronization (Mobile App / Web Dashboard)

### Mobile App Integration

The system integrates with the **IoT MQTT Panel** mobile application through the **MQTT protocol**, allowing users to remotely monitor sensor data and control connected devices.

* **Remote monitoring:** Collects and displays temperature, humidity, light intensity, and motion data on the mobile app.
* **Remote control:** Allows users to remotely control devices such as LEDs and fans through MQTT commands.
* **MQTT communication:** The ESP32 publishes sensor data to MQTT topics and subscribes to control topics from the mobile application.
* **Two-way communication:** The mobile application can both receive data from the ESP32 and send control commands back to the device.

**Communication flow:**

`ESP32 → MQTT Broker → IoT MQTT Panel`

`IoT MQTT Panel → MQTT Broker → ESP32`

### Connectivity

* **WiFi STA:** Stable WiFi connection with automatic reconnection
* **MQTT:** Communication with an MQTT broker (HiveMQ public broker by default)
* **Topic-based communication:** Organizes data and commands using clearly structured MQTT topics

### Web Dashboard

* Displays real-time sensor data
* Controls devices through a web interface
* Hosted on **GitHub Pages**

## Technologies Used

* **Firmware:** ESP-IDF 5.5.3
* **Microcontroller:** ESP32
* **Protocols:** MQTT, WiFi
* **Programming:** C with FreeRTOS
* **JSON:** cJSON library for data processing
* **Web:** HTML/CSS/JavaScript (GitHub Pages)

## Project Structure

```text
MQTT_with_Gitpage/
├── main/                         # Main application
│   ├── main.c                    # WiFi initialization, tasks, sensors
│   └── CMakeLists.txt
├── components/
│   ├── my_mqtt/                  # MQTT component
│   │   ├── src/my_mqtt.c         # MQTT events & command handling
│   │   └── include/my_mqtt.h
│   └── sensor/                   # Sensor component (DHT22, BH1750, RFID)
├── build/                        # Build directory
├── CMakeLists.txt                # Main CMake configuration
└── sdkconfig                     # ESP-IDF configuration
```

## Main Functions

### WiFi Connectivity

* Automatic WiFi connection with retry logic
* Stores the ESP32 IP address

### MQTT Publishing

* Publishes temperature and humidity data every 3 seconds
* Publishes light intensity data
* Publishes motion status
* Publishes device status and feedback

### MQTT Subscribing

* Receives fan speed control commands in JSON format
* Receives timer configuration
* Processes control requests from the web dashboard and mobile application

### Multi-tasking (FreeRTOS)

* `sensor_read_task`: Periodically reads sensor data
* `motion_read_task`: Detects motion
* `check_timer_task`: Checks and executes scheduled tasks

## MQTT Topic Structure

```text
esp32_vuVanNGhia/home/
├── sensors/
│   ├── dht22      → {"temperature": 25.5, "humidity": 60.0}
│   ├── lux        → {"lux": 500}
│   └── motion     → {"motion": 1}
├── fan/
│   └── status     → {"speed": 75, "pwm": 191}
├── config/
│   └── timer/status → {"target": "fan", "time": "10:30", "action": "on"}
└── feedback        → {"device": "fan", "state": "success"}
```

## Hardware Requirements

* ESP32 Development Board
* DHT22 Sensor
* BH1750 Sensor
* PIR Motion Sensor
* PWM Fan (or another PWM-controlled device)
* Breadboard & jumper wires
* 5V/3.3V power supply

## Configuration

**File: `Kconfig.projbuild`**

```text
CONFIG_ESP_WIFI_SSID=your_ssid
CONFIG_ESP_WIFI_PASSWORD=your_password
CONFIG_BROKER_URL=mqtt://broker.hivemq.com:1883
```

## Knowledge & Skills Applied

✅ ESP32 & FreeRTOS
✅ WiFi & MQTT Communication
✅ Sensor Integration (I2C, Digital I/O)
✅ PWM Control
✅ Event-driven Architecture
✅ JSON Parsing & Serialization
✅ Task Management & Synchronization

## Notes

* Uses the **HiveMQ public MQTT broker** for testing.
* A GitHub Pages repository is required to host the web dashboard.
* The system can be extended with additional sensors and actuators.
