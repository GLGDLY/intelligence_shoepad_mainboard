#ifndef _BLE_APP_H
#define _BLE_APP_H

#include "config.h"

#include <stdbool.h>
#include <stdint.h>

// Forward declaration to avoid circular dependency
struct processed_sensor_data_t;

/* Enums */
typedef enum {
    BLE_STATUS_OFFLINE = 0,
    BLE_STATUS_ADVERTISING,
    BLE_STATUS_CONNECTED,
    NUM_OF_BLE_STATUS,
} ble_status_t;

typedef enum {
    BLE_CMD_CALIBRATE_SENSOR = 0,
    BLE_CMD_RESET_TIMER,
    NUM_OF_BLE_COMMANDS,
} ble_command_t;

/* Structs */
typedef struct {
    uint64_t timestamp;  // 8 bytes
    uint8_t sensor_id;   // 1 byte
    int16_t raw_X;       // 2 bytes - Raw X-axis magnetic field
    int16_t raw_Y;       // 2 bytes - Raw Y-axis magnetic field
    int16_t raw_Z;       // 2 bytes - Raw Z-axis magnetic field
    float force_X;       // 4 bytes - Force-converted X value
    float force_Y;       // 4 bytes - Force-converted Y value
    float force_Z;       // 4 bytes - Force-converted Z value
} __attribute__((packed)) ble_sensor_data_t;

typedef struct {
    uint8_t sensor_id;
    bool calibration_complete;
    bool sensor_ready;
} __attribute__((packed)) ble_sensor_status_t;

typedef struct {
    ble_command_t command;
    uint8_t sensor_id;
    uint8_t reserved[2];
} __attribute__((packed)) ble_command_packet_t;

/* Global variables */
extern ble_status_t ble_status;

/* Function prototypes */
void ble_app_init(void);
void ble_app_start(void);
void ble_app_stop(void);

bool ble_publish_sensor_data(const uint8_t sensor_id, const char* data);
bool ble_publish_processed_sensor_data(const uint8_t sensor_id, const struct processed_sensor_data_t* processed_data);
bool ble_publish_sensor_status(const uint8_t sensor_id, bool calibration_complete, bool sensor_ready);
void ble_publish_sensor_cal_end(const uint8_t sensor_id);

/* Device identification */
void ble_device_id_init(void);
char* ble_get_device_id(void);

#endif // _BLE_APP_H
