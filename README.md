# IMU Exercise Monitoring System

## Overview
Complete real-time exercise monitoring system combining embedded sensor acquisition with web-based visualization and feedback. The system captures precise orientation data using IMU sensors and provides immediate visual/audio feedback for exercise form and repetition tracking.

## System Architecture

```
┌─────────────┐    USB Serial    ┌─────────────┐    WebSocket     ┌─────────────┐
│   Teensy    │ ───────────────► │   Server    │ ───────────────► │  Web Client │
│   IMU       │     CSV Data     │   Flask     │   Real-time      │  Dashboard  │
│ Acquisition │                  │  SocketIO   │     Data         │  Feedback   │
└─────────────┘                  └─────────────┘                  └─────────────┘
```

### Data Flow
1. **Sensor Layer**: Teensy acquires IMU data from BNO055/ADXL337 sensors
2. **Processing Layer**: Advanced filtering, calibration, and orientation calculation
3. **Communication Layer**: Serial transmission of CSV-formatted data
4. **Server Layer**: Flask server processes data and manages WebSocket connections
5. **Client Layer**: Real-time web interfaces for monitoring and exercise feedback

## Project Structure

```
Project/
├── README.md                 # This file
├── Teensy/                   # Embedded sensor acquisition
│   ├── README.md            # Teensy-specific documentation
│   ├── project_config.h     # Global configuration
│   ├── debug_utils.h        # Debug utilities
│   ├── sensor_bno.h/.cpp    # BNO055 IMU driver
│   ├── sensor_adxl.h/.cpp   # ADXL337 accelerometer driver
│   └── output.h/.cpp        # CSV output formatting
└── Server/                   # Web server and interfaces
    ├── README.md            # Server-specific documentation
    ├── server.py            # Main Flask application
    ├── requirements.txt     # Python dependencies
    ├── start.sh             # System startup script
    ├── teensy_recovery.sh   # Recovery utilities
    ├── teensy_firmware.hex  # Backup firmware
    ├── venv/                # Python virtual environment
    └── templates/
        ├── dashboard.html   # Technical monitoring interface
        └── feedback.html    # Exercise feedback interface
```

## Key Features

### Multi-Sensor Support
- **BNO055**: 9-axis IMU with sensor fusion (pitch, roll, yaw)
- **ADXL337**: 3-axis analog accelerometer (pitch, roll only)
- **Dual Mode**: Simultaneous operation for comparison/validation

### Advanced Signal Processing
- **Auto-Calibration**: Intelligent offset compensation for accelerometers
- **Adaptive Filtering**: Movement-aware filtering with dead-zone processing
- **Coordinate Mapping**: Configurable axis orientation and sign correction
- **Real-time Processing**: <50ms latency from sensor to display

### Exercise Feedback System
- **Phase Detection**: Automatic exercise state recognition
- **Audio Cues**: Browser-based sound generation for timing
- **Visual Feedback**: Full-screen color-coded interface
- **Progress Tracking**: Automatic repetition counting and completion

### Web-Based Interface
- **Real-time Monitoring**: Live sensor data with interactive charts
- **Multi-Client Support**: Concurrent connections for trainers/patients
- **System Diagnostics**: Comprehensive connection and error monitoring
- **Mobile Compatible**: Responsive design for tablets/phones

## Hardware Requirements

### Core Components
- **Teensy 3.6** (or compatible ARM Cortex-M4 microcontroller)
- **BNO055** IMU sensor (I2C interface) *OR*
- **ADXL337** analog accelerometer (3.3V supply)
- **Raspberry Pi 4** (or equivalent Linux system for server)
- **USB Cable** (reliable data connection)

### Optional Components
- **External Crystal** for BNO055 (higher precision)
- **Power Supply** (5V/2A minimum for Pi + Teensy)
- **Enclosure** with secure sensor mounting
- **Display/Projector** for exercise feedback

## Quick Start

### 1. Hardware Setup
```bash
# BNO055 Connection (I2C)
BNO055_VIN  → Teensy 3.3V
BNO055_GND  → Teensy GND  
BNO055_SDA  → Teensy SDA (Pin 18)
BNO055_SCL  → Teensy SCL (Pin 19)

# ADXL337 Connection (Analog)
ADXL337_VCC → Teensy 3.3V
ADXL337_GND → Teensy GND
ADXL337_X   → Teensy A0
ADXL337_Y   → Teensy A1
ADXL337_Z   → Teensy A2
```

### 2. Teensy Configuration
```cpp
// In project_config.h
#define ACTIVE_SENSOR 1     // 1=BNO055, 2=ADXL337, 3=DUAL
#define DEBUG_MODE 0        // 0=Clean CSV, 1=Debug output

// Upload to Teensy via Arduino IDE
```

### 3. Server Setup
```bash
# On Raspberry Pi or Linux system
cd Server/
chmod +x start.sh
./start.sh
```

### 4. Access Interfaces
- **Dashboard**: `http://[pi-ip]:5000/`
- **Feedback**: `http://[pi-ip]:5000/feedback`
- **Status**: `http://[pi-ip]:5000/status`

## Configuration Options

### Sensor Selection
| Mode | Description | Capabilities |
|------|-------------|--------------|
| BNO055 Only | 9-axis IMU with fusion | Full 3D orientation (pitch/roll/yaw) |
| ADXL337 Only | Analog accelerometer | 2D orientation (pitch/roll) |
| Dual Mode | Both sensors active | Comparison and validation |

### Coordinate System Customization
```cpp
// In sensor_adxl.h - customize physical-to-logical mapping
#define LOGICAL_X_AXIS 'Z'    // Physical axis for logical X
#define LOGICAL_X_SIGN -1     // Sign correction
// Repeat for Y and Z axes
```

### Exercise Parameters
- **Target Repetitions**: Configurable in feedback interface
- **Hold Time**: Minimum time in target position
- **Tolerance**: Angular tolerance for "in position" detection
- **Phase Timing**: Customizable timeouts for each exercise phase

## Applications

### Physical Therapy
- **Range of Motion**: Precise measurement of joint angles
- **Exercise Compliance**: Automatic tracking of prescribed movements
- **Progress Monitoring**: Objective measurement of improvement
- **Remote Monitoring**: Therapist can observe patient from different location

### Sports Training
- **Form Analysis**: Real-time feedback on exercise technique
- **Consistency Training**: Repetition accuracy measurement
- **Performance Metrics**: Quantitative movement analysis
- **Comparative Analysis**: Dual-sensor validation

### Research Applications
- **Biomechanics**: High-precision movement capture
- **Human Factors**: Ergonomic assessment tools
- **Rehabilitation Research**: Objective outcome measurement
- **Sensor Validation**: Comparison of different IMU technologies

## Performance Specifications

### Sensor Accuracy
- **BNO055**: ±1° typical accuracy with sensor fusion
- **ADXL337**: ±2° after calibration (gravity vector calculation)
- **Update Rate**: Up to 100Hz (configurable)
- **Latency**: <50ms sensor-to-display

### System Capacity
- **Concurrent Users**: Up to 10 clients tested
- **Data Rate**: ~1KB/s per client
- **Processing Load**: <10% CPU on Raspberry Pi 4
- **Memory Usage**: ~100MB + 10MB per client

## Development

### Building from Source
```bash
# Teensy code (Arduino IDE required)
1. Install Teensyduino
2. Select Board: "Teensy 3.6"
3. Open main sketch file
4. Configure in project_config.h
5. Upload to Teensy

# Server development
cd Server/
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python server.py
```

### Extending the System
- **New Sensors**: Add drivers following existing pattern
- **Custom Exercises**: Modify phase detection logic
- **Additional Interfaces**: Create new HTML templates
- **Data Export**: Add CSV/JSON export functionality
- **Cloud Integration**: Extend with database storage

## Troubleshooting

### Common Issues

#### Hardware
- **No Sensor Data**: Check wiring, power supply (3.3V), I2C pullups
- **Incorrect Readings**: Verify sensor orientation, run calibration
- **Connection Issues**: Try different USB cable, check port permissions

#### Software  
- **Teensy Not Found**: Install Teensyduino, check bootloader mode
- **Web Interface**: Verify Flask is running, check firewall port 5000
- **No Audio**: Requires user interaction for browser audio permissions

#### System
- **High CPU**: Check for USB errors, reduce update rate
- **Memory Issues**: Limit concurrent connections, restart server
- **Network Problems**: Check Pi network configuration, WiFi stability

### Debug Tools
- **Serial Monitor**: Arduino IDE serial monitor for Teensy debug
- **Browser Console**: WebSocket event monitoring and errors  
- **Debug Endpoints**: `/debug` and `/status` for system validation
- **Recovery Scripts**: Automated USB and Teensy recovery utilities

## Contributing

### Development Setup
1. Fork repository and create feature branch
2. Set up development environment (Teensy + Server)
3. Make changes with appropriate comments
4. Test on actual hardware before submitting
5. Update documentation as needed

### Code Style
- **C++**: Follow Arduino/Teensy conventions
- **Python**: PEP 8 compliance
- **JavaScript**: ES6+ features, clear variable naming
- **HTML/CSS**: Semantic markup, responsive design

## License

This project is provided as-is for educational and research purposes. Please ensure compliance with all applicable regulations for medical/therapeutic device development if used in clinical settings.

## Support

For technical support:
1. Check relevant README files (Teensy/ or Server/)
2. Review troubleshooting sections
3. Test with debug modes enabled
4. Verify hardware connections and power supply
5. Check system logs and browser console for errors

## Version History

- **v1.0**: Initial ADXL337 analog sensor support
- **v1.1**: Added release with BNO055 support
- **v1.2**: Implemented dual-sensor mode and advanced calibration
- **v1.3**: Enhanced web interface with audio feedback
- **Current**: Full exercise feedback system with recovery utilities
