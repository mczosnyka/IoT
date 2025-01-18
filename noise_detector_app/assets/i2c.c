#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "nvs_flash.h"      
#include "esp_system.h"
#include "ssd1306.c"
#include "common_header.h"

// #include "user_input.c"


#define CONFIG_SDA_GPIO 21
#define CONFIG_SCL_GPIO 22
#define EXTERNAL_BUTTON_GPIO GPIO_NUM_18
#define LED_GPIO GPIO_NUM_4


//MIKROFONSTWO
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_log.h"
#include <math.h>

#define SAMPLE_COUNT 100
#define NOISE_THRESHOLD 1350
#define REFERENCE_VOLTAGE 3.3f
#define ADC_VREF 3.6f
#define ADC_MAX 4095.0f
#define ADC_CHANNEL ADC_CHANNEL_7



bool display_noise = true;

SSD1306_t dev;
static adc_oneshot_unit_handle_t adc1_handle;

void setup_adc(void) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));
}

float measure_noise() {
    uint32_t sum = 0;
    int valid_samples = 0;
    int adc_raw;

    // Sample collection
    for(int i = 0; i < SAMPLE_COUNT; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &adc_raw));
        if(adc_raw > NOISE_THRESHOLD) {
            sum += adc_raw;
            valid_samples++;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Process readings
    if (valid_samples > 0) {
        float avg_reading = (float)sum / valid_samples;
        decibel = 96.66*log(avg_reading)-657.48;
        avg_decibel *= measurement_count;
        measurement_count++;
        avg_decibel += decibel;
        avg_decibel /= measurement_count;  
        mqtt_decibel += decibel;
        mqtt_divider++;
        if(decibel > mqtt_max_decibel){
            mqtt_max_decibel = decibel;
        }
        // Debug output
        ESP_LOGI(TAG, "AVG Reading: %.2f, Decibel: %.2fdB",
                 avg_reading, decibel);
        return decibel;
    } 
    else {
        ESP_LOGW(TAG, "No valid samples collected");
    }
    return 0;
}

void noise_task(void *param) {
    bool noise_displayed = false;
    gpio_config_t led_conf = {
    .pin_bit_mask = (1ULL << LED_GPIO),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);
    while (1) {
        if(display_noise){
            if(!noise_displayed){
                ssd1306_clear_screen(&dev, inverted_colors);
                noise_displayed = true;
            }
            ssd1306_display_text(&dev, 0, "Current noise:", strlen("Current noise:"), inverted_colors, xoffset);
            ssd1306_display_text(&dev, 2, "Average noise:", strlen("Average noise:"), inverted_colors, xoffset);
            float db = measure_noise();
            char display_text[50];
            snprintf(display_text, sizeof(display_text), "%.1f dB ", db);
            ssd1306_display_text(&dev, 1, display_text, strlen(display_text), inverted_colors, xoffset);
            char display_text2[50];
            snprintf(display_text2, sizeof(display_text2), "%.1f dB ", avg_decibel);
            ssd1306_display_text(&dev, 3, display_text2, strlen(display_text2), inverted_colors, xoffset);
            if (db > treshold_db) {
                gpio_set_level(LED_GPIO, 1); // Turn LED on
            } else {
                gpio_set_level(LED_GPIO, 0); // Turn LED off
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else{
            if(noise_displayed){
                ssd1306_clear_screen(&dev, inverted_colors);
                noise_displayed = false;
            }
            ssd1306_display_text(&dev, 0, "MAC ADDRESS:", strlen("MAC ADDRESS:"), inverted_colors, xoffset);
            ssd1306_display_text(&dev, 1, mac_str, strlen(mac_str), inverted_colors, xoffset);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

    }
}

void button_task(void *param) {
        gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << EXTERNAL_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    while (1) {
        int button_state = gpio_get_level(EXTERNAL_BUTTON_GPIO);
        if (button_state == 0) {
            display_noise = !display_noise;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


// void app_main(void)
// {
//     nvs_flash_init();
//     bool inverted_colors = false;
//     int contrast = 255;
//     int yoffset = 0;
//     int xoffset = 0;
//     // get_user_input(&inverted_colors, &contrast, &yoffset, &xoffset);
// 	i2c_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO);
//     ssd1306_init(&dev);
//     ssd1306_clear_screen(&dev, inverted_colors);
//     ssd1306_contrast(&dev, contrast);
//     ssd1306_yoffset(&dev, yoffset);
//     ssd1306_display_text(&dev, 0, "Noise level:", strlen("Noise level:"), inverted_colors, xoffset);
//     setup_adc();

//     //BUTTON?
//     gpio_config_t io_conf = {
//         .pin_bit_mask = (1ULL << EXTERNAL_BUTTON_GPIO),
//         .mode = GPIO_MODE_INPUT,
//         .pull_up_en = GPIO_PULLUP_ENABLE,
//         .pull_down_en = GPIO_PULLDOWN_DISABLE,
//         .intr_type = GPIO_INTR_DISABLE
//     };
//     gpio_config(&io_conf);

//     gpio_config_t led_conf = {
//     .pin_bit_mask = (1ULL << LED_GPIO),
//     .mode = GPIO_MODE_OUTPUT,
//     .pull_up_en = GPIO_PULLUP_DISABLE,
//     .pull_down_en = GPIO_PULLDOWN_DISABLE,
//     .intr_type = GPIO_INTR_DISABLE
// };
// gpio_config(&led_conf);

//     xTaskCreatePinnedToCore(noise_task, "NoiseTask", 4096, &dev, 5, NULL, 0);
//     xTaskCreatePinnedToCore(button_task, "ButtonTask", 2048, NULL, 5, NULL, 0);




// }