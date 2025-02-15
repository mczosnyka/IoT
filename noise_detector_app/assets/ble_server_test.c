
#include "common_header.h"


///Declare the static function
static void gatts_profile_main_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void add_char(void *param);

#define GATTS_SERVICE_UUID   0x1820
#define SSID_CHAR_UUID      0x2A8A
#define PASSWORD_CHAR_UUID  0x2A90  
#define GATTS_SERVICE_HANDLE_NUM     7


#define SSID_KEY "ssid_key"
#define PASSWORD_KEY "password_key"

#define BUTTON_GPIO GPIO_NUM_0

#define DEVICE_NAME            "Noise Detector"
#define TEST_MANUFACTURER_DATA_LEN  17

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40

#define PREPARE_BUF_MAX_SIZE 1024



static uint16_t ssid_char_handle = 0;
static uint16_t password_char_handle = 0;

static bool is_configuration_mode = false;



esp_bt_uuid_t ssid_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = SSID_CHAR_UUID
    } 
};

esp_bt_uuid_t password_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = PASSWORD_CHAR_UUID
    },

};

esp_attr_value_t ssid_attr_value = {
    .attr_max_len = MAX_SSID_LEN,
    .attr_len = sizeof(ssid_value),
    .attr_value = (uint8_t*) ssid_value,
};

static uint8_t char1_str[] = {0x11,0x22,0x33};
static esp_gatt_char_prop_t method_property = 0;

bool password_changed = 0;
bool ssid_changed = 0;

static uint8_t adv_config_done = 0;
#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

#ifdef CONFIG_SET_RAW_ADV_DATA
static uint8_t raw_adv_data[] = {
        0x02, 0x01, 0x06,                  // Length 2, Data Type 1 (Flags), Data 1 (LE General Discoverable Mode, BR/EDR Not Supported)
        0x02, 0x0a, 0xeb,                  // Length 2, Data Type 10 (TX power level), Data 2 (-21)
        0x03, 0x03, 0xab, 0xcd,            // Length 3, Data Type 3 (Complete 16-bit Service UUIDs), Data 3 (UUID)
};
static uint8_t raw_scan_rsp_data[] = {     // Length 15, Data Type 9 (Complete Local Name), Data 1 (ESP_GATTS_DEMO)
        0x0f, 0x09, 0x45, 0x53, 0x50, 0x5f, 0x47, 0x41, 0x54, 0x54, 0x53, 0x5f, 0x44,
        0x45, 0x4d, 0x4f
};
#else

static uint8_t adv_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xEE, 0x00, 0x00, 0x00,
    //second uuid, 32bit, [12], [13], [14], [15] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

// The length of adv data must be less than 31 bytes
//static uint8_t test_manufacturer[TEST_MANUFACTURER_DATA_LEN] =  {0x12, 0x23, 0x45, 0x56};
//adv data
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data =  NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
// scan response data
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    //.min_interval = 0x0006,
    //.max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data =  NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

#endif /* CONFIG_SET_RAW_ADV_DATA */

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

#define PROFILE_NUM 1
#define PROFILE_APP_ID 0

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handles[2];
    esp_bt_uuid_t char_uuids[2];
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handles[2];
    esp_bt_uuid_t descr_uuids[2];
};

static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_ID] = {
        .gatts_cb = gatts_profile_main_event_handler,
        .gatts_if = ESP_GATT_IF_NONE, 
    }
};

typedef struct {
    uint8_t                 *prepare_buf;
    int                     prepare_len;
} prepare_type_env_t;

static prepare_type_env_t ssid_prepare_write_env;

void example_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);


void example_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param){
    esp_gatt_status_t status = ESP_GATT_OK;
    if (param->write.need_rsp){
        if (param->write.is_prep) {
            if(param->write.offset > PREPARE_BUF_MAX_SIZE) {
                status = ESP_GATT_INVALID_OFFSET;
            } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
                status = ESP_GATT_INVALID_ATTR_LEN;
            }
            if (status == ESP_GATT_OK && prepare_write_env->prepare_buf == NULL) {
                prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE*sizeof(uint8_t));
                prepare_write_env->prepare_len = 0;
                if (prepare_write_env->prepare_buf == NULL) {
                    ESP_LOGE(GATTS_TAG, "Gatt_server prep no mem");
                    status = ESP_GATT_NO_RESOURCES;
                }
            }

            esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
            if (gatt_rsp) {
                gatt_rsp->attr_value.len = param->write.len;
                gatt_rsp->attr_value.handle = param->write.handle;
                gatt_rsp->attr_value.offset = param->write.offset;
                gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
                memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
                esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
                if (response_err != ESP_OK) {
                    ESP_LOGE(GATTS_TAG, "Send response error\n");
                }
                free(gatt_rsp);
            } else {
                ESP_LOGE(GATTS_TAG, "malloc failed, no resource to send response error\n");
                status = ESP_GATT_NO_RESOURCES;
            }
            if (status != ESP_GATT_OK){
                return;
            }
            memcpy(prepare_write_env->prepare_buf + param->write.offset,
                   param->write.value,
                   param->write.len);
            prepare_write_env->prepare_len += param->write.len;

        } else {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
        }
    }
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
#ifdef CONFIG_SET_RAW_ADV_DATA
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done==0){
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done==0){
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
#else
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0){
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done == 0){
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
#endif
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTS_TAG, "Advertising start failed");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTS_TAG, "Advertising stop failed");
        } else {
            ESP_LOGI(GATTS_TAG, "Stop adv successfully");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(GATTS_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
        ESP_LOGI(GATTS_TAG, "packet length updated: rx = %d, tx = %d, status = %d",
                  param->pkt_data_length_cmpl.params.rx_len,
                  param->pkt_data_length_cmpl.params.tx_len,
                  param->pkt_data_length_cmpl.status);
        break;
    default:
        break;
    }
}

static void gatts_profile_main_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(GATTS_TAG, "REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
        gl_profile_tab[PROFILE_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID;

        esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(DEVICE_NAME);
        if (set_dev_name_ret){
            ESP_LOGE(GATTS_TAG, "set device name failed, error code = %x", set_dev_name_ret);
        }
        //config adv data
        esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
        if (ret){
            ESP_LOGE(GATTS_TAG, "config adv data failed, error code = %x", ret);
        }
        adv_config_done |= adv_config_flag;
        //config scan response data
        ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
        if (ret){
            ESP_LOGE(GATTS_TAG, "config scan response data failed, error code = %x", ret);
        }
        adv_config_done |= scan_rsp_config_flag;

        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_APP_ID].service_id, GATTS_SERVICE_HANDLE_NUM);
        break;

    case ESP_GATTS_READ_EVT: {
ESP_LOGI(GATTS_TAG, "GATT_READ_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d", 
                 param->read.conn_id, param->read.trans_id, param->read.handle);

        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        if (rsp.attr_value.handle == ssid_char_handle){
                size_t ssid_length = strlen(ssid_value);
                rsp.attr_value.len = ssid_length;
                memcpy(rsp.attr_value.value, ssid_value, ssid_length);
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
        break;
        }else if(rsp.attr_value.handle == password_char_handle){
            size_t password_length = strlen(password_value);
                rsp.attr_value.len = password_length;
                memcpy(rsp.attr_value.value, password_value, password_length);
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
        break;
        }else{
            break;
        }
        

        // Populate response with wifi_ssid value
        
    }
    case ESP_GATTS_WRITE_EVT: {
        if(!is_configuration_mode) {
            ESP_LOGI(GATTS_TAG, "Device is not in configuration mode. Press button");
            example_write_event_env(gatts_if, &ssid_prepare_write_env, param);
            break;
        }
        ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d", param->write.conn_id, param->write.trans_id, param->write.handle);
        if (!param->write.is_prep) {
            ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value : %s", param->write.len, param->write.value);
            if (param->write.handle == ssid_char_handle && param->write.len < MAX_SSID_LEN) {
                for(int i = 0; i < param->write.len; i++) {
                    ssid_value[i] = param->write.value[i];
                }
                ssid_value[param->write.len] = '\0';
                ssid_changed = 1;
                ESP_LOGI(GATTS_TAG, "SSID written successfully: %s", ssid_value);
            }
            else if (param->write.handle == password_char_handle && param->write.len < MAX_PASSWORD_LEN) {
                for(int i = 0; i < param->write.len; i++) {
                    password_value[i] = param->write.value[i];
                }
                password_value[param->write.len] = '\0';
                password_changed = 1;
                ESP_LOGI(GATTS_TAG, "Password written successfully: %s", password_value);
            }

            else if (gl_profile_tab[PROFILE_APP_ID].descr_handles[0] == param->write.handle && param->write.len == 2){
                uint16_t descr_value = param->write.value[1]<<8 | param->write.value[0];
                if (descr_value == 0x0001){
                    if (method_property & ESP_GATT_CHAR_PROP_BIT_NOTIFY){
                        ESP_LOGI(GATTS_TAG, "notify enable");
                        uint8_t notify_data[15];
                        for (int i = 0; i < sizeof(notify_data); ++i)
                        {
                            notify_data[i] = i%0xff;
                        }
                        //the size of notify_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab[PROFILE_APP_ID].char_handles[0],
                                                sizeof(notify_data), notify_data, false);
                    }
                }else if (descr_value == 0x0002){
                    if (method_property & ESP_GATT_CHAR_PROP_BIT_INDICATE){
                        ESP_LOGI(GATTS_TAG, "indicate enable");
                        uint8_t indicate_data[15];
                        for (int i = 0; i < sizeof(indicate_data); ++i)
                        {
                            indicate_data[i] = i%0xff;
                        }
                        //the size of indicate_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab[PROFILE_APP_ID].char_handles[0],
                                                sizeof(indicate_data), indicate_data, true);
                    }
                }
                else if (descr_value == 0x0000){
                    ESP_LOGI(GATTS_TAG, "notify/indicate disable ");
                }else{
                    ESP_LOGE(GATTS_TAG, "unknown descr value");
                }
            }
            else if(gl_profile_tab[PROFILE_APP_ID].descr_handles[1] == param->write.handle && param->write.len == 2) {
                uint16_t descr_value = param->write.value[1]<<8 | param->write.value[0];
                if (descr_value == 0x0001){
                    if (method_property & ESP_GATT_CHAR_PROP_BIT_NOTIFY){
                        ESP_LOGI(GATTS_TAG, "notify enable");
                        uint8_t notify_data[15];
                        for (int i = 0; i < sizeof(notify_data); ++i)
                        {
                            notify_data[i] = i%0xff;
                        }
                        //the size of notify_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab[PROFILE_APP_ID].char_handles[0],
                                                sizeof(notify_data), notify_data, false);
                    }
                }else if (descr_value == 0x0002){
                    if (method_property & ESP_GATT_CHAR_PROP_BIT_INDICATE){
                        ESP_LOGI(GATTS_TAG, "indicate enable");
                        uint8_t indicate_data[15];
                        for (int i = 0; i < sizeof(indicate_data); ++i)
                        {
                            indicate_data[i] = i%0xff;
                        }
                        //the size of indicate_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab[PROFILE_APP_ID].char_handles[1],
                                                sizeof(indicate_data), indicate_data, true);
                    }
                }
                else if (descr_value == 0x0000){
                    ESP_LOGI(GATTS_TAG, "notify/indicate disable ");
                }else{
                    ESP_LOGE(GATTS_TAG, "unknown descr value");
                }
            }
        }
        example_write_event_env(gatts_if, &ssid_prepare_write_env, param);
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(GATTS_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(GATTS_TAG, "CREATE_SERVICE_EVT, status %d,  service_handle %d", param->create.status, param->create.service_handle);
        gl_profile_tab[PROFILE_APP_ID].service_handle = param->create.service_handle;
        gl_profile_tab[PROFILE_APP_ID].char_uuids[0] = ssid_uuid;
        gl_profile_tab[PROFILE_APP_ID].char_uuids[1] = password_uuid;

        esp_ble_gatts_start_service(gl_profile_tab[PROFILE_APP_ID].service_handle);

        int num_of_char_ssid = 0;
        int num_of_char_password = 1;
        xTaskCreate(add_char, "adding_char_ssid", 2048, (void*)&num_of_char_ssid, 5, NULL);
        vTaskDelay(pdMS_TO_TICKS(100));
        xTaskCreate(add_char, "adding_char_pass", 2048, (void*)&num_of_char_password, 5, NULL);
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT: {
        ESP_LOGI(GATTS_TAG, "ADD_CHAR_EVT, status %d,  attr_handle %d, service_handle %d",
                param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
        if(param->add_char.char_uuid.uuid.uuid16 == SSID_CHAR_UUID) {
            if(param->add_char.status != 0) {
                int num_of_char_ssid = 0;
                xTaskCreate(add_char, "adding_char_ssid", 2048, (void*)&num_of_char_ssid, 5, NULL);
                break;
            }
            gl_profile_tab[PROFILE_APP_ID].char_handles[0] = param->add_char.attr_handle;
            ssid_char_handle = param->add_char.attr_handle;

            uint16_t length = 0;
            const uint8_t* prf_char;
            esp_err_t get_attr_ret = esp_ble_gatts_get_attr_value(param->add_char.attr_handle,  &length, &prf_char);
            if (get_attr_ret == ESP_FAIL){
                ESP_LOGE(GATTS_TAG, "ILLEGAL HANDLE");
            }

            esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_APP_ID].service_handle, &gl_profile_tab[PROFILE_APP_ID].descr_uuids[0],
                                                                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
            if (add_descr_ret){
                ESP_LOGE(GATTS_TAG, "add char descr failed, error code =%x", add_descr_ret);
            }

        }
        else if(param->add_char.char_uuid.uuid.uuid16 == PASSWORD_CHAR_UUID) {
            if(param->add_char.status != 0) {
                int num_of_char_password = 1;   
                xTaskCreate(add_char, "adding_char_pass", 2048, (void*)&num_of_char_password, 5, NULL);
                break;
            }
            gl_profile_tab[PROFILE_APP_ID].char_handles[1] = param->add_char.attr_handle;
            password_char_handle = param->add_char.attr_handle;
            
        }
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        if(param->add_char.attr_handle == ssid_char_handle) {
            gl_profile_tab[PROFILE_APP_ID].descr_handles[0] = param->add_char_descr.attr_handle;
        }
        else if(param->add_char.attr_handle == password_char_handle) {
            gl_profile_tab[PROFILE_APP_ID].descr_handles[1] = param->add_char_descr.attr_handle;
        }

        ESP_LOGI(GATTS_TAG, "ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d",
                 param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGI(GATTS_TAG, "SERVICE_START_EVT, status %d, service_handle %d",
                 param->start.status, param->start.service_handle);
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT: {
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
        ESP_LOGI(GATTS_TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
        gl_profile_tab[PROFILE_APP_ID].conn_id = param->connect.conn_id;
        //start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GATTS_CONF_EVT:
        ESP_LOGI(GATTS_TAG, "ESP_GATTS_CONF_EVT, status %d attr_handle %d", param->conf.status, param->conf.handle);
        if (param->conf.status != ESP_GATT_OK){
            esp_log_buffer_hex(GATTS_TAG, param->conf.value, param->conf.len);
        }
        break;
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        break;
    }
}

void add_char(void *param) {
    int* num_of_char = (int*) param;
    method_property = ESP_GATT_CHAR_PROP_BIT_WRITE;
    esp_err_t add_char_ret = esp_ble_gatts_add_char(gl_profile_tab[PROFILE_APP_ID].service_handle, &gl_profile_tab[PROFILE_APP_ID].char_uuids[*num_of_char],
                                                    ESP_GATT_PERM_WRITE | ESP_GATT_PERM_READ,
                                                    method_property | ESP_GATT_CHAR_PROP_BIT_READ,
                                                    &ssid_attr_value, NULL);
    if (add_char_ret){
        ESP_LOGE(GATTS_TAG, "add char failed, error code =%x",add_char_ret);
    }
    else {
        ESP_LOGI(GATTS_TAG, "successfully added char %d", *num_of_char);
    }

    vTaskDelete(NULL);
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
        } else {
            ESP_LOGI(GATTS_TAG, "Reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gatts_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gatts_if == gl_profile_tab[idx].gatts_if) {
                if (gl_profile_tab[idx].gatts_cb) {
                    gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}


void configure_button() {
    esp_rom_gpio_pad_select_gpio(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);
}

void save_wifi_credentials(void* param) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    password_changed = 0;
    ssid_changed = 0;
    ESP_LOGI(GATTS_TAG, "Waiting for wi-fi credentials...");
    while(is_configuration_mode) {
        err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE("NVS", "Failed to open NVS handle!");
            break;
        }

        // Zapis SSID
        err = nvs_set_str(nvs_handle, SSID_KEY, ssid_value);
        if (err != ESP_OK) {
            nvs_close(nvs_handle);
            break;
        }

        // Zapis hasła
        err = nvs_set_str(nvs_handle, PASSWORD_KEY, password_value);
        if (err != ESP_OK) {
            nvs_close(nvs_handle);
            break;
        }

        // Zapisanie zmian w pamięci
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            nvs_close(nvs_handle);
            break;
        }

        nvs_close(nvs_handle);
        if(password_changed == 1 && ssid_changed == 1) {
            ESP_LOGI("NVS", "Both Wi-Fi credentials saved successfully.");
            vTaskDelay(pdMS_TO_TICKS(2000));
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI("NVS", "Leaving configuration mode.");
    is_configuration_mode = false;
    vTaskDelete(NULL);
}


void set_configuration_mode(void *param) {
    while(true) {
        if(!is_configuration_mode) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        xTaskCreate(save_wifi_credentials, "save_wifi_task", 4096, NULL, 5, NULL);
        vTaskDelete(NULL);
        break;
    }
}

void read_nvs_data() {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(GATTS_TAG, "Nie udało się otworzyć NVS: %s", esp_err_to_name(err));
        return;
    }
    char ssid[MAX_SSID_LEN];
    size_t ssid_len = sizeof(ssid);
    char password[MAX_PASSWORD_LEN];
    size_t password_len = sizeof(password);
    err = nvs_get_str(nvs_handle, SSID_KEY, ssid, &ssid_len);
    if (err == ESP_OK) {
        strcpy(ssid_value, ssid);
        ESP_LOGI(GATTS_TAG, "Current SSID: %s", ssid);
    } else {
        ESP_LOGE(GATTS_TAG, "Unable to read SSID: %s", esp_err_to_name(err));
    }
    err = nvs_get_str(nvs_handle, PASSWORD_KEY, password, &password_len);
    if (err == ESP_OK) {
        strcpy(password_value, password);
        ESP_LOGI(GATTS_TAG, "Current password: %s", password);
    } else {
        ESP_LOGE(GATTS_TAG, "Unable to read password: %s", esp_err_to_name(err));
    }
    nvs_close(nvs_handle);
}

// void app_main(void)
// {
//     esp_err_t ret;

//     ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK( ret );
//     read_nvs_data();
//     ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

//     esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
//     ret = esp_bt_controller_init(&bt_cfg);
//     if (ret) {
//         ESP_LOGE(GATTS_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
//         return;
//     }

//     ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
//     if (ret) {
//         ESP_LOGE(GATTS_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
//         return;
//     }

//     ret = esp_bluedroid_init();
//     if (ret) {
//         ESP_LOGE(GATTS_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
//         return;
//     }
//     ret = esp_bluedroid_enable();
//     if (ret) {
//         ESP_LOGE(GATTS_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
//         return;
//     }

//     ret = esp_ble_gatts_register_callback(gatts_event_handler);
//     if (ret){
//         ESP_LOGE(GATTS_TAG, "gatts register error, error code = %x", ret);
//         return;
//     }
//     ret = esp_ble_gap_register_callback(gap_event_handler);
//     if (ret){
//         ESP_LOGE(GATTS_TAG, "gap register error, error code = %x", ret);
//         return;
//     }
//     ret = esp_ble_gatts_app_register(PROFILE_APP_ID);
//     if (ret){
//         ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
//         return;
//     }

//     esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
//     if (local_mtu_ret){
//         ESP_LOGE(GATTS_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
//     }
//     configure_button();
//     xTaskCreate(listen_for_configuration_mode_button, "listen_for_configuration_mode_button", 2048, NULL, tskIDLE_PRIORITY, NULL);
//     return;
// }