#include "ssd1306.h"
#include "esp_log.h"
#include "string.h"
#define I2C_MASTER_FREQ_HZ 400000 // I2C clock of SSD1306 can run at 400 kHz max.
#define I2C_TICKS_TO_WAIT 100	  // Maximum ticks to wait before issuing a timeout.
#if CONFIG_I2C_PORT_0
#define I2C_NUM I2C_NUM_0
#else 
#define I2C_NUM I2C_NUM_1
#endif


void i2c_master_init(SSD1306_t * dev, int16_t sda, int16_t scl)
{
	i2c_master_bus_config_t i2c_mst_config = {
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.i2c_port = I2C_NUM,
		.scl_io_num = scl,
		.sda_io_num = sda,
		.flags.enable_internal_pullup = true,
	};
	i2c_master_bus_handle_t i2c_bus_handle;
	ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));

	i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = I2C_ADDRESS,
		.scl_speed_hz = I2C_MASTER_FREQ_HZ,
	};
	i2c_master_dev_handle_t i2c_dev_handle;
	ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &i2c_dev_handle));


	dev->_address = I2C_ADDRESS;
	dev->_i2c_num = I2C_NUM;
	dev->_i2c_bus_handle = i2c_bus_handle;
	dev->_i2c_dev_handle = i2c_dev_handle;
}


void i2c_init(SSD1306_t * dev, int width, int height) {
	dev->_width = width;
	dev->_height = height;
	dev->_pages = 8;	
	uint8_t out_buf[27];
	int out_index = 0;
	out_buf[out_index++] = OLED_CONTROL_BYTE_CMD_STREAM;
	out_buf[out_index++] = OLED_CMD_DISPLAY_OFF;				
	out_buf[out_index++] = OLED_CMD_SET_MUX_RATIO;			 
	out_buf[out_index++] = 0x3F;
	out_buf[out_index++] = OLED_CMD_SET_DISPLAY_OFFSET;		 
	out_buf[out_index++] = 0x00;
	out_buf[out_index++] = OLED_CMD_SET_DISPLAY_START_LINE;	
	out_buf[out_index++] = OLED_CMD_SET_SEGMENT_REMAP_1;	
	out_buf[out_index++] = OLED_CMD_SET_COM_SCAN_MODE;		
	out_buf[out_index++] = OLED_CMD_SET_DISPLAY_CLK_DIV;	
	out_buf[out_index++] = 0x80;
	out_buf[out_index++] = OLED_CMD_SET_COM_PIN_MAP;		
    out_buf[out_index++] = 0x12;
	out_buf[out_index++] = 0x01;			//OLED_CMD_SET_CONTRAST
	out_buf[out_index++] = 0xFF;
	out_buf[out_index++] = OLED_CMD_DISPLAY_RAM;			
	out_buf[out_index++] = OLED_CMD_SET_VCOMH_DESELCT;		
	out_buf[out_index++] = 0x40;
	out_buf[out_index++] = OLED_CMD_SET_MEMORY_ADDR_MODE;	
	out_buf[out_index++] = OLED_CMD_SET_PAGE_ADDR_MODE;		
	out_buf[out_index++] = 0x10;
	out_buf[out_index++] = OLED_CMD_SET_CHARGE_PUMP;			
	out_buf[out_index++] = 0x14;
	out_buf[out_index++] = OLED_CMD_DEACTIVE_SCROLL;			
	out_buf[out_index++] = OLED_CMD_DISPLAY_NORMAL;			
	out_buf[out_index++] = OLED_CMD_DISPLAY_ON;				

	esp_err_t res;
	res = i2c_master_transmit(dev->_i2c_dev_handle, out_buf, out_index, I2C_TICKS_TO_WAIT);
	if (res == ESP_OK) {
		ESP_LOGI(TAG, "OLED configured successfully");
	} else {
		ESP_LOGE(TAG, "Could not write to device [0x%02x at %d]: %d (%s)", dev->_address, dev->_i2c_num, res, esp_err_to_name(res));
	}
    	for (int i=0;i<dev->_pages;i++) {
		memset(dev->_page[i]._segs, 0, 128);
	}
}

void ssd1306_clear_screen(SSD1306_t * dev, bool invert)
{
	char space[16];
	memset(space, 0x00, sizeof(space));
	for (int page = 0; page < dev->_pages; page++) {
		ssd1306_display_text(dev, page, space, sizeof(space), invert);
	}
}

void ssd1306_contrast(SSD1306_t * dev, int contrast) {
	uint8_t _contrast = contrast;
	if (contrast < 0x0) _contrast = 0;
	if (contrast > 0xFF) _contrast = 0xFF;

	uint8_t out_buf[3];
	int out_index = 0;
	out_buf[out_index++] = OLED_CONTROL_BYTE_CMD_STREAM; // 00
	out_buf[out_index++] = 0x81; 
	out_buf[out_index++] = _contrast;

	esp_err_t res = i2c_master_transmit(dev->_i2c_dev_handle, out_buf, 3, I2C_TICKS_TO_WAIT);
	if (res != ESP_OK)
		ESP_LOGE(TAG, "Could not write to device [0x%02x at %d]: %d (%s)", dev->_address, dev->_i2c_num, res, esp_err_to_name(res));
}

void ssd1306_display_text(SSD1306_t * dev, int page, char * text, int text_len, bool invert)
{
	if (page >= dev->_pages) return;
	int _text_len = text_len;
	if (_text_len > 16) _text_len = 16;

	int seg = 0;
	uint8_t image[8];
	for (int i = 0; i < _text_len; i++) {
		memcpy(image, font8x8_basic_tr[(uint8_t)text[i]], 8);
		if (invert) ssd1306_invert(image, 8);
		i2c_display_image(dev, page, seg, image, 8);
		seg = seg + 8;
	}
}

void i2c_display_image(SSD1306_t * dev, int page, int seg, uint8_t * images, int width) {
	if (page >= dev->_pages) return;
	if (seg >= dev->_width) return;

	int _seg = seg + 0; // 0 to config_offsetx
	uint8_t columLow = _seg & 0x0F;
	uint8_t columHigh = (_seg >> 4) & 0x0F;

	int _page = page;

	uint8_t *out_buf;
	out_buf = malloc(width < 4 ? 4 : width + 1);
	if (out_buf == NULL) {
		ESP_LOGE(TAG, "malloc fail");
		return;
	}
	int out_index = 0;
	out_buf[out_index++] = OLED_CONTROL_BYTE_CMD_STREAM;
	// Set Lower Column Start Address for Page Addressing Mode
	out_buf[out_index++] = (0x00 + columLow);
	// Set Higher Column Start Address for Page Addressing Mode
	out_buf[out_index++] = (0x10 + columHigh);
	// Set Page Start Address for Page Addressing Mode
	out_buf[out_index++] = 0xB0 | _page;

	esp_err_t res;
	res = i2c_master_transmit(dev->_i2c_dev_handle, out_buf, out_index, I2C_TICKS_TO_WAIT);
	if (res != ESP_OK)
		ESP_LOGE(TAG, "Could not write to device [0x%02x at %d]: %d (%s)", dev->_address, dev->_i2c_num, res, esp_err_to_name(res));

	out_buf[0] = OLED_CONTROL_BYTE_DATA_STREAM;
	memcpy(&out_buf[1], images, width);

	res = i2c_master_transmit(dev->_i2c_dev_handle, out_buf, width + 1, I2C_TICKS_TO_WAIT);
	if (res != ESP_OK)
		ESP_LOGE(TAG, "Could not write to device [0x%02x at %d]: %d (%s)", dev->_address, dev->_i2c_num, res, esp_err_to_name(res));
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


uint8_t ssd1306_rotate_byte(uint8_t ch1) {
	uint8_t ch2 = 0;
	for (int j=0;j<8;j++) {
		ch2 = (ch2 << 1) + (ch1 & 0x01);
		ch1 = ch1 >> 1;
	}
	return ch2;
}