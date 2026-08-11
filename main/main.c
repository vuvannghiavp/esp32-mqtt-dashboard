#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "my_mqtt.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "stdbool.h"
#include "confi.h"
#include "sr522.h"
#include "DHT22.h"
#include "zh_bh1750.h"
#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY CONFIG_ESP_MAXIMUM_RETRY
char esp_ip[16];
bool wifi_connected = false;
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
static const char *TAG = "MAIN";
static int s_retry_num = 0;
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(esp_ip, sizeof(esp_ip), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "got ip: %s", esp_ip);
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}
void sensor_read_task(void *pvParameters)
{
    while (!wifi_connected)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    for (;;)
    {
        int ret = readDHT();
        s_sensor_data.temperature = getTemperature();
        s_sensor_data.humidity = getHumidity();
        esp_err_t err = zh_bh1750_read(&bh1750_handle, &s_sensor_data.lux);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read BH1750");
        }
        else
        {
            publish_lux_data(client, s_sensor_data.lux);
        }
        if (ret != DHT_OK)
        {
            ESP_LOGE(TAG, "Failed to read DHT22");
        }
        else
        {
            publish_dht22_data(client, s_sensor_data.temperature, s_sensor_data.humidity);
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
void motion_read_task(void *pvParameters)
{
    bool last_motion = false;
    while (!wifi_connected)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    for (;;)
    {
        s_sensor_data.motion_detected = sr522_is_motion_detected();
        if (s_sensor_data.motion_detected != last_motion)
        {
            last_motion = s_sensor_data.motion_detected;
            publish_motion_data(client, s_sensor_data.motion_detected);

            // AUTO MODE: Điều khiển đèn tự động theo cảm biến chuyển động
            if (s_equipment_status.led_auto_mode)
            {
                gpio_set_level(RELAY1_GPIO, s_sensor_data.motion_detected ? 1 : 0);
                s_equipment_status.led_state = s_sensor_data.motion_detected;
                
                ESP_LOGI(TAG, "[LED AUTO] Motion %s -> LED %s", 
                    s_sensor_data.motion_detected ? "Detected" : "Not Detected",
                    s_sensor_data.motion_detected ? "ON" : "OFF");
                
                // Publish LED status khi auto mode thay đổi
                char payload[100];
                snprintf(payload, sizeof(payload), "{\"mode\":\"auto\",\"state\":%d}", s_sensor_data.motion_detected);
                esp_mqtt_client_publish(client, "esp32_vuVanNGhia/home/led/status", payload, 0, 1, 1);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    hardware_init();
    bh_1750_init();

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    mqtt_app_start();

    xTaskCreate(
        sensor_read_task,
        "Read Data Task",
        4096,
        NULL,
        5,
        NULL);
    xTaskCreate(
        motion_read_task,
        "Read Motion",
        4096,
        NULL,
        5,
        NULL);
    xTaskCreate(
        check_timer_task,
        "Check Timers",
        4096,
        NULL,
        5,
        NULL);

    ret = sr522_init(&sr522_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init SR522: %s", esp_err_to_name(ret));
        return;
    }
}
