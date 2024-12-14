#include <stdio.h>
#include <ssd1306.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "nvs_flash.h"

#define CONFIG_SDA_GPIO 21
#define CONFIG_SCL_GPIO 22
void app_main(void)
{
    nvs_flash_init();
    SSD1306_t dev;
// inicjalizacja master i2c
// inicjalizacja ssd1306 128x64
// czyszczenie ekranu
// kontrast ?
// wypisanie
	i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO);
    i2c_init(&dev, 128, 64);
    ssd1306_clear_screen(&dev, true);
    ssd1306_contrast(&dev, 0xff);
    ssd1306_display_text(&dev, 3, "  Noise level:", strlen("  Noise level:"), true);
    while(true){
        float random_decibel = 30.0 + ((float)rand() / RAND_MAX) * (90.0 - 30.0);
        char display_text[50];
        snprintf(display_text, sizeof(display_text), "     %.1f dB", random_decibel);
        ssd1306_display_text(&dev, 4, display_text, strlen(display_text), true);
        vTaskDelay(500);
    }


}
