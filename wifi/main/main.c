#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_http_client.h"
#include "mqtt_client.h"
#include <time.h>
#include "esp_system.h"
#include <cJSON.h> 
// #define WIFI_SSID "wifi_przemek"   // Nazwa sieci WiFi   MyNET_Fiber_5385 wifi_przemek
// #define WIFI_PASS "xdxdxdxd"           // Hasło do sieci WiFi   8b0fc920 xdxdxdxd
#define LED_PIN GPIO_NUM_2  // Definiujemy GPIO2 jako pin LED
#define AP_SSID "noise_detector"
#define AP_PASS "qwerty123"

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static const char *TAG = "noise_detector";
char mac_str[13];

bool connected = false;

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
#define MAX_SSID_LEN 32
#define MAX_PASSWORD_LEN 32
#define SSID_KEY "ssid_key"
#define PASSWORD_KEY "password_key"
#define STORAGE_NAMESPACE "WIFI_CONFIG"
#define DEFAULT_VALUE "default"

char ssid_value[MAX_SSID_LEN] = DEFAULT_VALUE;
char password_value[MAX_PASSWORD_LEN] = DEFAULT_VALUE;

static void mqtt_app_start(void);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event);
static void mqtt_app_task(void *pvParameters);


esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, Key=%s, Value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
                // printf("%.*s", evt->data_len, (char*)evt->data); 
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            break;
    }
    return ESP_OK;
}


static void http_rest_with_hostname_path(void)
{
    esp_http_client_config_t config = {
        .host = "httpbin.org",
        .path = "/",
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = _http_event_handler,
        .port=80,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGI(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        connected = false;
        ESP_LOGI(TAG, "%d", connected);
        ESP_LOGI(TAG, "Retrying connection to the WiFi...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        connected = true;
        ESP_LOGI(TAG, "%d", connected);
        ESP_LOGI(TAG, "Connected to WiFi!");
        // http_rest_with_hostname_path();
        //mqtt_app_start();
        xTaskCreate(&mqtt_app_task, "mqtt_app_task", 4096, NULL, 5, NULL);
    }
}

void wifi_init_sta() {
    wifi_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    // ODCZYTYWANIE SSID I HASLA DO WIFI:

    nvs_handle_t nvs_handle;
    esp_err_t err;
    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    char ssid[MAX_SSID_LEN];
    size_t ssid_len = sizeof(ssid);
    char password[MAX_PASSWORD_LEN];
    size_t password_len = sizeof(password);
    err = nvs_get_str(nvs_handle, SSID_KEY, ssid, &ssid_len);
    if (err == ESP_OK) {
        strcpy(ssid_value, ssid);
        ESP_LOGI(TAG, "Current SSID: %s", ssid);
    } else {
        ESP_LOGE(TAG, "Unable to read SSID: %s", esp_err_to_name(err));
    }
    err = nvs_get_str(nvs_handle, PASSWORD_KEY, password, &password_len);
    if (err == ESP_OK) {
        strcpy(password_value, password);
        ESP_LOGI(TAG, "Current password: %s", password);
    } else {
        ESP_LOGE(TAG, "Unable to read password: %s", esp_err_to_name(err));
    }
    nvs_close(nvs_handle);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

void wifi_init_ap() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASS,
            .max_connection = 4,       // Max clients that can connect to the AP
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    if (strlen(AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "ESP32 configured in Access Point mode with SSID: %s", AP_SSID);
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        while (1) {
            float random_decibel = 30.0 + ((float)rand() / RAND_MAX) * (90.0 - 30.0);
            time_t now = time(NULL);
            struct tm *localTime = localtime(&now);
            char timeString[100]; 
            strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", localTime);



            // Wyświetl adres MAC jako string
            ESP_LOGI("MAC_ADDRESS", "Adres MAC: %s", mac_str);

            // Utwórz temat MQTT dynamicznie z adresem MAC
            char topic_noise[128];
            snprintf(topic_noise, sizeof(topic_noise), "noise_detector_web/%s/noise_data", mac_str);

            char noise_data_json[512];
            snprintf(noise_data_json, sizeof(noise_data_json),
                     "{"
                     "\"sensor_id\": \"ESP32\","
                     "\"timestamp\": \"%s\","
                     "\"noise_data\": {"
                     "\"decibels\": %.1f,"
                     "\"status\": \"active\""
                     "}"
                     "}",
                     timeString,
                     random_decibel);

            // Publikacja danych na temat z adresem MAC
            esp_mqtt_client_publish(client, topic_noise, noise_data_json, 0, 1, 0);

            char topic_sensor_info[128];
            snprintf(topic_sensor_info, sizeof(topic_sensor_info), "noise_detector_web/%s/sensor_info", mac_str);

            char sensor_info_json[512];
            snprintf(sensor_info_json, sizeof(sensor_info_json),
                     "{"
                     "\"sensor_id\": \"ESP32\","
                     "\"location\": {"
                     "\"city\": \"Krakow\","
                     "\"street\": \"al. Adama Mickiewicza 30\","
                     "\"latitude\": 50.064470,"
                     "\"longitude\": 19.923290"
                     "},"
                     "\"additional_info\": {"
                     "\"sensor_health\": \"good\""
                     "}"
                     "}");

            // Publikacja informacji o sensorze na temat z adresem MAC
            esp_mqtt_client_publish(client, topic_sensor_info, sensor_info_json, 0, 1, 0);

            vTaskDelay(pdMS_TO_TICKS(15000));  // Odczekaj 15 sekund
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("\nTOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, (int)event_id);
    xTaskCreate((TaskFunction_t)mqtt_event_handler_cb, "mqtt_cb", 1024, event_data, tskIDLE_PRIORITY, NULL);
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://mqtt.eclipseprojects.io",
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

void blink_led(void){
    while(1){
        if(connected==false){
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
    }
    }


}

static void mqtt_app_task(void *pvParameters) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://mqtt.eclipseprojects.io",
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);

    while (1) {
        if (connected) {
            float random_decibel = 30.0 + ((float)rand() / RAND_MAX) * (90.0 - 30.0);
            time_t now = time(NULL);
            struct tm *localTime = localtime(&now);
            char timeString[100]; 
            strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", localTime);



            // Wyświetl adres MAC jako string
            ESP_LOGI("MAC_ADDRESS", "Adres MAC: %s", mac_str);

            // Utwórz temat MQTT dynamicznie z adresem MAC
            char topic_noise[128];
            snprintf(topic_noise, sizeof(topic_noise), "noise_detector_web/%s/noise_data", mac_str);

            char noise_data_json[512];
            snprintf(noise_data_json, sizeof(noise_data_json),
                     "{"
                     "\"sensor_id\": \"ESP32\","
                     "\"timestamp\": \"%s\","
                     "\"noise_data\": {"
                     "\"decibels\": %.1f,"
                     "\"status\": \"active\""
                     "}"
                     "}",
                     timeString,
                     random_decibel);

            // Publikacja danych na temat z adresem MAC
            esp_mqtt_client_publish(client, topic_noise, noise_data_json, 0, 1, 0);

            char topic_sensor_info[128];
            snprintf(topic_sensor_info, sizeof(topic_sensor_info), "noise_detector_web/%s/sensor_info", mac_str);

            char sensor_info_json[512];
            snprintf(sensor_info_json, sizeof(sensor_info_json),
                     "{"
                     "\"sensor_id\": \"ESP32\","
                     "\"location\": {"
                     "\"city\": \"Krakow\","
                     "\"street\": \"al. Adama Mickiewicza 30\","
                     "\"latitude\": 50.064470,"
                     "\"longitude\": 19.923290"
                     "},"
                     "\"additional_info\": {"
                     "\"sensor_health\": \"good\""
                     "}"
                     "}");

            // Publikacja informacji o sensorze na temat z adresem MAC
            esp_mqtt_client_publish(client, topic_sensor_info, sensor_info_json, 0, 1, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(15000)); // Delay 15 seconds
    }
}


void app_main(void) {
    nvs_flash_init();         // Inicjalizacja pamięci NVS
    esp_rom_gpio_pad_select_gpio(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT); 
    xTaskCreate(blink_led, "blink_led", 1024, NULL, tskIDLE_PRIORITY, NULL);
    wifi_init_sta();          // Inicjalizacja i połączenie z WiFi 
    //wifi_init_ap();
    uint8_t mac_addr[6];
    esp_read_mac(mac_addr, ESP_MAC_WIFI_STA);
    snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X", 
                mac_addr[0], mac_addr[1], mac_addr[2], 
                mac_addr[3], mac_addr[4], mac_addr[5]);
    printf("%s\n", mac_str);

}




