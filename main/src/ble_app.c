#include "ble_app.h"

#include "ble_utils.h"
#include "config.h"
#include "debug.h"
#include "mqtt_timer.h"
#include "os.h"
#include "spi_app.h"
#include "signal_processing.h"

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_common_api.h>
#include <esp_gatts_api.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <string.h>

/* Global variables */
ble_status_t ble_status = BLE_STATUS_OFFLINE;
static char device_id[6 * 2 + 1] = {0};
static uint16_t gatts_if = ESP_GATT_IF_NONE;
static uint16_t conn_id = 0;
static bool is_connected = false;

/* Service and characteristic handles */
static uint16_t service_handle;
static uint16_t char_handle_sensor_data;
static uint16_t char_handle_sensor_status;
static uint16_t char_handle_calibration;
static uint16_t char_handle_timer_control;

/* BLE Advertisement data */
static uint8_t adv_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    // Service UUID: 6ba7b810-9dad-11d1-80b4-00c04fd430c8
    0xc8, 0x30, 0xd4, 0x4f, 0xc0, 0x00, 0xb4, 0x80, 0xd1, 0x11, 0xad, 0x9d, 0x10, 0xb8, 0xa7, 0x6b,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = BLE_ADV_INTERVAL_MIN,
    .max_interval = BLE_ADV_INTERVAL_MAX,
    .appearance = BLE_APPEARANCE,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = BLE_ADV_INTERVAL_MIN,
    .adv_int_max = BLE_ADV_INTERVAL_MAX,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* Forward declarations */
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void add_characteristics(void);

/* Device ID initialization */
void ble_device_id_init(void) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    sprintf(device_id, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    LOGI("BLE Device ID: %s", device_id);
}

char* ble_get_device_id(void) {
    return device_id;
}

/* BLE initialization */
void ble_app_init(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));
    
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name(BLE_DEVICE_NAME));
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));
    
    ble_device_id_init();
    LOGI("BLE initialization complete");
}

void ble_app_start(void) {
    ble_app_init();
    ble_start_advertising();
}

void ble_app_stop(void) {
    esp_ble_gap_stop_advertising();
    ble_status = BLE_STATUS_OFFLINE;
}

/* Data publishing functions */
bool ble_publish_sensor_data(const uint8_t sensor_id, const char* data) {
    if (!is_connected) {
        return false;
    }
    
    // Parse the data format: "timestamp/T,X,Y,Z"
    char *token;
    char data_copy[256];
    strncpy(data_copy, data, sizeof(data_copy) - 1);
    data_copy[sizeof(data_copy) - 1] = '\0';
    
    ble_sensor_data_t sensor_data = {0};
    sensor_data.sensor_id = sensor_id;
    
    // Parse timestamp
    token = strtok(data_copy, "/");
    if (token) {
        sensor_data.timestamp = strtoull(token, NULL, 10);
    }
    
    // Parse T,X,Y,Z (skip T - temperature)
    token = strtok(NULL, ",");
    // Skip temperature value - we read it but don't use it
    
    token = strtok(NULL, ",");
    if (token) sensor_data.X = atoi(token);
    
    token = strtok(NULL, ",");
    if (token) sensor_data.Y = atoi(token);
    
    token = strtok(NULL, ",");
    if (token) sensor_data.Z = atoi(token);
    
    // Send notification
    esp_err_t ret = esp_ble_gatts_send_indicate(gatts_if, conn_id, char_handle_sensor_data,
                                                sizeof(sensor_data), (uint8_t*)&sensor_data, false);
    
    static uint32_t last_log_time[NUM_OF_SPI_DEV] = {0};
    static int log_cnt[NUM_OF_SPI_DEV] = {0};
    if (get_ticks() - last_log_time[sensor_id] > ms_to_ticks(1000)) {
        LOGI("Publishing sensor %d data via BLE: %d times", sensor_id, log_cnt[sensor_id]);
        last_log_time[sensor_id] = get_ticks();
        log_cnt[sensor_id] = 0;
    } else {
        log_cnt[sensor_id]++;
    }
    
    return (ret == ESP_OK);
}

bool ble_publish_processed_sensor_data(const uint8_t sensor_id, const processed_sensor_data_t* processed_data) {
    if (!is_connected || !processed_data) {
        return false;
    }
    
    ble_sensor_data_t sensor_data = {
        .timestamp = processed_data->timestamp,
        .sensor_id = sensor_id,
        .raw_X = processed_data->raw_x,
        .raw_Y = processed_data->raw_y,
        .raw_Z = processed_data->raw_z,
        .force_X = (float)processed_data->force_x,
        .force_Y = (float)processed_data->force_y,
        .force_Z = (float)processed_data->force_z,
    };
    
    // Send notification with processed data
    esp_err_t ret = esp_ble_gatts_send_indicate(gatts_if, conn_id, char_handle_sensor_data,
                                                sizeof(sensor_data), (uint8_t*)&sensor_data, false);
    
    static uint32_t last_log_time[NUM_OF_SPI_DEV] = {0};
    static int log_cnt[NUM_OF_SPI_DEV] = {0};
    if (get_ticks() - last_log_time[sensor_id] > ms_to_ticks(1000)) {
        LOGI("Publishing processed sensor %d data via BLE: %d times (Force: X=%.2f, Y=%.2f, Z=%.2f)", 
             sensor_id, log_cnt[sensor_id], sensor_data.force_X, sensor_data.force_Y, sensor_data.force_Z);
        last_log_time[sensor_id] = get_ticks();
        log_cnt[sensor_id] = 0;
    } else {
        log_cnt[sensor_id]++;
    }
    
    return (ret == ESP_OK);
}

bool ble_publish_sensor_status(const uint8_t sensor_id, bool calibration_complete, bool sensor_ready) {
    if (!is_connected) {
        return false;
    }
    
    ble_sensor_status_t status = {
        .sensor_id = sensor_id,
        .calibration_complete = calibration_complete,
        .sensor_ready = sensor_ready
    };
    
    esp_err_t ret = esp_ble_gatts_send_indicate(gatts_if, conn_id, char_handle_sensor_status,
                                                sizeof(status), (uint8_t*)&status, true);
    return (ret == ESP_OK);
}

void ble_publish_sensor_cal_end(const uint8_t sensor_id) {
    while (ble_status != BLE_STATUS_CONNECTED) {
        delay(ms_to_ticks(50));
        continue;
    }
    
    LOGI("Publishing calibration end for sensor %d via BLE", sensor_id);
    ble_publish_sensor_status(sensor_id, true, true);
}

/* GATT Server Event Handler */
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if_param, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            LOGI("GATT server registered, app_id: %d", param->reg.app_id);
            gatts_if = gatts_if_param;
            ble_services_init();
            break;
            
        case ESP_GATTS_CREATE_EVT:
            LOGI("Service created, service_handle: %d", param->create.service_handle);
            service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(service_handle);
            // Add characteristics after service is created
            add_characteristics();
            break;
            
        case ESP_GATTS_ADD_CHAR_EVT:
            LOGI("Characteristic added, handle: %d", param->add_char.attr_handle);
            // Store characteristic handles based on UUID
            if (memcmp(param->add_char.char_uuid.uuid.uuid128, char_uuid_sensor_data, 16) == 0) {
                char_handle_sensor_data = param->add_char.attr_handle;
            } else if (memcmp(param->add_char.char_uuid.uuid.uuid128, char_uuid_sensor_status, 16) == 0) {
                char_handle_sensor_status = param->add_char.attr_handle;
            } else if (memcmp(param->add_char.char_uuid.uuid.uuid128, char_uuid_calibration, 16) == 0) {
                char_handle_calibration = param->add_char.attr_handle;
            } else if (memcmp(param->add_char.char_uuid.uuid.uuid128, char_uuid_timer_control, 16) == 0) {
                char_handle_timer_control = param->add_char.attr_handle;
            }
            break;
            
        case ESP_GATTS_CONNECT_EVT:
            LOGI("BLE client connected, conn_id: %d", param->connect.conn_id);
            conn_id = param->connect.conn_id;
            is_connected = true;
            ble_status = BLE_STATUS_CONNECTED;
            esp_ble_gap_stop_advertising();
            break;
            
        case ESP_GATTS_DISCONNECT_EVT:
            LOGI("BLE client disconnected, conn_id: %d", param->disconnect.conn_id);
            is_connected = false;
            ble_status = BLE_STATUS_ADVERTISING;
            ble_start_advertising();
            break;
            
        case ESP_GATTS_WRITE_EVT:
            LOGI("GATT write event, handle: %d, len: %d", param->write.handle, param->write.len);
            
            // Handle calibration commands
            if (param->write.handle == char_handle_calibration && param->write.len >= sizeof(ble_command_packet_t)) {
                ble_command_packet_t *cmd = (ble_command_packet_t*)param->write.value;
                if (cmd->command == BLE_CMD_CALIBRATE_SENSOR) {
                    LOGI("Received calibration command for sensor %d", cmd->sensor_id);
                    mlx_set_force_normalization(cmd->sensor_id);
                }
            }
            
            // Handle timer commands
            if (param->write.handle == char_handle_timer_control && param->write.len >= sizeof(ble_command_packet_t)) {
                ble_command_packet_t *cmd = (ble_command_packet_t*)param->write.value;
                if (cmd->command == BLE_CMD_RESET_TIMER) {
                    LOGI("Received timer reset command");
                    mqtt_timer_reset();
                }
            }
            
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if_param, param->write.conn_id, param->write.trans_id,
                                            ESP_GATT_OK, NULL);
            }
            break;
            
        default:
            break;
    }
}

/* GAP Event Handler */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            LOGI("Advertisement data set complete");
            break;
            
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                LOGI("Advertisement started successfully");
                ble_status = BLE_STATUS_ADVERTISING;
            } else {
                LOGE("Advertisement start failed: %d", param->adv_start_cmpl.status);
            }
            break;
            
        default:
            break;
    }
}

/* Utility functions */
bool ble_start_advertising(void) {
    esp_err_t ret = esp_ble_gap_start_advertising(&adv_params);
    return (ret == ESP_OK);
}

bool ble_stop_advertising(void) {
    esp_err_t ret = esp_ble_gap_stop_advertising();
    return (ret == ESP_OK);
}

/* Service and Characteristic UUIDs in binary format (Little Endian) */
static uint8_t char_uuid_sensor_data[16] = {
    // 6ba7b811-9dad-11d1-80b4-00c04fd430c8
    0xc8, 0x30, 0xd4, 0x4f, 0xc0, 0x00, 0xb4, 0x80, 0xd1, 0x11, 0xad, 0x9d, 0x11, 0xb8, 0xa7, 0x6b
};

static uint8_t char_uuid_sensor_status[16] = {
    // 6ba7b812-9dad-11d1-80b4-00c04fd430c8
    0xc8, 0x30, 0xd4, 0x4f, 0xc0, 0x00, 0xb4, 0x80, 0xd1, 0x11, 0xad, 0x9d, 0x12, 0xb8, 0xa7, 0x6b
};

static uint8_t char_uuid_calibration[16] = {
    // 6ba7b813-9dad-11d1-80b4-00c04fd430c8
    0xc8, 0x30, 0xd4, 0x4f, 0xc0, 0x00, 0xb4, 0x80, 0xd1, 0x11, 0xad, 0x9d, 0x13, 0xb8, 0xa7, 0x6b
};

static uint8_t char_uuid_timer_control[16] = {
    // 6ba7b814-9dad-11d1-80b4-00c04fd430c8
    0xc8, 0x30, 0xd4, 0x4f, 0xc0, 0x00, 0xb4, 0x80, 0xd1, 0x11, 0xad, 0x9d, 0x14, 0xb8, 0xa7, 0x6b
};

static void add_characteristics(void) {
    esp_bt_uuid_t char_uuid;
    esp_gatt_char_prop_t char_property;
    esp_attr_control_t attr_control = {
        .auto_rsp = ESP_GATT_AUTO_RSP,
    };
    
    // Add sensor data characteristic (notify)
    char_uuid.len = ESP_UUID_LEN_128;
    memcpy(char_uuid.uuid.uuid128, char_uuid_sensor_data, 16);
    char_property = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
    
    esp_ble_gatts_add_char(service_handle, &char_uuid, ESP_GATT_PERM_READ,
                           char_property, NULL, &attr_control);
    
    // Add sensor status characteristic (indicate)
    memcpy(char_uuid.uuid.uuid128, char_uuid_sensor_status, 16);
    char_property = ESP_GATT_CHAR_PROP_BIT_INDICATE;
    
    esp_ble_gatts_add_char(service_handle, &char_uuid, ESP_GATT_PERM_READ,
                           char_property, NULL, &attr_control);
    
    // Add calibration characteristic (write)
    memcpy(char_uuid.uuid.uuid128, char_uuid_calibration, 16);
    char_property = ESP_GATT_CHAR_PROP_BIT_WRITE;
    
    esp_ble_gatts_add_char(service_handle, &char_uuid, ESP_GATT_PERM_WRITE,
                           char_property, NULL, &attr_control);
    
    // Add timer control characteristic (write)
    memcpy(char_uuid.uuid.uuid128, char_uuid_timer_control, 16);
    char_property = ESP_GATT_CHAR_PROP_BIT_WRITE;
    
    esp_ble_gatts_add_char(service_handle, &char_uuid, ESP_GATT_PERM_WRITE,
                           char_property, NULL, &attr_control);
}
