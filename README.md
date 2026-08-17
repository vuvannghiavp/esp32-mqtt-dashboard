# 📱 ESP32 IoT Dashboard with MQTT

Một hệ thống nhà thông minh (Smart Home IoT) dựa trên ESP32 với khả năng giám sát cảm biến, điều khiển thiết bị từ xa thông qua MQTT và dashboard web được xây dựng trên GitHub Pages.

## 🎯 Tính Năng Chính

### 📊 Giám Sát Cảm Biến Real-time
- **DHT22**: Đo nhiệt độ và độ ẩm
- **BH1750**: Đo cường độ ánh sáng (Lux)
- **Motion Sensor**: Phát hiện chuyển động

### 🔧 Điều Khiển Thiết Bị
- **Quạt thông minh (Smart Fan)**: Điều chỉnh tốc độ qua PWM (0-100%)
- **Hẹn giờ (Timer)**: Lên lịch bật/tắt thiết bị theo thời gian
- **Phản hồi trạng thái**: Xác nhận tất cả lệnh điều khiển
### Đồng bộ nhiều thiết bị (app / web)
### 🌐 Kết Nối
- **WiFi STA**: Kết nối mạng WiFi ổn định với tự động reconnect
- **MQTT**: Giao tiếp với MQTT broker (HiveMQ public broker mặc định)
- **Topic-based**: Tổ chức dữ liệu theo các chủ đề MQTT rõ ràng

### 📈 Dashboard Web
- Hiển thị dữ liệu cảm biến real-time
- Điều khiển các thiết bị từ giao diện web
- Xây dựng trên GitHub Pages (dễ deploy & miễn phí hosting)

## 🛠️ Công Nghệ Sử Dụng

- **Firmware**: ESP-IDF 5.5.3
- **Microcontroller**: ESP32
- **Protocol**: MQTT, WiFi (802.11 b/g/n)
- **Lập trình**: C (FreeRTOS)
- **JSON**: cJSON library để xử lý dữ liệu
- **Web**: HTML/CSS/JavaScript (GitHub Pages)

## 📋 Cấu Trúc Dự Án

```
MQTT_with_Gitpage/
├── main/                 # Điểm vào chính
│   ├── main.c           # Khởi tạo WiFi, task, sensor
│   └── CMakeLists.txt
├── components/
│   ├── my_mqtt/         # Component MQTT
│   │   ├── src/my_mqtt.c      # Xử lý MQTT events & commands
│   │   └── include/my_mqtt.h
│   └── sensor/          # Component sensor (DHT22, BH1750, RFID)
├── build/               # Thư mục build
├── CMakeLists.txt       # Cấu hình CMake chính
└── sdkconfig           # Cấu hình ESP-IDF
```

## 🚀 Chức Năng Chính

### 1️⃣ WiFi Connectivity
- Kết nối WiFi tự động với retry logic
- Lưu trữ IP address ESP32

### 2️⃣ MQTT Publishing
- Gửi dữ liệu nhiệt độ/độ ẩm (3 giây/lần)
- Gửi dữ liệu ánh sáng
- Gửi trạng thái chuyển động
- Gửi trạng thái phản hồi thiết bị

### 3️⃣ MQTT Subscribing
- Nhận lệnh điều chỉnh tốc độ quạt (JSON format)
- Nhận cấu hình hẹn giờ
- Xử lý các yêu cầu điều khiển từ dashboard

### 4️⃣ Multi-tasking (FreeRTOS)
- `sensor_read_task`: Đọc cảm biến định kỳ
- `motion_read_task`: Phát hiện chuyển động
- `check_timer_task`: Kiểm tra hẹn giờ

## 💻 Format MQTT Topics

```
esp32_vuVanNGhia/home/
├── sensors/
│   ├── dht22      → {"temperature": 25.5, "humidity": 60.0}
│   ├── lux        → {"lux": 500}
│   └── motion     → {"motion": 1}
├── fan/
│   └── status     → {"speed": 75, "pwm": 191}
├── config/
│   └── timer/status → {"target": "fan", "time": "10:30", "action": "on"}
└── feedback       → {"device": "fan", "state": "success"}
```

## 🔌 Yêu Cầu Phần Cứng

- ESP32 Development Board
- Sensor DHT22
- Sensor BH1750
- PIR Motion Sensor
- PWM Fan (hoặc thiết bị điều khiển qua PWM)
- Breadboard & Dây cắm
- Nguồn điện 5V/3.3V

## ⚙️ Cấu Hình

**File `Kconfig.projbuild`:**
```
CONFIG_ESP_WIFI_SSID=your_ssid
CONFIG_ESP_WIFI_PASSWORD=your_password
CONFIG_BROKER_URL=mqtt://broker.hivemq.com:1883
```

## 📦 Cách Build & Flash

```bash
# Build project
idf.py build

# Flash lên ESP32
idf.py -p COM3 flash

# Monitor output
idf.py -p COM3 monitor
```

## 🎓 Kiến Thức Áp Dụng

✅ ESP32 & FreeRTOS  
✅ WiFi & MQTT Communication  
✅ Sensor Integration (I2C, Digital IO)  
✅ PWM Control  
✅ Event-driven Architecture  
✅ JSON Parsing & Serialization  
✅ Task Management & Synchronization  

## 📝 Ghi Chú

- Sử dụng HiveMQ public MQTT broker để test
- Cần tạo GitHub Pages repository để host dashboard web
- Có thể mở rộng với thêm sensor, actuator khác
