# Server - Real-time Sensor Data Web Interface

## Description
Flask-based web server that provides real-time visualization and feedback for IMU sensor data from Teensy microcontroller. The system offers two main interfaces: a technical dashboard for monitoring and a fullscreen feedback system for exercise guidance.

## Architecture

### Core Components
- **Flask Web Server**: HTTP server with REST endpoints
- **Flask-SocketIO**: Real-time WebSocket communication
- **Serial Communication**: Direct interface with Teensy via USB
- **Multi-threaded Processing**: Concurrent serial reading and web serving

### Web Interfaces
1. **Dashboard** (`/`): Technical monitoring interface with real-time charts
2. **Feedback** (`/feedback`): Fullscreen exercise guidance with audio cues
3. **Debug** (`/debug`): Testing endpoint for system validation

## File Structure

```
Server/
├── server.py              # Main Flask application
├── requirements.txt       # Python dependencies
├── start.sh              # System startup script
├── teensy_recovery.sh    # USB/Teensy recovery utilities
├── teensy_firmware.hex   # Backup firmware for Teensy
├── venv/                 # Python virtual environment
└── templates/
    ├── dashboard.html    # Technical monitoring interface
    └── feedback.html     # Exercise feedback interface
```

## Key Features

### Real-time Data Processing
- Automatic Teensy port detection (`/dev/ttyACM*`, `/dev/ttyUSB*`)
- CSV data parsing with validation and error handling
- NaN/infinite value filtering for JSON compatibility
- Multi-client WebSocket broadcasting

### Exercise Feedback System
- **Phase Detection**: Ready → Go → Hold → Release → Return → Completed
- **Audio Cues**: Browser-compatible sound generation for exercise timing
- **Visual Feedback**: Full-screen color-coded interface with animations
- **Repetition Tracking**: Automatic counting with configurable targets

### System Recovery
- **USB Bus Reset**: Automatic recovery from stuck USB devices
- **Teensy Recovery**: Bootloader detection and firmware restoration
- **Connection Monitoring**: Automatic reconnection and heartbeat system

### Data Visualization
- **Real-time Charts**: Live angle visualization with Chart.js
- **Connection Status**: Visual indicators for all system components
- **Debug Logging**: Comprehensive event tracking and troubleshooting

## Hardware Requirements

### Recommended Setup
- **Raspberry Pi 4** (tested platform) or equivalent Linux system
- **Teensy 3.6** or compatible microcontroller
- **USB Connection**: Direct USB-A to Micro-USB cable
- **Network**: Wi-Fi or Ethernet for web interface access

### Minimum System Requirements
- Python 3.7+
- 2GB RAM (for comfortable multi-client operation)
- USB port with reliable power delivery
- Modern web browser with WebSocket support

## Installation

### Automated Setup
```bash
# Clone project and navigate to Server directory
cd /path/to/project/Server

# Make scripts executable
chmod +x start.sh teensy_recovery.sh

# Run startup script (handles dependencies)
./start.sh
```

### Manual Setup
```bash
# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install dependencies
pip install -r requirements.txt

# Set USB permissions
sudo chmod 666 /dev/ttyACM*

# Start server
python server.py
```

## Configuration

### Serial Communication
- **Baud Rate**: 115200 (configurable in `server.py`)
- **Data Format**: CSV with `DATA` prefix
- **Initialization**: Automatic detection of `INIT_START`/`INIT_COMPLETE` markers

### WebSocket Settings
```python
# Flask-SocketIO configuration
async_mode='threading'     # Raspberry Pi compatible
ping_timeout=60           # Connection timeout
ping_interval=25          # Heartbeat interval
cors_allowed_origins="*"  # Allow all origins
```

### Data Protocol
Expected CSV format from Teensy:
```
DATA,<vert>,<lat>,<tors>,<inTarget>,<tempoPos>,<rep>
```

Where:
- `vert`: Vertical angle (float)
- `lat`: Lateral angle (float) 
- `tors`: Torsion angle (float, NaN for ADXL337)
- `inTarget`: Boolean flag (0/1)
- `tempoPos`: Time in position (milliseconds)
- `rep`: Repetition counter

## Web Interfaces

### Dashboard Interface
- **Real-time Monitoring**: Live sensor values with 1Hz updates
- **Connection Status**: WebSocket and serial connection indicators  
- **Data Visualization**: Bar charts for angle measurements
- **Debug Console**: Event logging and system diagnostics
- **Manual Controls**: Reconnection and testing buttons

### Feedback Interface
- **Exercise Phases**: 
  - READY: Initial positioning
  - GO: Move to target
  - HOLD: Maintain position (with audio cue)
  - RELEASE: Position achieved (with audio cue)  
  - RETURN: Return to start
  - COMPLETED: All repetitions finished (with fanfare)

- **Audio System**: Web Audio API with cross-browser compatibility
- **Visual Cues**: Full-screen color coding and animations
- **Progress Tracking**: Rep counter and completion status

## API Endpoints

### HTTP Routes
- `GET /`: Dashboard interface
- `GET /feedback`: Feedback interface
- `GET /debug`: Send test data to connected clients
- `GET /status`: System status JSON response

### WebSocket Events
#### Client → Server
- `test_message`: Connection testing
- `connect`/`disconnect`: Connection management

#### Server → Client
- `sensor_update`: Real-time sensor data
- `system_status`: System state changes
- `heartbeat`: Connection keepalive

## System Recovery

### Teensy Recovery Script
```bash
# Manual recovery
./teensy_recovery.sh

# Automatic recovery (integrated in start.sh)
# - Detects bootloader mode
# - Attempts firmware restoration
# - USB bus reset if needed
```

### USB Troubleshooting
1. **Permission Issues**: `sudo chmod 666 /dev/ttyACM*`
2. **Device Not Found**: Check `lsusb` output and cable connection
3. **Bootloader Mode**: Run recovery script or manual Arduino IDE upload
4. **Bus Reset**: Automatic via `teensy_recovery.sh`

## Production Deployment

### Systemd Service (Optional)
```ini
[Unit]
Description=Sensor Dashboard
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/Project_server
ExecStart=/home/pi/Project_server/start.sh
Restart=always

[Install]
WantedBy=multi-user.target
```

### Firewall Configuration
```bash
# Allow web server port
sudo ufw allow 5000/tcp

# For production, consider nginx reverse proxy
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp
```

## Performance Notes

### Optimization Settings
- **Threading Mode**: Better Raspberry Pi compatibility vs eventlet
- **Data Buffering**: Minimal buffering for real-time performance  
- **Client Limit**: Tested up to 10 concurrent connections
- **Update Rate**: ~10-50 Hz depending on Teensy configuration

### Resource Usage
- **RAM**: ~100MB base + 10MB per connected client
- **CPU**: <10% on Raspberry Pi 4 with 5 clients
- **Network**: ~1KB/s per client for sensor data

## Dependencies

### Python Packages
- `Flask==2.2.5`: Web framework
- `Flask-SocketIO==5.1.1`: WebSocket support  
- `pyserial==3.5`: Serial communication
- `eventlet==0.33.3`: Async networking (fallback)

### System Dependencies
- `teensy-loader-cli`: Firmware recovery utility
- `usbutils`: USB device management (`lsusb`)

## Troubleshooting

### Common Issues
1. **Port Permission Denied**: Run `sudo chmod 666 /dev/ttyACM*`
2. **Teensy Not Detected**: Check USB connection, try recovery script
3. **WebSocket Connection Failed**: Check firewall, verify port 5000
4. **Audio Not Working**: Requires user interaction for browser audio policy
5. **High CPU Usage**: Check for serial read errors or USB issues

### Debug Tools
- **Debug Endpoint**: `/debug` - sends test data
- **Status Endpoint**: `/status` - system health check
- **Browser Console**: WebSocket event monitoring
- **Server Logs**: Console output with timestamps

### Log Analysis
```bash
# Monitor live logs
python server.py 2>&1 | tee sensor.log

# System logs (if using systemd)
journalctl -u sensor-dashboard -f
```
