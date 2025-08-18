#!/bin/bash
# Full path to your project directory
PROJECT_DIR="/home/pi/Project_server"

echo "=== Starting Sensor Dashboard ==="

# Function to check if Teensy is in proper mode
check_teensy() {
    if lsusb | grep -q "Teensyduino Serial"; then
        echo "Teensy in normal mode"
        return 0
    elif lsusb | grep -q "Teensy Halfkay Bootloader"; then
        echo "Teensy in bootloader mode - attempting recovery"
        return 1
    else
        echo "Teensy not detected"
        return 2
    fi
}

# Run recovery script if needed
if ! check_teensy; then
    echo "Running Teensy recovery..."
    if [ -f "$PROJECT_DIR/teensy_recovery.sh" ]; then
        bash "$PROJECT_DIR/teensy_recovery.sh"
    else
        echo "Recovery script not found, trying basic USB reset..."
        # Basic USB reset
        for port in /sys/bus/usb/drivers/usb/1-*; do
            if [ -e "$port" ]; then
                port_name=$(basename "$port")
                echo "$port_name" | sudo tee /sys/bus/usb/drivers/usb/unbind > /dev/null 2>&1
                sleep 1
                echo "$port_name" | sudo tee /sys/bus/usb/drivers/usb/bind > /dev/null 2>&1
            fi
        done
        sleep 5
    fi
fi

# Wait for serial port
echo "Waiting for serial port..."
for i in {1..10}; do
    if ls /dev/ttyACM* &> /dev/null; then
        echo "Serial port found: $(ls /dev/ttyACM*)"
        break
    fi
    echo "Attempt $i/10: No serial port, waiting..."
    sleep 2
done

# Set permissions
sudo chmod 666 /dev/ttyACM* 2>/dev/null

# Activate virtual environment
source "$PROJECT_DIR/venv/bin/activate"

# Check and install missing dependencies
if ! python -c "import simple_websocket" 2>/dev/null; then
    echo "Installing missing WebSocket dependencies..."
    pip install simple-websocket python-socketio[client]
fi

# Start the server with error handling
cd "$PROJECT_DIR"
echo "Starting sensor dashboard server..."
python server.py 2>&1 | logger -t sensor-dashboard
