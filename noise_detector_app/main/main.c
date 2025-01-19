#include <stdio.h>
#include "../assets/wifi.c"
#include "../assets/ble_server_test.c"
#include "../assets/i2c.c"
// laczymy sie z wifi, czekamy na tryb konfiguracyjny, zaczynamy mierzyc i wyswietlac
// jak sie polaczymy z wifi, zaczynamy wysylac na mqtt i pobierac z app info konfiguracyjne
// guzik ma cos robic XD sigma skibidi mtv vip
 void listen_for_configuration_mode_button(void* param) {
    uint8_t last_button_state = 1;
    int i = 0;
    while(i<100) {
        uint8_t current_button_state = gpio_get_level(BUTTON_GPIO);
        if(last_button_state == 1 && current_button_state == 0) {
            is_configuration_mode = !is_configuration_mode;
            if(is_configuration_mode) {
                ESP_LOGI(GATTS_TAG, "You are in configuration mode. Press again to leave");
                i = 0;
                xTaskCreate(save_wifi_credentials, "save_wifi_task", 4096, NULL, 5, NULL);
            }
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        else{
            if(!is_configuration_mode){
                i++;
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }
    }
    ESP_LOGI(GATTS_TAG, "Time to configure the device has ended.");
    ESP_LOGI(GATTS_TAG, "Initializing wifi connection...");
    wifi_init_sta();
    xTaskCreate(blink_led, "blink_led", 4096, NULL, tskIDLE_PRIORITY, NULL);
    vTaskDelete(NULL);
}

void change_configuration(void* param){
    int prev_contrast = contrast;
    int prev_yoffset = yoffset;
    bool prev_inversion = inverted_colors;
    int prev_xoffset = xoffset;
    while(1){
        if(contrast != prev_contrast){
            ssd1306_clear_screen(&dev, inverted_colors);
            ssd1306_contrast(&dev, contrast);
            prev_contrast = contrast;
        }
        if(yoffset != prev_yoffset){
            ssd1306_clear_screen(&dev, inverted_colors);
            ssd1306_yoffset(&dev, yoffset);
            prev_yoffset = yoffset;
        }
        if(prev_inversion != inverted_colors){
            ssd1306_clear_screen(&dev, inverted_colors);
            prev_inversion = inverted_colors;
        }
        if(xoffset != prev_xoffset){
            ssd1306_clear_screen(&dev, inverted_colors);
            prev_xoffset = xoffset;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
} 


void app_main(void) {
    esp_err_t ret;
    ret = nvs_flash_init(); 
    uint8_t mac_addr[6];
    esp_read_mac(mac_addr, ESP_MAC_WIFI_STA);
    snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X", 
                mac_addr[0], mac_addr[1], mac_addr[2], 
                mac_addr[3], mac_addr[4], mac_addr[5]);        
    //wifi_init_ap();
    xTaskCreate(change_configuration, "change_configuration", 4096, NULL, tskIDLE_PRIORITY, NULL);
    read_nvs_data();
    i2c_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO);
    ssd1306_init(&dev);
    ssd1306_clear_screen(&dev, inverted_colors);
    ssd1306_contrast(&dev, contrast);
    ssd1306_yoffset(&dev, yoffset);
    setup_adc();
    xTaskCreatePinnedToCore(noise_task, "NoiseTask", 4096, &dev, 5, NULL, 0);
    xTaskCreatePinnedToCore(button_task, "ButtonTask", 4096, NULL, 5, NULL, 0);
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    ret = esp_bluedroid_init();
    ret = esp_bluedroid_enable();
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    ret = esp_ble_gap_register_callback(gap_event_handler);
    ret = esp_ble_gatts_app_register(PROFILE_APP_ID);
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    configure_button();
    xTaskCreate(listen_for_configuration_mode_button, "listen_for_configuration_mode_button", 4096, NULL, tskIDLE_PRIORITY, NULL);



}