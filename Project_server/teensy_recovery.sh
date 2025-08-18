#!/bin/bash

PROJECT_DIR="/home/pi/Project_server"

echo "=== Teensy Recovery Script ==="
echo "Checking Teensy status..."

# Function to check Teensy mode
check_teensy_status() {
    if lsusb | grep -q "Teensyduino Serial"; then
        echo "Teensy in NORMAL mode - ready for serial communication"
        return 0
    elif lsusb | grep -q "Teensy Halfkay Bootloader"; then
        echo "Teensy in BOOTLOADER mode - needs programming"
        return 1
    else
        echo "Teensy not detected"
        return 2
    fi
}

# Function to reset USB bus
reset_usb_bus() {
    echo "Resetting USB bus..."
    
    # Method 1: Unbind and rebind USB ports
    for port in /sys/bus/usb/drivers/usb/1-*; do
        if [ -e "$port" ]; then
            port_name=$(basename "$port")
            echo "Resetting USB port: $port_name"
            echo "$port_name" | sudo tee /sys/bus/usb/drivers/usb/unbind > /dev/null 2>&1
            sleep 1
            echo "$port_name" | sudo tee /sys/bus/usb/drivers/usb/bind > /dev/null 2>&1
            sleep 1
        fi
    done
    
    # Method 2: Reset USB hub if present
    if lsusb | grep -q "VIA Labs"; then
        echo "Resetting VIA Labs USB hub..."
        # Find and reset the hub
        for device in /sys/bus/usb/devices/*/; do
            if [ -f "$device/idVendor" ] && [ -f "$device/idProduct" ]; then
                vendor=$(cat "$device/idVendor" 2>/dev/null)
                product=$(cat "$device/idProduct" 2>/dev/null)
                if [ "$vendor" = "2109" ]; then
                    device_name=$(basename "$device")
                    echo "Resetting hub device: $device_name"
                    echo "$device_name" | sudo tee /sys/bus/usb/drivers/usb/unbind > /dev/null 2>&1
                    sleep 2
                    echo "$device_name" | sudo tee /sys/bus/usb/drivers/usb/bind > /dev/null 2>&1
                    sleep 3
                fi
            fi
        done
    fi
    
    # Method 3: Power cycle USB if available
    if [ -d /sys/bus/usb/drivers/usb ]; then
        echo "Attempting USB power cycle..."
        echo "suspend" | sudo tee /sys/bus/usb/devices/*/power/level > /dev/null 2>&1
        sleep 2
        echo "auto" | sudo tee /sys/bus/usb/devices/*/power/level > /dev/null 2>&1
        sleep 3
    fi
}

# Function to attempt Teensy recovery
recover_teensy() {
    echo "Attempting Teensy recovery..."
    
    # Try to install teensy-loader-cli if not present
    if ! command -v teensy-loader-cli &> /dev/null; then
        echo "Installing teensy-loader-cli..."
        sudo apt-get update -qq
        sudo apt-get install -y teensy-loader-cli
    fi
    
    # If we have a backup firmware, try to flash it
    if [ -f "$PROJECT_DIR/teensy_firmware.hex" ]; then
        echo "Found backup firmware, attempting to flash..."
        teensy-loader-cli -mmcu=TEENSY36 -w "$PROJECT_DIR/teensy_firmware.hex"
        sleep 3
    else
        echo "No backup firmware found at $PROJECT_DIR/teensy_firmware.hex"
        echo "You need to:"
        echo "1. Compile your Teensy sketch in Arduino IDE"
        echo "2. Export the .hex file and save it as $PROJECT_DIR/teensy_firmware.hex"
        echo "3. Or manually flash the firmware using Arduino IDE"
    fi
}

# Main recovery process
main() {
    echo "Starting recovery process..."
    
    # Check initial status
    check_teensy_status
    initial_status=$?
    
    if [ $initial_status -eq 0 ]; then
        echo "Teensy is working normally!"
        exit 0
    fi
    
    if [ $initial_status -eq 2 ]; then
        echo "Teensy not detected, trying USB reset..."
        reset_usb_bus
        sleep 5
        check_teensy_status
        if [ $? -eq 2 ]; then
            echo "Still not detected. Check physical connections."
            echo "Try unplugging and reconnecting the Teensy."
            exit 1
        fi
    fi
    
    # If in bootloader mode, try to recover
    if [ $initial_status -eq 1 ] || lsusb | grep -q "Teensy Halfkay Bootloader"; then
        echo "Teensy in bootloader mode, attempting recovery..."
        recover_teensy
        sleep 5
        
        # Check if recovery worked
        check_teensy_status
        if [ $? -eq 0 ]; then
            echo "Recovery successful!"
        else
            echo "Recovery failed. Manual intervention required."
            echo "Please:"
            echo "1. Open Arduino IDE on your main computer"
            echo "2. Load your Teensy sketch"
            echo "3. Select Tools → Board → Teensy (your version)"
            echo "4. Upload the sketch"
            exit 1
        fi
    fi
    
    # Final verification
    echo "Waiting for Teensy to stabilize..."
    sleep 5
    
    if ls /dev/ttyACM* &> /dev/null; then
        echo "Serial port detected: $(ls /dev/ttyACM*)"
        sudo chmod 666 /dev/ttyACM*
        echo "Permissions set"
        echo "Ready to start sensor dashboard!"
    else
        echo "No serial port found"
        exit 1
    fi
}

# Run with error handling
set -e
main "$@"