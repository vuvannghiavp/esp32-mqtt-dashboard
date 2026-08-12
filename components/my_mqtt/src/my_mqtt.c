
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "my_mqtt.h"
#include "confi.h"
#include "cJSON.h"
#include "esp_err.h"

extern char esp_ip[16];
extern bool wifi_connected;
extern int s_timer_count;

esp_mqtt_client_handle_t client = NULL;
static const char *TAG = "MQTT";

static bool parse_on_off_state(const cJSON *state_item)
{
    if (state_item == NULL)
    {
        return false;
    }

    if (cJSON_IsBool(state_item))
    {
        return cJSON_IsTrue(state_item);
    }

    if (cJSON_IsNumber(state_item))
    {
        return state_item->valueint != 0;
    }

    if (cJSON_IsString(state_item) && state_item->valuestring != NULL)
    {
        const char *value = state_item->valuestring;
        if (strcmp(value, "ON") == 0 || strcmp(value, "on") == 0 ||
            strcmp(value, "true") == 0 || strcmp(value, "TRUE") == 0 ||
            strcmp(value, "1") == 0)
        {
            return true;
        }
        if (strcmp(value, "OFF") == 0 || strcmp(value, "off") == 0 ||
            strcmp(value, "false") == 0 || strcmp(value, "FALSE") == 0 ||
            strcmp(value, "0") == 0)
        {
            return false;
        }
    }

    return false;
}

static void apply_fan_state(bool fan_on, int speed_percent)
{
    if (speed_percent < 0)
    {
        speed_percent = fan_on ? 100 : 0;
    }
    if (speed_percent > 100)
    {
        speed_percent = 100;
    }
    if (speed_percent < 0)
    {
        speed_percent = 0;
    }

    int pwm_value = fan_on ? (speed_percent * 255) / 100 : 0;
    gpio_set_level(RELAY2_GPIO, fan_on ? 1 : 0);
    ledc_set_duty(PWM_MODE, PWM_CHANNEL, pwm_value);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL);
    s_equipment_status.fan_state = fan_on;
    s_equipment_status.fan_speed = fan_on ? speed_percent : 0;
}

// HÀM 1: Xử lý tốc độ quạt (fan/speed/set)
// ================================================================
static void handle_fan_speed(const char *data, int data_len)
{
    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "[FAN] Lỗi parse JSON");
        return;
    }

    cJSON *speed_item = cJSON_GetObjectItem(root, "speed");
    cJSON *pwm_item = cJSON_GetObjectItem(root, "pwm");

    if (!cJSON_IsNumber(speed_item) && !cJSON_IsNumber(pwm_item))
    {
        ESP_LOGW(TAG, "[FAN] Payload thiếu 'speed' hoặc 'pwm'");
        cJSON_Delete(root);
        return;
    }

    int speed_percent = cJSON_IsNumber(speed_item) ? speed_item->valueint : -1;
    int pwm_value = cJSON_IsNumber(pwm_item) ? pwm_item->valueint : -1;

    // Nếu không có pwm sẵn, tự tính từ speed(%)
    if (pwm_value < 0 && speed_percent >= 0)
    {
        if (speed_percent > 100)
            speed_percent = 100;
        if (speed_percent < 0)
            speed_percent = 0;
        pwm_value = (speed_percent * 255) / 100;
    }
    if (pwm_value > 255)
        pwm_value = 255;
    if (pwm_value < 0)
        pwm_value = 0;

    apply_fan_state(pwm_value > 0, speed_percent >= 0 ? speed_percent : (pwm_value * 100 / 255));

    ESP_LOGI(TAG, "[FAN] Đã set tốc độ: %d%% (PWM=%d)", speed_percent, pwm_value);

    // Phản hồi trạng thái lại cho dashboard
    cJSON *status = cJSON_CreateObject();
    cJSON_AddNumberToObject(status, "speed", speed_percent);
    cJSON_AddNumberToObject(status, "pwm", pwm_value);
    char *out = cJSON_PrintUnformatted(status);
    esp_mqtt_client_publish(client, "esp32_vuVanNGhia/home/fan/status", out, 0, 1, 1);
    cJSON_free(out);
    cJSON_Delete(status);

    cJSON_Delete(root);
}

// ================================================================
// HÀM 2: Xử lý cấu hình hẹn giờ (config/timer)
// ================================================================
static void handle_timer_config(const char *data, int data_len)
{
    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "[TIMER] Lỗi parse JSON");
        return;
    }

    cJSON *target_item = cJSON_GetObjectItem(root, "target");
    cJSON *time_item = cJSON_GetObjectItem(root, "time");
    cJSON *action_item = cJSON_GetObjectItem(root, "action");
    cJSON *repeat_item = cJSON_GetObjectItem(root, "repeat");
    cJSON *enabled_item = cJSON_GetObjectItem(root, "enabled");

    if (!cJSON_IsString(target_item) || !cJSON_IsString(time_item) || !cJSON_IsString(action_item))
    {
        ESP_LOGW(TAG, "[TIMER] Payload thiếu trường bắt buộc (target/time/action)");
        cJSON_Delete(root);
        return;
    }

    if (s_timer_count >= MAX_TIMERS)
    {
        ESP_LOGW(TAG, "[TIMER] Đã đạt giới hạn số lịch hẹn");
        cJSON_Delete(root);
        return;
    }

    timer_config_t *t = &s_timers[s_timer_count++];
    strncpy(t->target, target_item->valuestring, sizeof(t->target) - 1);
    t->target[sizeof(t->target) - 1] = '\0';
    strncpy(t->time, time_item->valuestring, sizeof(t->time) - 1);
    t->time[sizeof(t->time) - 1] = '\0';
    t->action_on = (strcmp(action_item->valuestring, "ON") == 0 || strcmp(action_item->valuestring, "on") == 0);
    t->repeat_daily = cJSON_IsTrue(repeat_item);
    t->active = (enabled_item == NULL || cJSON_IsTrue(enabled_item) || cJSON_IsBool(enabled_item) == false);

    ESP_LOGI(TAG, "[TIMER] Đã lưu lịch: %s -> %s lúc %s (lặp lại: %s, active: %s)",
             t->target, t->action_on ? "ON" : "OFF", t->time,
             t->repeat_daily ? "YES" : "NO",
             t->active ? "YES" : "NO");

    // Xác nhận lại cho dashboard
    cJSON *ack = cJSON_CreateObject();
    cJSON_AddStringToObject(ack, "status", "saved");
    cJSON_AddStringToObject(ack, "target", t->target);
    cJSON_AddStringToObject(ack, "time", t->time);
    cJSON_AddStringToObject(ack, "action", t->action_on ? "ON" : "OFF");
    cJSON_AddBoolToObject(ack, "repeat", t->repeat_daily);
    char *out = cJSON_PrintUnformatted(ack);
    esp_mqtt_client_publish(client, "esp32_vuVanNGhia/home/config/timer/status", out, 0, 1, 1);
    cJSON_free(out);
    cJSON_Delete(ack);

    cJSON_Delete(root);
}

// ================================================================
// HÀM 3: Xử lý chuyển đổi chế độ LED (Auto/Manual)
// ================================================================
static void handle_led_mode_switch(const char *data, int data_len)
{
    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "[LED MODE] Lỗi parse JSON");
        return;
    }

    cJSON *mode_item = cJSON_GetObjectItem(root, "mode");
    if (!cJSON_IsString(mode_item))
    {
        ESP_LOGW(TAG, "[LED MODE] Payload thiếu 'mode' (auto/manual)");
        cJSON_Delete(root);
        return;
    }

    const char *mode_str = mode_item->valuestring;
    bool auto_mode = (strcmp(mode_str, "auto") == 0);

    s_equipment_status.led_auto_mode = auto_mode;
    ESP_LOGI(TAG, "[LED MODE] Chuyển sang: %s", auto_mode ? "AUTO" : "MANUAL");

    // Phản hồi lại dashboard
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "mode", auto_mode ? "auto" : "manual");
    cJSON_AddBoolToObject(response, "success", true);
    char *out = cJSON_PrintUnformatted(response);
    esp_mqtt_client_publish(client, "esp32_vuVanNGhia/home/led/mode/status", out, 0, 1, 1);
    cJSON_free(out);
    cJSON_Delete(response);

    cJSON_Delete(root);
}

// ================================================================
// HÀM 4: Xử lý bật/tắt đèn MANUAL
// ================================================================
static void handle_led_manual_control(const char *data, int data_len)
{
    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "[LED MANUAL] Lỗi parse JSON");
        return;
    }

    cJSON *state_item = cJSON_GetObjectItem(root, "state");
    if (!cJSON_IsBool(state_item) && !cJSON_IsNumber(state_item))
    {
        ESP_LOGW(TAG, "[LED MANUAL] Payload thiếu 'state' (true/false)");
        cJSON_Delete(root);
        return;
    }

    bool led_on = cJSON_IsTrue(state_item) || (cJSON_IsNumber(state_item) && state_item->valueint != 0);
    
    // Chỉ điều khiển thủ công khi ở chế độ MANUAL
    if (!s_equipment_status.led_auto_mode)
    {
        gpio_set_level(RELAY1_GPIO, led_on ? 1 : 0);
        s_equipment_status.led_state = led_on;
        ESP_LOGI(TAG, "[LED MANUAL] Đèn: %s", led_on ? "BẬT" : "TẮT");
    }
    else
    {
        ESP_LOGW(TAG, "[LED MANUAL] Ở chế độ AUTO, không thể điều khiển thủ công");
    }

    // Phản hồi lại
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "state", s_equipment_status.led_state);
    cJSON_AddBoolToObject(response, "auto_mode", s_equipment_status.led_auto_mode);
    char *out = cJSON_PrintUnformatted(response);
    esp_mqtt_client_publish(client, "esp32_vuVanNGhia/home/led/control/status", out, 0, 1, 1);
    cJSON_free(out);
    cJSON_Delete(response);

    cJSON_Delete(root);
}

void mqtt_data_dispatch(const char *topic, int topic_len,
                        const char *data, int data_len)
{
    if (strncmp(topic, "esp32_vuVanNGhia/home/fan/speed/set", topic_len) == 0 &&
        topic_len == strlen("esp32_vuVanNGhia/home/fan/speed/set"))
    {
        handle_fan_speed(data, data_len);
    }
    else if (strncmp(topic, "esp32_vuVanNGhia/home/config/timer", topic_len) == 0 &&
             topic_len == strlen("esp32_vuVanNGhia/home/config/timer"))
    {
        handle_timer_config(data, data_len);
    }
    else if (strncmp(topic, "esp32_vuVanNGhia/home/led/mode/set", topic_len) == 0 &&
             topic_len == strlen("esp32_vuVanNGhia/home/led/mode/set"))
    {
        handle_led_mode_switch(data, data_len);
    }
    else if (strncmp(topic, "esp32_vuVanNGhia/home/led/control/set", topic_len) == 0 &&
             topic_len == strlen("esp32_vuVanNGhia/home/led/control/set"))
    {
        handle_led_manual_control(data, data_len);
    }
}
// static void log_error_if_nonzero(const char *message, int error_code)
// {
//     if (error_code != 0)
//     {
//         ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
//     }
// }
// 2. GỬI DỮ LIỆU CẢM BIẾN LÊN WEB DASHBOARD:
void publish_dht22_data(esp_mqtt_client_handle_t client, float temp, float humidity)
{
    // Create JSON object and add sensor data
    // Data sensor DHT22
    cJSON *root = cJSON_CreateObject();          // create JSON object
    cJSON_AddNumberToObject(root, "temp", temp); // add temperature and humidity to JSON object
    cJSON_AddNumberToObject(root, "humidity", humidity);

    char *payload = cJSON_PrintUnformatted(root);                                             // convert JSON into 1 string
    esp_mqtt_client_publish(client, "esp32_vuVanNGhia/home/sensors/dht22", payload, 0, 0, 0); // publish to MQTT topic

    cJSON_Delete(root);
    free(payload);
}
void publish_lux_data(esp_mqtt_client_handle_t client, float lux)
{
    // Data sensor Lux
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "lux", lux);

    char *payload = cJSON_PrintUnformatted(root); // convert JSON into 1 string
    esp_mqtt_client_publish(client, "esp32_vuVanNGhia/home/sensors/lux", payload, 0, 0, 0);

    cJSON_Delete(root);
    free(payload);
}
void publish_motion_data(esp_mqtt_client_handle_t client, uint8_t motion_detected)
{
    // Data sensor Motion
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "motion", motion_detected);

    char *payload = cJSON_PrintUnformatted(root); // convert JSON into 1 string
    esp_mqtt_client_publish(client, "esp32_vuVanNGhia/home/sensors/motion", payload, 0, 1, 1);

    cJSON_Delete(root);
    free(payload);
}
// void publish_status(esp_mqtt_client_handle_t client)
// {
//     cJSON *root = cJSON_CreateObject();
//     cJSON_AddStringToObject(root, "status", "online");
//     cJSON_AddStringToObject(root, "ip", esp_ip);
//     char *payload = cJSON_PrintUnformatted(root); // convert JSON into 1 string
//     esp_mqtt_client_publish(client, "esp32_vuVanNGhia/home/status", payload, 0, 1, 1);
//     // qos = 1, retain = 1
//     cJSON_Delete(root);
//     free(payload);
// }
void publish_feedback(esp_mqtt_client_handle_t client, const char *device, const char *state)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device", device);
    cJSON_AddStringToObject(root, "state", state);

    char *payload = cJSON_PrintUnformatted(root); // convert JSON into 1 string
    esp_mqtt_client_publish(client, "esp32_vuVanNGhia/home/feedback", payload, 0, 1, 1);

    cJSON_Delete(root);
    free(payload);
}
void publish_status(void)
{
    if (client == NULL)
    {
        return;
    }
    cJSON *status = cJSON_CreateObject();
    cJSON_AddStringToObject(status, "status", "online");
    cJSON_AddStringToObject(status, "ip", esp_ip);
    char *payload = cJSON_PrintUnformatted(status);
    esp_mqtt_client_publish(client, "esp32_vuVanNGhia/home/status", payload, 0, 1, 1);
    cJSON_free(payload);
    cJSON_Delete(status);
}

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .session.keepalive = 30,
        .session.last_will = {
            .topic = "esp32_vuVanNGhia/home/status",
            .msg = "{\"status\":\"offline\"}",
            .msg_len = 20,
            .qos = 1,
            .retain = 1,
        },
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

// Kiểm tra & thực thi lịch hẹn (gọi định kỳ mỗi giây, vd trong 1 task riêng)
// currentTimeHHMM: chuỗi "HH:MM" lấy từ SNTP/RTC
// ================================================================
void check_timers(const char *current_time_hhmm)
{
    for (int i = 0; i < s_timer_count; i++)
    {
        if (!s_timers[i].active)
            continue;
        if (strcmp(s_timers[i].time, current_time_hhmm) == 0)
        {
            if (strcmp(s_timers[i].target, "led") == 0)
            {
                bool led_on = s_timers[i].action_on;
                gpio_set_level(RELAY1_GPIO, led_on ? 1 : 0);
                s_equipment_status.led_state = led_on;
                publish_feedback(client, "led", led_on ? "ON" : "OFF");
            }
            else if (strcmp(s_timers[i].target, "fan") == 0)
            {
                bool fan_on = s_timers[i].action_on;
                apply_fan_state(fan_on, fan_on ? 100 : 0);
                publish_feedback(client, "fan", fan_on ? "ON" : "OFF");
            }
            ESP_LOGI(TAG, "[TIMER] Kích hoạt: %s -> %s",
                     s_timers[i].target, s_timers[i].action_on ? "ON" : "OFF");
            if (!s_timers[i].repeat_daily)
            {
                s_timers[i].active = false; // chỉ chạy 1 lần rồi tắt
            }
        }
    }
}

void check_timer_task(void *pvParameters)
{
    char current_time[6] = {0};
    struct tm timeinfo = {0};
    time_t now;

    while (1)
    {
        if (wifi_connected)
        {
            time(&now);
            localtime_r(&now, &timeinfo);
            if (timeinfo.tm_year >= 120)
            {
                snprintf(current_time, sizeof(current_time), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
                check_timers(current_time);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id)
    {
        // ESP32 kết nối MQTT Broker thành công. Subscribe các topic, gửi trạng thái ban đầu
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        wifi_connected = true;
        esp_mqtt_client_subscribe(client, "esp32_vuVanNGhia/home/led/set", 1);
        esp_mqtt_client_subscribe(client, "esp32_vuVanNGhia/home/led/control/set", 1);
        esp_mqtt_client_subscribe(client, "esp32_vuVanNGhia/home/led/mode/set", 1);
        esp_mqtt_client_subscribe(client, "esp32_vuVanNGhia/home/fan/set", 1);
        esp_mqtt_client_subscribe(client, "esp32_vuVanNGhia/home/fan/speed/set", 1);
        esp_mqtt_client_subscribe(client, "esp32_vuVanNGhia/home/config/timer", 1);
        publish_status();
        break;
        // Biết MQTT đã mất kết nối, ghi log/xử lý trạng thái
    case MQTT_EVENT_DISCONNECTED:
        wifi_connected = false;
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
        // Kiểm tra việc subscribe đã thành công
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "SUBSRIBED_SUCCECSSFULLY");
        break;
        // Theo dõi việc gửi message
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "PUBLISH_SUCCECSSFULLY");
        break;
        // ESP32 nhận được message từ topic đã subscribe. Đọc topic + payload và xử lý lệnh/dữ liệu
    case MQTT_EVENT_DATA:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        char topic[64] = {0};
        snprintf(topic, sizeof(topic), "%.*s", event->topic_len, event->topic);

        cJSON *root = cJSON_ParseWithLength(event->data, event->data_len);
        if (root != NULL)
        {
            bool published_feedback = false;
            char feedback_device[16] = {0};
            char feedback_state[16] = {0};

            if (strcmp(topic, "esp32_vuVanNGhia/home/led/set") == 0)
            {
                cJSON *state = cJSON_GetObjectItem(root, "state");
                bool led_on = parse_on_off_state(state);
                gpio_set_level(RELAY1_GPIO, led_on ? 1 : 0);
                s_equipment_status.led_state = led_on;
                strcpy(feedback_device, "led");
                strcpy(feedback_state, led_on ? "ON" : "OFF");
                ESP_LOGI(TAG, "[LED SET] Đèn %s", led_on ? "BẬT" : "TẮT");
                published_feedback = true;
            }
            else if (strcmp(topic, "esp32_vuVanNGhia/home/fan/set") == 0)
            {
                cJSON *state = cJSON_GetObjectItem(root, "state");
                bool fan_on = parse_on_off_state(state);
                apply_fan_state(fan_on, fan_on ? 100 : 0);
                strcpy(feedback_device, "fan");
                strcpy(feedback_state, fan_on ? "ON" : "OFF");
                ESP_LOGI(TAG, "[FAN SET] Quạt %s", fan_on ? "BẬT" : "TẮT");
                published_feedback = true;
            }
            else
            {
                mqtt_data_dispatch(event->topic, event->topic_len,
                                   event->data, event->data_len);
            }

            if (published_feedback)
            {
                publish_feedback(client, feedback_device, feedback_state);
            }
        }
        else
        {
            ESP_LOGW(TAG, "MQTT_EVENT_DATA payload parse failed");
        }

        if (root != NULL)
        {
            cJSON_Delete(root);
        }
        break;
    }
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}