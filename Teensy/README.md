# Teensy - IMU Data Acquisition System

## Description
This module handles data acquisition from inertial sensors (IMU) on Teensy microcontroller, with support for two sensor types:
- **BNO055**: 9-axis IMU sensor with integrated fusion algorithms
- **ADXL337**: 3-axis analog accelerometer

The system is designed to acquire orientation data (pitch, roll, yaw) and transmit it via serial in CSV format for PC analysis.

## Architecture

### Sensor Configuration
- **ACTIVE_SENSOR = 1**: BNO055 only
- **ACTIVE_SENSOR = 2**: ADXL337 only  
- **ACTIVE_SENSOR = 3**: Dual mode (both sensors)

### Debug Mode
- **DEBUG_MODE = 0**: Clean CSV output for data acquisition
- **DEBUG_MODE = 1**: Full debug output with diagnostic information

## Hardware Requirements

### BNO055
- I2C communication
- Default address: 0x28
- External crystal supported for higher precision

### ADXL337
- Analog connections:
  - Pin A0: X-axis
  - Pin A1: Y-axis  
  - Pin A2: Z-axis
- Power supply: 3.3V
- Sensitivity: 0.33V/g

## File Structure

```
Teensy/
├── project_config.h     # Global configurations (sensors, debug)
├── debug_utils.h        # Conditional debug macros
├── sensor_bno.h/.cpp    # BNO055 driver
├── sensor_adxl.h/.cpp   # ADXL337 driver with advanced calibration
└── output.h/.cpp        # CSV output formatting
```

## Key Features

### ADXL337 Calibration
- Automatic startup calibration with 100 samples
- Offset compensation for each axis
- Configurable logical axis mapping
- Customizable coordinate system

### Data Filtering
- Exponential low-pass filter (EMA) for accelerations
- Anti-noise filter with dead-zone
- Movement detection for adaptive filtering
- Gravity vector normalization
- Separate filter for calculated angles

### CSV Output
Header: `vert,lat,tors,inTarget,tempoPos,rep`

Where:
- `vert`: Vertical angle (pitch)
- `lat`: Lateral angle (roll)
- `tors`: Torsion angle (yaw, NaN for ADXL337)
- `inTarget`: Boolean flag (0/1)
- `tempoPos`: Timestamp in milliseconds
- `rep`: Repetition/sample number

## Configuration

### ADXL337 Axis Customization
In `sensor_adxl.h` file:
```cpp
#define LOGICAL_X_AXIS 'Z'    // Physical axis for logical X
#define LOGICAL_X_SIGN -1     // Sign for logical X
#define LOGICAL_Y_AXIS 'Y'    // Physical axis for logical Y  
#define LOGICAL_Y_SIGN 1      // Sign for logical Y
#define LOGICAL_Z_AXIS 'X'    // Physical axis for logical Z
#define LOGICAL_Z_SIGN -1     // Sign for logical Z
```

### Analog Pins
```cpp
#define ADXL_PIN_X A0
#define ADXL_PIN_Y A1
#define ADXL_PIN_Z A2
```

## Usage

### Setup
1. Configure parameters in `project_config.h`
2. Connect sensors according to hardware schema
3. Upload code to Teensy

### ADXL337 Calibration
1. At startup, position sensor in reference orientation
2. Wait 3 seconds for stabilization
3. System acquires 100 samples to calculate offsets
4. Calibration is stored for current session

### Data Acquisition
- Data is transmitted continuously via serial
- Recommended speed: 115200 baud or higher
- Format: CSV with initial header

## Main APIs

### BNO055
```cpp
bool BNO_begin(uint8_t address = 0x28, int32_t id = 55);
EulerAngles BNO_readEuler();
void BNO_useExtCrystal(bool use);
```

### ADXL337
```cpp
bool ADXL_begin();
EulerAngles ADXL_readEuler();
void ADXL_printRawValues();    // Debug
void ADXL_resetFilters();      // Reset filters
```

### Output
```cpp
void printCsvHeader();
void printCsvRow(float vert, float lat, float tors, 
                 bool inTarget, unsigned long tempoPos, 
                 unsigned int rep);
```

## Technical Notes

### ADXL337 Limitations
- Cannot measure yaw (rotation around Z-axis)
- Requires calibration to compensate hardware offsets
- Sensitive to vibrations and electrical noise

### Optimizations
- Adaptive filters based on movement detection
- Dead-zone to reduce noise when stationary
- Gravity vector normalization for better precision

### Troubleshooting
1. **Wrong ADXL337 values**: Check 3.3V power supply and calibration
2. **BNO055 not detected**: Verify I2C connections and address
3. **Unstable data**: Increase filtering or check sensor mounting

## Dependencies
- Arduino IDE or PlatformIO
- Adafruit_BNO055 library
- Adafruit_Sensor library
- Teensy board (tested on Teensy 3.6)
