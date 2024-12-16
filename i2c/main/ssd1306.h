#include "driver/i2c_master.h"
#include "font8x8_basic.h"

#define TAG "SSD1306_t"
#define I2C_ADDRESS 0x3C

typedef struct {
uint8_t _segs[128];
} PAGE_t;

typedef struct {
int _address;
int _width;
int _height;
int _pages;
PAGE_t _page[8];
i2c_port_t _i2c_num;
i2c_master_bus_handle_t _i2c_bus_handle;
i2c_master_dev_handle_t _i2c_dev_handle;
} SSD1306_t;


void i2c_init(SSD1306_t * dev, int16_t sda, int16_t scl);
void ssd1306_init(SSD1306_t * dev);
void ssd1306_clear_screen(SSD1306_t * dev, bool invert);
void ssd1306_display_text(SSD1306_t * dev, int page, char * text, int text_len, bool invert, int seg);
void i2c_display_image(SSD1306_t * dev, int page, int seg, uint8_t * images, int width);
void ssd1306_invert(uint8_t *buf, size_t blen);
void ssd1306_contrast(SSD1306_t * dev, int contrast);
void ssd1306_yoffset(SSD1306_t * dev, int xoffset);
