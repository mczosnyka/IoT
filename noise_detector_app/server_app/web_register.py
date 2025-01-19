from flask import Flask, request, jsonify, render_template, redirect, url_for, session
import json
import os
import uuid
from werkzeug.security import generate_password_hash, check_password_hash
import paho.mqtt.client as mqtt
from datetime import datetime

app = Flask(__name__)
app.secret_key = 'your_secret_key'

# Files to store user and device data
USER_DB_FILE = 'users.json'
DEVICE_DB_FILE = 'devices.json'
MQTT_MESSAGES_FILE = 'mqtt_messages.json'

# MQTT settings
MQTT_BROKER_URL = "mqtt.eclipseprojects.io"
MQTT_BROKER_PORT = 1883


# Global variable to store received messages
received_messages = []
last_config = {}
with open(MQTT_MESSAGES_FILE, 'r') as file:
        messages_data = json.load(file)

def get_active_mac_address():
    if os.path.exists(DEVICE_DB_FILE) and os.path.getsize(DEVICE_DB_FILE) > 0:
        with open(DEVICE_DB_FILE, 'r') as file:
            devices = json.load(file)
            print(devices)
            global topics_to_subscribe
            topics_to_subscribe = []
            active_device = None
            global mqtt_topic, mqtt_config_topic, mqtt_sensor_topic
            for device in devices:
                processed_mac_address = device['mac_address'].replace(":","")
                if device.get('active'):
                    active_device = processed_mac_address
                    mqtt_topic = f"noise_detector_web/{processed_mac_address}/noise_data"
                    mqtt_config_topic = f"noise_detector_web/{processed_mac_address}/configuration_data"
                    mqtt_sensor_topic = f"noise_detector_web/{processed_mac_address}/sensor_info"
                # print(mqtt_topic)
                if f"noise_detector_web/{processed_mac_address}/noise_data" not in topics_to_subscribe:
                    topics_to_subscribe.append(f"noise_detector_web/{processed_mac_address}/noise_data")
                    topics_to_subscribe.append(f"noise_detector_web/{processed_mac_address}/sensor_info")
                # return device['mac_address']
    return active_device

get_active_mac_address()

# Helper function to load user data from JSON
def load_users():
    if os.path.exists(USER_DB_FILE) and os.path.getsize(USER_DB_FILE) > 0:
        with open(USER_DB_FILE, 'r') as file:
            return json.load(file)
    return []

# Helper function to save user data to JSON
def save_users(users):
    with open(USER_DB_FILE, 'w') as file:
        json.dump(users, file)

# Helper function to load device data from JSON
def load_devices():
    if os.path.exists(DEVICE_DB_FILE) and os.path.getsize(DEVICE_DB_FILE) > 0:
        with open(DEVICE_DB_FILE, 'r') as file:
            return json.load(file)
    return []

# Helper function to save device data to JSON
def save_devices(devices):
    with open(DEVICE_DB_FILE, 'w') as file:
        json.dump(devices, file)

# Helper function to save MQTT messages to JSON
def save_mqtt_message(message):
    messages_data["data"].append(message)
    with open(MQTT_MESSAGES_FILE, 'w') as file:
       json.dump(messages_data, file)

# Helper function to load MQTT messages from JSON
def load_mqtt_messages():
    if os.path.exists(MQTT_MESSAGES_FILE) and os.path.getsize(MQTT_MESSAGES_FILE) > 0:
        with open(MQTT_MESSAGES_FILE, 'r') as file:
            return json.load(file)
    return {"data": []}

# MQTT callbacks
def on_connect(client, userdata, flags, rc):
    print(f"Connected to MQTT broker with result code {rc}")
    print(f"Subscribing to topics: {topics_to_subscribe}")
    for topic in topics_to_subscribe:
        client.subscribe(topic)
    # client.subscribe(mqtt_topic)
    # client.subscribe(mqtt_sensor_topic)

def on_message(client, userdata, msg):
    print(f"Received message: {msg.topic} {msg.payload.decode()}")
    
    # devices = load_devices()
    # 
    # for device in devices:
        # if device['owner_id'] == session['user_id'] and device['active']:
            # mqtt_config_topic = f"noise_detector_web/{device['mac_address'].replace(":","")}/configuration_data"
            # mqtt_sensor_topic = f"noise_detector_web/{device['mac_address'].replace(":","")}/sensor_info"
    
    if msg.topic in [mqtt_sensor_topic, mqtt_config_topic]:
        # print("ENTERED IF")
        try:
            payload = msg.payload.decode().replace("'", '"')
            data = json.loads(payload)
            global last_config
            last_config = data.get("configuration_data", last_config)
            print("===================================== LAST CONFIG =====================================")
            print(last_config)
        except json.JSONDecodeError as e:
            print(f"JSON decode error: {e}")
        except Exception as e:
            print(f"Unexpected error: {e}")
    # if msg.topic == mqtt_topic:
    # print("ENTERED IF")
    message = {
        "timestamp": datetime.now().isoformat(),
        "topic": msg.topic,
        "payload": msg.payload.decode()
    }
    received_messages.append(msg.payload.decode())
    save_mqtt_message(message)

# Initialize MQTT client
mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(MQTT_BROKER_URL, MQTT_BROKER_PORT, 60)
mqtt_client.loop_start()

@app.route('/')
def home():
    if 'user_id' in session:
        return render_template('home.html', logged_in=True)
    return render_template('home.html', logged_in=False)

@app.route('/register', methods=['GET', 'POST'])
def register():
    if request.method == 'POST':
        username = request.form['username']
        password = request.form['password']
        
        users = load_users()
        
        # Check if username already exists
        if any(user['username'] == username for user in users):
            return "Username already exists!"
        
        # Create new user with unique ID
        new_user = {
            "id": str(uuid.uuid4()),
            "username": username,
            "password": generate_password_hash(password)
        }
        users.append(new_user)
        save_users(users)
        
        return redirect(url_for('home'))
    
    return render_template('register.html')

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form['username']
        password = request.form['password']
        
        users = load_users()
        
        user = next((user for user in users if user['username'] == username), None)
        
        if user and check_password_hash(user['password'], password):
            session['user_id'] = user['id']
            return redirect(url_for('dashboard'))
        else:
            return "Invalid username or password!"
    
    return render_template('login.html')

@app.route('/logout')
def logout():
    session.pop('user_id', None)
    return redirect(url_for('home'))

@app.route('/dashboard')
def dashboard():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    last_message = received_messages[-1] if received_messages else None
    return render_template('dashboard.html', message=last_message)

@app.route('/add_device', methods=['GET', 'POST'])
def add_device():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    
    if request.method == 'POST':
        mac_address = request.form['mac_address']
        devices = load_devices()
        
        # Check if the MAC address already exists
        if any(device['mac_address'] == mac_address for device in devices) or any(device['mac_address'].replace(":","") == mac_address for device in devices):
            return "Device with this MAC address already exists!"
        
        # Add new device
        new_device = {
            "mac_address": mac_address,
            "owner_id": session['user_id'],
            "active": False
        }
        devices.append(new_device)
        save_devices(devices)
        
        return redirect(url_for('device_list'))
    
    return render_template('add_device.html')

@app.route('/device_list')
def device_list():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    
    devices = load_devices()
    user_devices = [device for device in devices if device['owner_id'] == session['user_id']]
    
    return render_template('device_list.html', devices=user_devices)

@app.route('/messages')
def messages():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    
    return render_template('messages.html', messages=received_messages)

@app.route('/get_messages', methods=['GET'])
def get_messages():
    if 'user_id' not in session:
        return jsonify({"error": "Unauthorized"}), 401
    messages_data = load_mqtt_messages()

    devices = load_devices()

    res = []

    for device in devices:
        if device['owner_id'] == session['user_id'] and device['active']:
            print("ACTIVE DEVICE")
            mqtt_topic = f"noise_detector_web/{device['mac_address'].replace(":","")}/noise_data"
            res = [message for message in messages_data["data"] if message["topic"] == mqtt_topic]
            

    # messages_data["data"] = [message for message in messages_data["data"] if message["topic"] == mqtt_topic]
    # return jsonify({"messages": messages_data["data"][::-1]})  # Return messages in reverse order
    return jsonify({"messages": res[::-1]})

@app.route('/get_dashboard_messages', methods=['GET'])
def get_dashboard_messages():
    if 'user_id' not in session:
        return jsonify({"error": "Unauthorized"}), 401
    messages_data = load_mqtt_messages()
    devices = load_devices()

    res = []

    for device in devices:
        if device['owner_id'] == session['user_id'] and device['active']:
            print("ACTIVE DEVICE")
            mqtt_topic = f"noise_detector_web/{device['mac_address'].replace(":","")}/noise_data"
            mqtt_sensor_topic = f"noise_detector_web/{device['mac_address'].replace(":","")}/sensor_info"
            mqtt_config_topic = f"noise_detector_web/{device['mac_address'].replace(":","")}/configuration_data"
            res = [message for message in messages_data["data"] if message["topic"] == mqtt_topic]
            
    return jsonify({"messages": res})

@app.route('/config_device', methods=['GET', 'POST'])
def config_device():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    
    if request.method == 'POST':
        inverted_colors = 1 if request.form.get('inverted_colors') == 'on' else 0  # Handle 'on' value
        contrast = int(request.form.get('contrast', 255))
        yoffset = int(request.form.get('yoffset', 0))
        xoffset = int(request.form.get('xoffset', 0))
        threshold = float(request.form.get('threshold', 0.0))
        
        config_data = {
            "screen_invert_colors": inverted_colors,
            "screen_contrast": contrast,
            "screen_yoffset": yoffset,
            "screen_xoffset": xoffset,
            "LED_db_treshold": threshold
        }

        devices = load_devices()

        for device in devices:
            if device['owner_id'] == session['user_id'] and device['active']:
                mqtt_config_topic = f"noise_detector_web/{device['mac_address'].replace(":","")}/configuration_data"
        
        mqtt_client.publish(mqtt_config_topic, json.dumps({"configuration_data": config_data}))
        
        return "Device configuration sent successfully!"
    
    return render_template('config_device.html', config=last_config)

@app.route('/set_active_device', methods=['POST'])
def set_active_device():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    
    active_device_mac = request.form['active_device']
    devices = load_devices()
    
    for device in devices:
        if device['owner_id'] == session['user_id']:
            device['active'] = (device['mac_address'] == active_device_mac)
    
    save_devices(devices)
    get_active_mac_address()
    return redirect(url_for('device_list'))

@app.route('/handover_device', methods=['POST'])
def handover_device():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    
    device_mac = request.form['device_mac']
    new_owner_username = request.form['new_owner_username']
    
    users = load_users()
    devices = load_devices()
    
    new_owner = next((user for user in users if user['username'] == new_owner_username), None)
    
    if not new_owner:
        return "New owner does not exist!"
    
    if not any(device['mac_address'] == device_mac for device in devices):
        return "Device does not exist!"
    
    for device in devices:
        if device['mac_address'] == device_mac and device['owner_id'] == session['user_id']:
            device['owner_id'] = new_owner['id']
            device['active'] = False  # Reset active status when handing over
        elif device['mac_address'] == device_mac and device['owner_id'] != session['user_id']:
            return "You do not own this device!"
    
    save_devices(devices)
    return redirect(url_for('device_list'))

if __name__ == '__main__':
    app.run(debug=True)
