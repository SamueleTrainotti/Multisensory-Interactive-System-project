from flask import Flask, render_template
from flask_socketio import SocketIO, emit
import serial
import threading
import time
import glob
import logging
import os
import subprocess
from engineio.payload import Payload

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)

app = Flask(__name__)
app.config['SECRET_KEY'] = os.urandom(24).hex()

# Configure SocketIO with proper settings for Raspberry Pi
socketio = SocketIO(
    app, 
    async_mode='threading',  # Changed from 'eventlet' to 'threading'
    logger=True, 
    engineio_logger=True,    # Enable engineio logging for debugging
    cors_allowed_origins="*", # Allow all origins
    ping_timeout=60,
    ping_interval=25
)

# Initialize sensor data structure
sensor_data = {
    "vert": 0.0,
    "lat": 0.0,
    "tors": 0.0,
    "inPos": False,
    "tempoPos": 0,
    "conteggio": 0,
    "last_update": 0
}

# Global connection tracking
active_connections = set()

# Configuration constants
DATA_CODWORD = "DATA"
INIT_START_MARKER = "INIT_START"
INIT_COMPLETE_MARKER = "INIT_COMPLETE"


def find_teensy_port():
    """Automatically find Teensy's serial port"""
    possible_ports = glob.glob('/dev/ttyACM*') + glob.glob('/dev/ttyUSB*')
    for port in possible_ports:
        try:
            s = serial.Serial(port, 115200, timeout=1)
            s.close()
            logging.info(f"Found Teensy at: {port}")
            return port
        except (OSError, serial.SerialException):
            continue
    return None

def reset_usb_bus():
    """Reset the entire USB bus to recover stuck devices"""
    try:
        logging.warning("Performing full USB bus reset...")
        subprocess.run(['sudo', 'modprobe', '-r', 'usbhid'], timeout=5)
        time.sleep(1)
        subprocess.run(['sudo', 'modprobe', 'usbhid'], timeout=5)
        time.sleep(2)
        logging.info("USB bus reset complete")
        return True
    except Exception as e:
        logging.error(f"USB reset failed: {e}")
        return False

def is_valid_data_line(line):
    """Check if line is a valid data message with the expected format"""
    return line.startswith(DATA_CODWORD + ",")

def parse_data_line(line):
    """Parse a valid data line into a dictionary"""
    try:
        values = line[len(DATA_CODWORD)+1:].split(',')
        if len(values) == 6:
            def safe_float(value):
                try:
                    val = float(value)
                    # Check if the value is NaN or infinite
                    if val != val or val == float('inf') or val == float('-inf'):  # NaN check
                        return None  # Use None instead of NaN for JSON compatibility
                    return val
                except ValueError:
                    return None  # Use None instead of 0.0 for invalid values
            
            return {
                "vert": safe_float(values[0]),
                "lat": safe_float(values[1]),
                "tors": safe_float(values[2]),
                "inPos": bool(int(values[3])),
                "tempoPos": int(values[4]),
                "conteggio": int(values[5]),
                "last_update": time.time()
            }
    except (ValueError, IndexError, TypeError) as e:
        logging.error(f"Data parsing error: {e} | Line: {line}")
    return None

def serial_reader():
    global sensor_data
    connection_attempts = 0
    teensy_port = None
    
    while True:
        if not teensy_port:
            teensy_port = find_teensy_port()
            if not teensy_port:
                logging.warning("Teensy not found. Retrying in 5 seconds...")
                time.sleep(5)
                continue
        
        try:
            with serial.Serial(teensy_port, 115200, timeout=1) as ser:
                # Reset DTR
                try:
                    ser.setDTR(False)
                    time.sleep(0.5)
                    ser.setDTR(True)
                    time.sleep(0.5)
                    ser.flushInput()
                except Exception as e:
                    logging.error(f"DTR toggle failed: {e}")
                
                logging.info(f"Connected to Teensy at {teensy_port}")
                
                # Send wake-up command
                ser.write(b'\n')
                
                # Wait for initialization
                logging.info("Waiting for button press and initialization...")
                start_time = time.time()
                initialized = False
                
                while time.time() - start_time < 120:
                    if ser.in_waiting:
                        raw_line = ser.readline()
                        try:
                            line = raw_line.decode('utf-8', errors='replace').strip()
                        except UnicodeDecodeError:
                            continue
                        
                        if INIT_START_MARKER in line:
                            logging.info("Teensy initialization started")
                            socketio.emit('system_status', {'status': 'initializing'})
                            continue
                            
                        if INIT_COMPLETE_MARKER in line:
                            logging.info("Teensy initialization complete")
                            initialized = True
                            socketio.emit('system_status', {'status': 'ready'})
                            break
                        
                        if line and is_valid_data_line(line):
                            parsed_data = parse_data_line(line)
                            if parsed_data:
                                sensor_data = parsed_data
                                # Emit to all connected clients
                                socketio.emit('sensor_update', parsed_data)
                                logging.info(f"Broadcasted data to {len(active_connections)} clients")
                    
                    time.sleep(0.1)
                
                if not initialized:
                    logging.error("Teensy initialization timed out")
                    teensy_port = None
                    continue
                
                # Main reading loop
                while True:
                    try:
                        if ser.in_waiting:
                            raw_line = ser.readline()
                            try:
                                line = raw_line.decode('utf-8', errors='replace').strip()
                            except UnicodeDecodeError:
                                continue
                            
                            if not line:
                                continue
                            
                            if INIT_START_MARKER in line:
                                logging.info("Re-initialization started")
                                socketio.emit('system_status', {'status': 'initializing'})
                                continue
                                
                            if INIT_COMPLETE_MARKER in line:
                                logging.info("Re-initialization complete")
                                socketio.emit('system_status', {'status': 'ready'})
                                continue
                            
                            if is_valid_data_line(line):
                                parsed_data = parse_data_line(line)
                                if parsed_data:
                                    sensor_data = parsed_data
                                    # Emit to all connected clients
                                    socketio.emit('sensor_update', parsed_data)
                            else:
                                if any(char.isdigit() for char in line) and len(line) > 10:
                                    logging.debug(f"Received non-data: {line}")
                    except serial.SerialException:
                        break
                    except Exception as e:
                        logging.error(f"Unexpected error: {e}")
        
        except serial.SerialException as e:
            logging.error(f"Serial connection error: {e}")
            teensy_port = None
            time.sleep(3)
        except Exception as e:
            logging.error(f"Unexpected error: {e}")
            teensy_port = None
            time.sleep(3)

# SocketIO event handlers
@socketio.on('connect')
def handle_connect():
    logging.info(f"Client connected: {request.sid}")
    active_connections.add(request.sid)
    # Send current data to new client
    emit('sensor_update', sensor_data)
    emit('system_status', {'status': 'connected', 'active_connections': len(active_connections)})

@socketio.on('disconnect')
def handle_disconnect():
    logging.info(f"Client disconnected: {request.sid}")
    active_connections.discard(request.sid)

@socketio.on('test_message')
def handle_test_message(data):
    logging.info(f"Test message received from {request.sid}: {data}")
    emit('test_response', {'status': 'success', 'received': data, 'server_time': time.time()})

# Routes
@app.route('/')
def index():
    return render_template('dashboard.html')
   
@app.route('/feedback')
def feedback():
    return render_template('feedback.html')

@app.route('/debug')
def debug():
    test_data = {
        "vert": 12.3,
        "lat": -4.5,
        "tors": 7.8,
        "inPos": True,
        "tempoPos": 1234,
        "conteggio": 42,
        "last_update": time.time()
    }
    socketio.emit('sensor_update', test_data)
    logging.info(f"Debug event sent to {len(active_connections)} clients")
    return f"Test event sent to {len(active_connections)} clients"

@app.route('/status')
def status():
    return {
        'active_connections': len(active_connections),
        'last_data_update': sensor_data.get('last_update', 0),
        'server_time': time.time(),
        'sensor_data': sensor_data
    }

if __name__ == '__main__':
    # Import request here to avoid circular import
    from flask import request
    
    # Grant USB permissions at startup
    os.system('sudo chmod a+rw /dev/tty* 2>/dev/null')
    
    # Start serial thread
    serial_thread = threading.Thread(target=serial_reader, daemon=True)
    serial_thread.start()
    
    # Connection monitoring
    def monitor_connections():
        while True:
            logging.info(f"Active WebSocket connections: {len(active_connections)}")
            # Send periodic heartbeat
            if active_connections:
                socketio.emit('heartbeat', {'server_time': time.time(), 'connections': len(active_connections)})
            time.sleep(30)
    
    monitor_thread = threading.Thread(target=monitor_connections, daemon=True)
    monitor_thread.start()
    
    # Start web server
    logging.info("Starting web server at http://0.0.0.0:5000")
    socketio.run(app, host='0.0.0.0', port=5000, debug=False)
