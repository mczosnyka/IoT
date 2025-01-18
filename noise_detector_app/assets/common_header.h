#ifndef COMMON_HEADER_H
#define COMMON_HEADER_H

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
#include <stdlib.h>
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"
#include "driver/gpio.h"
#include "sdkconfig.h"


#define GATTS_TAG "NOISE_DETECTOR"
#define STORAGE_NAMESPACE "WIFI_CONFIG"
#define DEFAULT_VALUE "default"
#define MAX_SSID_LEN 32 
#define MAX_PASSWORD_LEN 32 

static const char *TAG = "noise_detector";
char ssid_value[MAX_SSID_LEN] = DEFAULT_VALUE;
char password_value[MAX_PASSWORD_LEN] = DEFAULT_VALUE;
float decibel = 0.0f;
float avg_decibel = 0.0f;
int measurement_count = 0;
int mqtt_divider = 0;
float mqtt_decibel = 0.0f;
float mqtt_max_decibel = 0.0f;
char mac_str[13];
bool inverted_colors = false;
int contrast = 255;
int yoffset = 0;
int xoffset = 0;
float treshold_db = 60.0f;
#endif