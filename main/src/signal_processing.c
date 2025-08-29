#include "signal_processing.h"

#include "debug.h"
#include "mqtt_timer.h"

#include <string.h>
#include <math.h>

/* Global Variables */
sensor_calibration_t sensor_calibrations[NUM_OF_SPI_DEV];
moving_window_t moving_windows[NUM_OF_SPI_DEV];

/* Default calibration constants (can be overridden per sensor) */
static const sensor_calibration_t default_calibration = {
    // Polynomial coefficients for multi-variate force calculation
    .K0 = 0.1765,
    .K1 = -0.3713,
    .K2 = -0.3663,
    .K3 = 0.8237,
    .K4 = 1.4129,
    .K5 = -0.0049,
    .K6 = -0.0289,
    .K7 = 1.3975,
    .K8 = 0.0247,
    .K9 = 0.0335,
    
    // Cubic polynomial coefficients for X axis
    .a_x = 0.092730,
    .b_x = 0.000000,
    .c_x = 0.964709,
    
    // Cubic polynomial coefficients for Y axis
    .a_y = 0.066385,
    .b_y = 0.000000,
    .c_y = 0.955328,
    
    // Cubic polynomial coefficients for Z axis
    .a_z = 0.002315,
    .b_z = 0.055302,
    .c_z = 1.636972,
};

/* Initialization Functions */

void signal_processing_init(void) {
    load_sensor_calibrations();
    init_moving_windows();
    LOGI("Signal processing initialized");
}

void load_sensor_calibrations(void) {
    // Initialize all sensors with default calibration
    // In the future, this could load per-sensor calibrations from flash
    for (uint8_t i = 0; i < NUM_OF_SPI_DEV; i++) {
        sensor_calibrations[i] = default_calibration;
    }
    LOGI("Sensor calibrations loaded");
}

void init_moving_windows(void) {
    for (uint8_t i = 0; i < NUM_OF_SPI_DEV; i++) {
        memset(&moving_windows[i], 0, sizeof(moving_window_t));
        moving_windows[i].is_initialized = false;
    }
    LOGI("Moving windows initialized");
}

/* Moving Window Noise Rejection */

void update_moving_window(uint8_t sensor_id, int16_t x, int16_t y, int16_t z) {
    if (sensor_id >= NUM_OF_SPI_DEV) {
        return;
    }
    
    moving_window_t* window = &moving_windows[sensor_id];
    
    // Initialize window on first use
    if (!window->is_initialized) {
        // Fill initial window with first reading
        for (uint8_t i = 0; i < SMOOTHING_WINDOW_SIZE; i++) {
            window->x_readings[i] = x;
            window->y_readings[i] = y;
            window->z_readings[i] = z;
        }
        window->x_total = x * SMOOTHING_WINDOW_SIZE;
        window->y_total = y * SMOOTHING_WINDOW_SIZE;
        window->z_total = z * SMOOTHING_WINDOW_SIZE;
        window->current_index = 0;
        window->is_initialized = true;
        return;
    }
    
    // Remove oldest values
    window->x_total -= window->x_readings[window->current_index];
    window->y_total -= window->y_readings[window->current_index];
    window->z_total -= window->z_readings[window->current_index];
    
    // Add new values
    window->x_readings[window->current_index] = x;
    window->y_readings[window->current_index] = y;
    window->z_readings[window->current_index] = z;
    
    window->x_total += window->x_readings[window->current_index];
    window->y_total += window->y_readings[window->current_index];
    window->z_total += window->z_readings[window->current_index];
    
    // Update index (circular buffer)
    window->current_index = (window->current_index + 1) % SMOOTHING_WINDOW_SIZE;
}

void get_smoothed_values(uint8_t sensor_id, double* smoothed_x, double* smoothed_y, double* smoothed_z) {
    if (sensor_id >= NUM_OF_SPI_DEV || !moving_windows[sensor_id].is_initialized) {
        *smoothed_x = 0.0;
        *smoothed_y = 0.0;
        *smoothed_z = 0.0;
        return;
    }
    
    moving_window_t* window = &moving_windows[sensor_id];
    
    // Calculate moving averages
    *smoothed_x = (double)window->x_total / SMOOTHING_WINDOW_SIZE;
    *smoothed_y = (double)window->y_total / SMOOTHING_WINDOW_SIZE;
    *smoothed_z = (double)window->z_total / SMOOTHING_WINDOW_SIZE;
}

/* Force Conversion Algorithms */

double calculate_multivariate_force(uint8_t sensor_id, double x, double y, double z) {
    if (sensor_id >= NUM_OF_SPI_DEV) {
        return 0.0;
    }
    
    const sensor_calibration_t* cal = &sensor_calibrations[sensor_id];
    
    // Multi-variate polynomial: 
    // Force = K0 + K1*X + K2*Y + K3*Z + K4*X² + K5*X*Y + K6*X*Z + K7*Y² + K8*Y*Z + K9*Z²
    double force = cal->K0 + 
                   cal->K1 * x + 
                   cal->K2 * y + 
                   cal->K3 * z + 
                   cal->K4 * x * x + 
                   cal->K5 * x * y + 
                   cal->K6 * x * z + 
                   cal->K7 * y * y + 
                   cal->K8 * y * z + 
                   cal->K9 * z * z;
    
    return force;
}

double calculate_cubic_polynomial_x(uint8_t sensor_id, double x) {
    if (sensor_id >= NUM_OF_SPI_DEV) {
        return 0.0;
    }
    
    const sensor_calibration_t* cal = &sensor_calibrations[sensor_id];
    
    // Cubic polynomial: X = a_x*X³ + b_x*X² + c_x*X
    return cal->a_x * x * x * x + cal->b_x * x * x + cal->c_x * x;
}

double calculate_cubic_polynomial_y(uint8_t sensor_id, double y) {
    if (sensor_id >= NUM_OF_SPI_DEV) {
        return 0.0;
    }
    
    const sensor_calibration_t* cal = &sensor_calibrations[sensor_id];
    
    // Cubic polynomial: Y = a_y*Y³ + b_y*Y² + c_y*Y
    return cal->a_y * y * y * y + cal->b_y * y * y + cal->c_y * y;
}

double calculate_cubic_polynomial_z(uint8_t sensor_id, double z) {
    if (sensor_id >= NUM_OF_SPI_DEV) {
        return 0.0;
    }
    
    const sensor_calibration_t* cal = &sensor_calibrations[sensor_id];
    
    // Cubic polynomial: Z = a_z*Z³ + b_z*Z² + c_z*Z
    return cal->a_z * z * z * z + cal->b_z * z * z + cal->c_z * z;
}

/* Complete Processing Pipeline */

bool process_sensor_data(uint8_t sensor_id, mlx90393_data_t* raw_data, processed_sensor_data_t* processed_data) {
    if (sensor_id >= NUM_OF_SPI_DEV || !raw_data || !processed_data) {
        return false;
    }
    
    // Initialize processed data structure
    processed_data->timestamp = mqtt_timer_get(sensor_id);
    processed_data->sensor_id = sensor_id;
    processed_data->raw_x = raw_data->X;
    processed_data->raw_y = raw_data->Y;
    processed_data->raw_z = raw_data->Z;
    
    // Step 1: Update moving window with raw values
    update_moving_window(sensor_id, raw_data->X, raw_data->Y, raw_data->Z);
    
    // Step 2: Get smoothed values
    get_smoothed_values(sensor_id, &processed_data->smoothed_x, 
                       &processed_data->smoothed_y, &processed_data->smoothed_z);
    
    // Step 3: Apply force conversion algorithms
    // First calculate the multi-variate force (this modifies smoothed_z)
    double multivariate_force = calculate_multivariate_force(sensor_id, 
                                                            processed_data->smoothed_x,
                                                            processed_data->smoothed_y, 
                                                            processed_data->smoothed_z);
    
    // Then apply cubic polynomials to each axis
    processed_data->force_x = calculate_cubic_polynomial_x(sensor_id, processed_data->smoothed_x);
    processed_data->force_y = calculate_cubic_polynomial_y(sensor_id, processed_data->smoothed_y);
    processed_data->force_z = calculate_cubic_polynomial_z(sensor_id, multivariate_force);
    
    return moving_windows[sensor_id].is_initialized;
}

/* Utility Functions */

void reset_moving_window(uint8_t sensor_id) {
    if (sensor_id >= NUM_OF_SPI_DEV) {
        return;
    }
    
    memset(&moving_windows[sensor_id], 0, sizeof(moving_window_t));
    moving_windows[sensor_id].is_initialized = false;
    LOGI("Reset moving window for sensor %d", sensor_id);
}

bool is_sensor_window_ready(uint8_t sensor_id) {
    if (sensor_id >= NUM_OF_SPI_DEV) {
        return false;
    }
    
    return moving_windows[sensor_id].is_initialized;
}
