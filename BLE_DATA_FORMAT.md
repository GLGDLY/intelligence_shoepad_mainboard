# BLE 5.1 Data Format Specification

## Overview
This document describes the BLE GATT service and characteristic structure for the Intelligent Shoe Pad system.

## BLE Service Structure

### Primary Service
- **Service UUID**: `6ba7b810-9dad-11d1-80b4-00c04fd430c8`
- **Service Name**: Sensor Data Service

### Characteristics

#### 1. Sensor Data Characteristic
- **UUID**: `6ba7b811-9dad-11d1-80b4-00c04fd430c8`
- **Properties**: Notify
- **Data Format**: Binary struct `ble_sensor_data_t`
- **Size**: 28 bytes

```c
typedef struct {
    uint64_t timestamp;  // 8 bytes - Timer value in milliseconds
    uint8_t sensor_id;   // 1 byte  - Sensor ID (0-4)
    int16_t raw_X;       // 2 bytes - Raw X-axis magnetic field (after normalization)
    int16_t raw_Y;       // 2 bytes - Raw Y-axis magnetic field (after normalization)
    int16_t raw_Z;       // 2 bytes - Raw Z-axis magnetic field (after normalization)
    float force_X;       // 4 bytes - Force-converted X value
    float force_Y;       // 4 bytes - Force-converted Y value
    float force_Z;       // 4 bytes - Force-converted Z value
} __attribute__((packed)) ble_sensor_data_t;
```

**Signal Processing Pipeline**:
1. **Raw Reading**: MLX90393 sensor data (T, X, Y, Z) - DRDY or polling mode
2. **Normalization**: Remove sensor baseline offsets
3. **Moving Window Filter**: 30-sample moving average for noise reduction
4. **Force Conversion**: Multi-variate polynomial + cubic polynomial conversion

**Note**: Temperature is read from MLX90393 sensor but not transmitted via BLE.

#### 2. Sensor Status Characteristic
- **UUID**: `6ba7b812-9dad-11d1-80b4-00c04fd430c8`
- **Properties**: Indicate
- **Data Format**: Binary struct `ble_sensor_status_t`
- **Size**: 3 bytes

```c
typedef struct {
    uint8_t sensor_id;           // 1 byte - Sensor ID (0-4)
    bool calibration_complete;   // 1 byte - Calibration status
    bool sensor_ready;          // 1 byte - Sensor ready status
} __attribute__((packed)) ble_sensor_status_t;
```

#### 3. Calibration Control Characteristic
- **UUID**: `6ba7b813-9dad-11d1-80b4-00c04fd430c8`
- **Properties**: Write
- **Data Format**: Binary struct `ble_command_packet_t`
- **Size**: 4 bytes

```c
typedef struct {
    ble_command_t command;  // 1 byte - Command type (0 = calibrate)
    uint8_t sensor_id;      // 1 byte - Target sensor ID (0-4)
    uint8_t reserved[2];    // 2 bytes - Reserved for future use
} __attribute__((packed)) ble_command_packet_t;
```

#### 4. Timer Control Characteristic
- **UUID**: `6ba7b814-9dad-11d1-80b4-00c04fd430c8`
- **Properties**: Write
- **Data Format**: Binary struct `ble_command_packet_t`
- **Size**: 4 bytes

Commands:
- `0`: Reset timer

## Data Flow

### 1. Sensor Data Streaming
- **Frequency**: 100 Hz per sensor (configurable via `DATA_PUBLISH_HZ`)
- **Method**: BLE notifications
- **Format**: Packed binary data for efficiency
- **Calibration**: Data is automatically calibrated (offset removed) before transmission

### 2. Status Updates
- **Trigger**: Calibration completion
- **Method**: BLE indications (guaranteed delivery)
- **Content**: Sensor ID and status flags

### 3. Remote Commands
- **Calibration**: Write to calibration characteristic to trigger sensor recalibration
- **Timer Reset**: Write to timer characteristic to reset timestamp counter

## Connection Parameters
- **Device Name**: "IntelligentShoePad"
- **Advertising Interval**: 20-40ms
- **Max Connections**: 1 concurrent connection
- **Connection Requirements**: BLE 5.1+ capable device

## Usage Example (Client Side)

### JavaScript (Web Bluetooth API)
```javascript
// Connect to device
const device = await navigator.bluetooth.requestDevice({
    filters: [{ name: 'IntelligentShoePad' }],
    optionalServices: ['6ba7b810-9dad-11d1-80b4-00c04fd430c8']
});

const server = await device.gatt.connect();
const service = await server.getPrimaryService('6ba7b810-9dad-11d1-80b4-00c04fd430c8');

// Subscribe to sensor data
const sensorChar = await service.getCharacteristic('6ba7b811-9dad-11d1-80b4-00c04fd430c8');
await sensorChar.startNotifications();

sensorChar.addEventListener('characteristicvaluechanged', (event) => {
    const data = new DataView(event.target.value.buffer);
    const timestamp = data.getBigUint64(0, true);  // Little endian
    const sensorId = data.getUint8(8);
    const rawX = data.getInt16(9, true);
    const rawY = data.getInt16(11, true);
    const rawZ = data.getInt16(13, true);
    const forceX = data.getFloat32(15, true);
    const forceY = data.getFloat32(19, true);
    const forceZ = data.getFloat32(23, true);
    
    console.log(`Sensor ${sensorId}:`);
    console.log(`  Raw: X=${rawX}, Y=${rawY}, Z=${rawZ}`);
    console.log(`  Force: X=${forceX.toFixed(3)}, Y=${forceY.toFixed(3)}, Z=${forceZ.toFixed(3)}`);
    console.log(`  @ ${timestamp}ms`);
});

// Trigger calibration for sensor 0
const calChar = await service.getCharacteristic('6ba7b813-9dad-11d1-80b4-00c04fd430c8');
const command = new Uint8Array([0, 0, 0, 0]); // Command 0, Sensor 0, Reserved
await calChar.writeValue(command);
```

## Performance Characteristics
- **Data Rate**: ~2.8 KB/s per sensor (100 Hz × 28 bytes)
- **Total Throughput**: ~14 KB/s for 5 sensors
- **Latency**: <20ms (depends on connection interval)
- **Range**: ~10-30 meters (typical BLE range)
- **Power Consumption**: Optimized for continuous operation

## Signal Processing Algorithms

### 1. Data Acquisition Modes

#### **DRDY Mode (Default)**
- **Configuration**: `#define USE_SPI_DRDY_PINS` in config.h
- **Data Ready Pins**: GPIO 8, 20, 46, 10, 12 (one per sensor)
- **Sync Signal**: GPIO 13 generates sync pulses for synchronized readings
- **Sensor Mode**: Trigger mode - sensors wait for sync signal
- **Advantages**: Event-driven, power efficient, synchronized sampling
- **Timing**: Variable based on sensor ready signals

#### **Polling Mode**
- **Configuration**: Comment out `#define USE_SPI_DRDY_PINS` in config.h
- **Polling Rate**: 300Hz (configurable via `SPI_POLLING_FREQUENCY_HZ`)
- **GPIO Usage**: No DRDY or SYNC pins needed
- **Sensor Mode**: Continuous mode - sensors sample continuously
- **Advantages**: Predictable timing, simpler wiring, no GPIO constraints
- **Timing**: Fixed 300Hz polling (3.33ms intervals)

### 2. Moving Window Noise Rejection
- **Window Size**: 30 samples (configurable via `SMOOTHING_WINDOW_SIZE`)
- **Algorithm**: Circular buffer with running sum for efficiency
- **Purpose**: Reduce high-frequency noise and sensor jitter

```c
// Pseudo-code for moving window filter
xtotal = xtotal - xreadings[currentIndex];  // Remove oldest value
xreadings[currentIndex] = newX;             // Store new value
xtotal = xtotal + xreadings[currentIndex];  // Add new value
currentIndex = (currentIndex + 1) % window_size;
smoothedX = xtotal / window_size;           // Calculate average
```

### 3. Multi-variate Polynomial Force Conversion
Converts smoothed magnetic field values to force using a 10-coefficient polynomial:

```c
Force = K0 + K1*X + K2*Y + K3*Z + K4*X² + K5*X*Y + K6*X*Z + K7*Y² + K8*Y*Z + K9*Z²
```

**Default Coefficients**:
- K0 = 0.1765, K1 = -0.3713, K2 = -0.3663, K3 = 0.8237, K4 = 1.4129
- K5 = -0.0049, K6 = -0.0289, K7 = 1.3975, K8 = 0.0247, K9 = 0.0335

### 4. Cubic Polynomial Axis Conversion
Each axis (X, Y, Z) is individually processed using cubic polynomials:

```c
X_force = a_x*X³ + b_x*X² + c_x*X
Y_force = a_y*Y³ + b_y*Y² + c_y*Y
Z_force = a_z*Z³ + b_z*Z² + c_z*Z
```

**Default Coefficients**:
- **X-axis**: a_x = 0.092730, b_x = 0.000000, c_x = 0.964709
- **Y-axis**: a_y = 0.066385, b_y = 0.000000, c_y = 0.955328
- **Z-axis**: a_z = 0.002315, b_z = 0.055302, c_z = 1.636972

### 5. Per-Sensor Calibration
Each of the 5 sensors can have individual calibration constants, allowing for:
- Manufacturing tolerance compensation
- Position-specific calibration
- Load-specific force curves

## UUID Compliance & Standards

### UUID Regulations
Our UUIDs follow **RFC 4122** and **Bluetooth SIG** standards:

#### **16-bit UUIDs (Reserved)**
- `0x1800-0x26FF`: Reserved by Bluetooth SIG
- ❌ **Cannot be used** for custom applications
- Examples: `0x180F` (Battery Service), `0x1800` (Generic Access)

#### **128-bit UUIDs (Custom Applications)**
- ✅ **Our approach**: Use proper UUID v4 generation
- **Format**: `6ba7b810-9dad-11d1-80b4-00c04fd430c8`
- **Compliance**: RFC 4122 compliant with proper version/variant bits

#### **Why Our UUIDs Are Compliant**
1. **Properly Generated**: Using UUID v4 random generation
2. **Unique Base**: `6ba7b810-9dad-11d1-80b4-00c04fd430c8` family
3. **Sequential Characteristics**: Only last digit differs (10→11→12→13→14)
4. **Version Bits**: Properly set for random UUIDs
5. **Variant Bits**: Correctly formatted for RFC 4122

#### **Legal Considerations**
- **No Registration Required**: 128-bit UUIDs don't need Bluetooth SIG approval
- **Collision Risk**: Extremely low (2^122 possible values)
- **Commercial Use**: Safe for production applications
- **Interoperability**: Standards-compliant across all BLE devices

## Migration Notes from MQTT
- **Data Format**: Changed from text-based CSV to binary structs for efficiency
- **Delivery**: BLE notifications replace MQTT publish
- **Commands**: BLE write operations replace MQTT subscribe/commands
- **Connection**: Direct peer-to-peer instead of broker-based
- **Reliability**: BLE indications used for critical status updates
- **UUIDs**: Now use RFC 4122 compliant 128-bit UUIDs instead of simple sequential ones
- **Temperature**: Removed from BLE transmission to reduce data size (still read from sensor)
