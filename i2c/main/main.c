#include <stdio.h>
#include <ssd1306.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CONFIG_SDA_GPIO 21
#define CONFIG_SCL_GPIO 22
void app_main(void)
{
    SSD1306_t dev;
	int center, top, bottom;
	char lineChar[20];
// inicjalizacja master i2c
// inicjalizacja ssd1306 128x64
// czyszczenie ekranu
// kontrast ?
// wypisanie
	i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO);
    ssd1306_init(&dev, 128, 64);
    ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
    for(int i = 0; i<20; i++){
	    ssd1306_display_text(&dev, 0, "hello", 5, false);
        vTaskDelay(5000);
    }


}
