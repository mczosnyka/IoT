import paho.mqtt.client as mqtt
import json

reads = []
with open('sensor1_data.json', 'r') as file:
    reads = json.load(file)

print(type(reads))
print(reads)

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Połączono z brokerem")

    else:
        print(f"Connection failed with code {rc}")


def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    data = json.loads(payload)
    if msg.topic == "noise_detector_web/sensor1/noise_data":
        reads.append(data)
        with open('sensor1_data.json', 'w') as file:
            json.dump(reads, file, indent=4)

    print(f"Otrzymano wiadomość: '{msg.payload.decode()}' \n na topicu: '{msg.topic}'")


client = mqtt.Client()

client.on_connect = on_connect
client.on_message = on_message

broker_address = "mqtt.eclipseprojects.io"
port = 1883
client.connect(broker_address, port, 60)

topics = ["noise_detector_web/sensor1/noise_data", "noise_detector_web/sensor1/sensor_info"]

for topic in topics:
    client.subscribe(topic)
    print(f"Subskrybuje topic: {topic}")

client.loop_forever()
