#ifndef _BLE_UTILS_H
#define _BLE_UTILS_H

#include <stdbool.h>
#include <stdint.h>

/* BLE Connection Management */
bool ble_is_connected(void);
bool ble_start_advertising(void);
bool ble_stop_advertising(void);

/* BLE Data Transmission */
bool ble_notify_data(uint16_t char_handle, const uint8_t* data, size_t len);
bool ble_indicate_data(uint16_t char_handle, const uint8_t* data, size_t len);

/* BLE Service and Characteristic Management */
void ble_services_init(void);
uint16_t ble_get_char_handle(const char* char_uuid);

/* BLE Event Callbacks */
void ble_on_connect_callback(void);
void ble_on_disconnect_callback(void);
void ble_on_write_callback(uint16_t char_handle, const uint8_t* data, size_t len);

/* Utility Functions */
void ble_mac_to_string(uint8_t* mac, char* str);
bool ble_string_to_uuid(const char* uuid_str, uint8_t* uuid_bytes);

#endif // _BLE_UTILS_H
