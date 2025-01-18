#include "common_header.h"
#define I2C_MASTER_FREQ_HZ 400000 
#define I2C_TICKS_TO_WAIT 100	  
#if CONFIG_I2C_PORT_0
#define I2C_NUM I2C_NUM_0
#else 
#define I2C_NUM I2C_NUM_1
#endif

#include "driver/i2c_master.h"
#include "font8x8_basic.h"  
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
void ssd1306_handle_display_text(SSD1306_t * dev, int page, int seg, uint8_t * images, int width);
void ssd1306_invert(uint8_t *buf, size_t blen);
void ssd1306_contrast(SSD1306_t * dev, int contrast);
void ssd1306_yoffset(SSD1306_t * dev, int yoffset);


// inicjalizacja magistrali i2c
void i2c_init(SSD1306_t * dev, int16_t sda, int16_t scl)
{
	//konfiguracja magistrali i2c
	i2c_master_bus_config_t i2c_mst_config = {
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.i2c_port = I2C_NUM,
		.scl_io_num = scl,
		.sda_io_num = sda,
		.flags.enable_internal_pullup = true,
	};

	//handle magistrali i2c
	i2c_master_bus_handle_t i2c_bus_handle;

	//inicjalizacja magistrali i2c
	ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));

	//konfiguracja urzadzenia i2c
	i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = I2C_ADDRESS,
		.scl_speed_hz = I2C_MASTER_FREQ_HZ,
	};

	//dodanie urządzenia do zainicjalizowanej komunikacji przez magistralę
	i2c_master_dev_handle_t i2c_dev_handle;
	ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &i2c_dev_handle));


	dev->_address = I2C_ADDRESS;
	dev->_i2c_num = I2C_NUM;
	dev->_i2c_bus_handle = i2c_bus_handle;
	dev->_i2c_dev_handle = i2c_dev_handle;
}


void ssd1306_init(SSD1306_t * dev) {
	dev->_width = 128;
	dev->_height = 64;
	dev->_pages = 8; // ssd1306 rozkłada ekran na 8 stron po 8 pikseli wysokości
	uint8_t out_buf[27]; // rozmiar tablicy z konfiguracją
	int out_index = 0;
	out_buf[out_index++] = 0x00; // CMD_STREAM wskazuje, że wysyłamy komendy
	out_buf[out_index++] = 0xAE; // wyłączenie wyświetlacza
	out_buf[out_index++] = 0xD3; // vertical offset - tutaj ustawiony na 0		 
	out_buf[out_index++] = 0x00;
	out_buf[out_index++] = 0x40; // początkowa linia wyświetlacza - (0x40-0x7F)
	out_buf[out_index++] = 0xA1; // remap segmentów - 0xA0 to domyślna wartość, 0xA1 to odwrócenie	
	out_buf[out_index++] = 0xC8; // skanowanie COM - 0xC0 to domyślna wartość, 0xC8 to odwrócenie
	out_buf[out_index++] = 0xD5; // ustawienie dzielnika zegara
	out_buf[out_index++] = 0x80;
	out_buf[out_index++] = 0xDA; // konfiguracja pinów COM 
    out_buf[out_index++] = 0x12;
	out_buf[out_index++] = 0x81; // ustawianie kontrastu
	out_buf[out_index++] = 0xFF;
	out_buf[out_index++] = 0xA4; // wyświetlanie pamięci RAM			
	out_buf[out_index++] = 0xDB; // ustawienie VCOMH 
	out_buf[out_index++] = 0x40;
	out_buf[out_index++] = 0x20; // ustawienie trybu adresowania pamięci
	out_buf[out_index++] = 0x02; // ustawienie pocztatkowego adresu w poziomie
	out_buf[out_index++] = 0x10; // 0x10 to początkowy adres kolumny
	out_buf[out_index++] = 0x8D; // charge pump - 0x8D to komenda, 0x14 to wartość			
	out_buf[out_index++] = 0x14;
	out_buf[out_index++] = 0x2E; // wyłączenie scrollowania				
	out_buf[out_index++] = 0xA6; // ustawienie normalnego wyświetlania (0xA7 - inwersja kolorow)	
	out_buf[out_index++] = 0xAF; // włączenie wyświetlacza			

	i2c_master_transmit(dev->_i2c_dev_handle, out_buf, out_index, I2C_TICKS_TO_WAIT);
    for (int i=0;i<dev->_pages;i++) {
	memset(dev->_page[i]._segs, 0, 128);
	}
}


void ssd1306_clear_screen(SSD1306_t * dev, bool invert)
{
	char space[16];
	memset(space, 0x00, sizeof(space));
	for (int page = 0; page < dev->_pages; page++) {
		ssd1306_display_text(dev, page, space, sizeof(space), invert, 0);
	}
}

void ssd1306_contrast(SSD1306_t * dev, int contrast) {
	uint8_t _contrast = contrast;
	if (contrast < 0x0) _contrast = 0;
	if (contrast > 0xFF) _contrast = 0xFF;

	uint8_t out_buf[3];
	int out_index = 0;
	out_buf[out_index++] = 0x00; 
	out_buf[out_index++] = 0x81; 
	out_buf[out_index++] = _contrast;

	i2c_master_transmit(dev->_i2c_dev_handle, out_buf, 3, I2C_TICKS_TO_WAIT);
}

void ssd1306_yoffset(SSD1306_t * dev, int yoffset) {
	uint8_t _yoffset = yoffset;
	if (yoffset < 0x0) _yoffset = 0;
	if (yoffset > 0x3F) _yoffset = 0x3F;
	_yoffset = 64-_yoffset;

	uint8_t out_buf[3];
	int out_index = 0;
	out_buf[out_index++] = 0x00;
	out_buf[out_index++] = 0xD3; 
	out_buf[out_index++] = _yoffset;

	i2c_master_transmit(dev->_i2c_dev_handle, out_buf, 3, I2C_TICKS_TO_WAIT);
}

void ssd1306_display_text(SSD1306_t * dev, int page, char * text, int text_len, bool invert, int seg)
{
	if (page >= dev->_pages) return;
	int _text_len = text_len;
	if (_text_len > 16) _text_len = 16;

	uint8_t image[8];
	for (int i = 0; i < _text_len; i++) {
		memcpy(image, font8x8_basic_tr[(uint8_t)text[i]], 8);
		if (invert) ssd1306_invert(image, 8);
		ssd1306_handle_display_text(dev, page, seg, image, 8);
		seg = seg + 8;
	}
}


void ssd1306_handle_display_text(SSD1306_t * dev, int page, int seg, uint8_t * images, int width) {
	if (page >= dev->_pages) return;
	if (seg >= dev->_width) return;

	int _seg = seg + 0; 
	uint8_t columLow = _seg & 0x0F;
	uint8_t columHigh = (_seg >> 4) & 0x0F;

	int _page = page;

	uint8_t *out_buf;
	out_buf = malloc(width < 4 ? 4 : width + 1);
	if (out_buf == NULL) {
		return;
	}
	int out_index = 0;
	out_buf[out_index++] = 0x00;
	out_buf[out_index++] = (0x00 + columLow);
	out_buf[out_index++] = (0x10 + columHigh);
	out_buf[out_index++] = 0xB0 | _page;

	i2c_master_transmit(dev->_i2c_dev_handle, out_buf, out_index, I2C_TICKS_TO_WAIT);
	
	out_buf[0] = 0x40;
	memcpy(&out_buf[1], images, width);

	i2c_master_transmit(dev->_i2c_dev_handle, out_buf, width + 1, I2C_TICKS_TO_WAIT);
	free(out_buf);
    memcpy(&dev->_page[page]._segs[seg], images, width);
}

void ssd1306_invert(uint8_t *buf, size_t blen)
{
	uint8_t wk;
	for(int i=0; i<blen; i++){
		wk = buf[i];
		buf[i] = ~wk;
	}
}
