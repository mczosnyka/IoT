#include <ssd1306.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "nvs_flash.h"      
#include "esp_system.h"
#include "user_input.c"

#define CONFIG_SDA_GPIO 21
#define CONFIG_SCL_GPIO 22




void app_main(void)
{
    nvs_flash_init();
    SSD1306_t dev;
    bool inverted_colors = false;
    int contrast = 255;
    int yoffset = 0;
    int xoffset = 0;
    get_user_input(&inverted_colors, &contrast, &yoffset, &xoffset);
	i2c_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO);
    ssd1306_init(&dev);
    ssd1306_clear_screen(&dev, inverted_colors);
    ssd1306_contrast(&dev, contrast);
    ssd1306_yoffset(&dev, yoffset);
    ssd1306_display_text(&dev, 0, "Noise level:", strlen("Noise level:"), inverted_colors, xoffset);
    
    while(true){
        float random_decibel = 30.0 + ((float)rand() / RAND_MAX) * (90.0 - 30.0);
        char display_text[50];
        snprintf(display_text, sizeof(display_text), "%.1f dB", random_decibel);
        ssd1306_display_text(&dev, 1, display_text, strlen(display_text), inverted_colors, xoffset+24);
        vTaskDelay(500);
    }





}