#include "ble_utils.h"

#include "config.h"
#include "debug.h"

#include <esp_gatts_api.h>
#include <esp_gatt_common_api.h>
#include <string.h>

/* External variables from ble_app.c */
extern uint16_t gatts_if;
extern uint16_t conn_id;
extern bool is_connected;

/* Service and Characteristic UUIDs in binary format (Little Endian) */
static uint8_t service_uuid[16] = {
    // 6ba7b810-9dad-11d1-80b4-00c04fd430c8
    0xc8, 0x30, 0xd4, 0x4f, 0xc0, 0x00, 0xb4, 0x80, 0xd1, 0x11, 0xad, 0x9d, 0x10, 0xb8, 0xa7, 0x6b
};

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

/* Connection Management */
bool ble_is_connected(void) {
    return is_connected;
}

/* Data Transmission */
bool ble_notify_data(uint16_t char_handle, const uint8_t* data, size_t len) {
    if (!is_connected) {
        return false;
    }
    
    esp_err_t ret = esp_ble_gatts_send_indicate(gatts_if, conn_id, char_handle,
                                                len, (uint8_t*)data, false);
    return (ret == ESP_OK);
}

bool ble_indicate_data(uint16_t char_handle, const uint8_t* data, size_t len) {
    if (!is_connected) {
        return false;
    }
    
    esp_err_t ret = esp_ble_gatts_send_indicate(gatts_if, conn_id, char_handle,
                                                len, (uint8_t*)data, true);
    return (ret == ESP_OK);
}

/* Service and Characteristic Management */
void ble_services_init(void) {
    // Create primary service
    esp_gatt_srvc_id_t service_id = {
        .is_primary = true,
        .id.inst_id = 0x00,
        .id.uuid.len = ESP_UUID_LEN_128,
    };
    memcpy(service_id.id.uuid.uuid.uuid128, service_uuid, 16);
    
    esp_err_t ret = esp_ble_gatts_create_service(gatts_if, &service_id, 8); // 4 characteristics + descriptors
    if (ret != ESP_OK) {
        LOGE("Failed to create service: %d", ret);
        return;
    }
    
    LOGI("BLE services initialization started");
}



uint16_t ble_get_char_handle(const char* char_uuid) {
    // This function is not currently used but kept for future extensibility
    return 0;
}

/* Utility Functions */
void ble_mac_to_string(uint8_t* mac, char* str) {
    sprintf(str, "%02x%02x%02x%02x%02x%02x", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool ble_string_to_uuid(const char* uuid_str, uint8_t* uuid_bytes) {
    // Simplified UUID conversion - assumes 128-bit UUID string format
    // "12345678-1234-1234-1234-123456789abc"
    if (strlen(uuid_str) != 36) {
        return false;
    }
    
    // This is a basic implementation - in practice you'd want more robust parsing
    char hex_str[3] = {0};
    int byte_index = 15; // Start from MSB for little-endian format
    
    for (int i = 0; i < 36; i++) {
        if (uuid_str[i] == '-') {
            continue;
        }
        
        hex_str[0] = uuid_str[i];
        hex_str[1] = uuid_str[++i];
        uuid_bytes[byte_index--] = (uint8_t)strtol(hex_str, NULL, 16);
        
        if (byte_index < 0) {
            break;
        }
    }
    
    return true;
}

/* Event Callbacks */
void ble_on_connect_callback(void) {
    LOGI("BLE connection established");
    // Add any additional connection setup here
}

void ble_on_disconnect_callback(void) {
    LOGI("BLE connection terminated");
    // Add any cleanup here
}

void ble_on_write_callback(uint16_t char_handle, const uint8_t* data, size_t len) {
    LOGI("BLE write callback - handle: %d, len: %zu", char_handle, len);
    // Additional write handling can be added here
}
