#ifndef _SIGNAL_PROCESSING_H
#define _SIGNAL_PROCESSING_H

#include "config.h"
#include "MLX90393_cmds.h"

#include <stdint.h>
#include <stdbool.h>

/* Data Structures */

// Per-sensor calibration constants for polynomial force conversion
typedef struct {
    // Polynomial coefficients for multi-variate force calculation
    double K0, K1, K2, K3, K4, K5, K6, K7, K8, K9;
    
    // Cubic polynomial coefficients for X, Y, Z axis conversion
    double a_x, b_x, c_x;  // X = a_x*X^3 + b_x*X^2 + c_x*X
    double a_y, b_y, c_y;  // Y = a_y*Y^3 + b_y*Y^2 + c_y*Y
    double a_z, b_z, c_z;  // Z = a_z*Z^3 + b_z*Z^2 + c_z*Z
} sensor_calibration_t;

// Moving window data for noise rejection
typedef struct {
    int16_t x_readings[SMOOTHING_WINDOW_SIZE];
    int16_t y_readings[SMOOTHING_WINDOW_SIZE];
    int16_t z_readings[SMOOTHING_WINDOW_SIZE];
    
    int32_t x_total;
    int32_t y_total;
    int32_t z_total;
    
    uint8_t current_index;
    bool is_initialized;
} moving_window_t;

// Processed sensor data with smoothed and force-converted values
typedef struct {
    uint64_t timestamp;
    uint8_t sensor_id;
    
    // Raw values (after normalization)
    int16_t raw_x, raw_y, raw_z;
    
    // Smoothed values (after moving window filter)
    double smoothed_x, smoothed_y, smoothed_z;
    
    // Force-converted values
    double force_x, force_y, force_z;
} processed_sensor_data_t;

/* Global Variables */
extern sensor_calibration_t sensor_calibrations[NUM_OF_SPI_DEV];
extern moving_window_t moving_windows[NUM_OF_SPI_DEV];

/* Function Prototypes */

// Initialization functions
void signal_processing_init(void);
void load_sensor_calibrations(void);
void init_moving_windows(void);

// Moving window noise rejection
void update_moving_window(uint8_t sensor_id, int16_t x, int16_t y, int16_t z);
void get_smoothed_values(uint8_t sensor_id, double* smoothed_x, double* smoothed_y, double* smoothed_z);

// Force conversion algorithms
double calculate_multivariate_force(uint8_t sensor_id, double x, double y, double z);
double calculate_cubic_polynomial_x(uint8_t sensor_id, double x);
double calculate_cubic_polynomial_y(uint8_t sensor_id, double y);
double calculate_cubic_polynomial_z(uint8_t sensor_id, double z);

// Complete processing pipeline
bool process_sensor_data(uint8_t sensor_id, mlx90393_data_t* raw_data, processed_sensor_data_t* processed_data);

// Utility functions
void reset_moving_window(uint8_t sensor_id);
bool is_sensor_window_ready(uint8_t sensor_id);

#endif // _SIGNAL_PROCESSING_H
