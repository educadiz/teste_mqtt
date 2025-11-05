import requests
import random
import time
from datetime import datetime
import threading

import paho.mqtt.client as mqtt

# ThingSpeak Configuration
WRITE_API_KEY = "2PI8MD4NVFEY9XSZ"
BASE_URL = "https://api.thingspeak.com/update.json"

# MQTT Configuration
MQTT_BROKER_HOST = "broker.hivemq.com"  # "mqtt://" schema is not used by the client lib
MQTT_BROKER_PORT = 1883
MQTT_TOPICS = {
    "temp": "est_01/temp",
    "umid": "est_01/umid",
    "solar": "est_01/solar",
}


def build_mqtt_client() -> mqtt.Client:
    """
    Create and connect an MQTT client. Runs network loop in background.
    Returns a connected client (best effort) and handles auto-reconnect.
    """

    client = mqtt.Client(client_id=f"simulator-{random.randint(1000, 9999)}", clean_session=True)

    # Optional callbacks for visibility/debug
    def on_connect(cl, userdata, flags, rc, properties=None):
        status = "success" if rc == 0 else f"failed rc={rc}"
        print(f"[MQTT] Connect {status} to {MQTT_BROKER_HOST}:{MQTT_BROKER_PORT}")

    def on_disconnect(cl, userdata, rc, properties=None):
        if rc != 0:
            print("[MQTT] Unexpected disconnect, will auto-reconnect…")
        else:
            print("[MQTT] Disconnected")

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect

    # Start network loop in background thread
    client.loop_start()

    # Try initial connect (non-blocking retry on failure)
    try:
        client.connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT, keepalive=30)
    except Exception as e:
        print(f"[MQTT] Initial connect error: {e}")

    return client

def generate_random_data():
    # Generate random values within realistic ranges
    temperature = round(random.uniform(15.0, 35.0), 2)  # Temperature in Celsius
    humidity = round(random.uniform(30.0, 90.0), 2)     # Humidity in percentage
    insolation = round(random.uniform(0.0, 100.0), 2)  # Insolation in W/m²
    return temperature, humidity, insolation

def main():
    headers = {
        'Content-Type': 'application/json'
    }

    mqtt_client = build_mqtt_client()

    try:
        while True:
            # Generate random data
            temp, hum, insol = generate_random_data()
            
            # Get current timestamp
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            
            # Prepare data for ThingSpeak
            data = {
                'api_key': WRITE_API_KEY,
                'field1': str(temp),
                'field2': str(hum),
                'field3': str(insol)
            }
            
            # Send data to ThingSpeak
            print(f"\n{timestamp}")
            print(f"Sending - Temperature: {temp}°C")
            print(f"Sending - Humidity: {hum}%")
            print(f"Sending - Insolation: {insol} W/m²")
            print(f"Sending - Chuva: off")
            print(f"Sending - Alerta Temperatura: off")
            response = requests.post(BASE_URL, json=data, headers=headers)
            if response.status_code == 200:
                print(f"Data sent successfully!")
            else:
                print(f"Failed to send data. Status code: {response.status_code}")
                print(f"Response: {response.text}")

            # Publish to MQTT topics (best-effort)
            try:
                mqtt_client.publish(MQTT_TOPICS["temp"], str(temp), qos=0, retain=False)
                mqtt_client.publish(MQTT_TOPICS["umid"], str(hum), qos=0, retain=False)
                mqtt_client.publish(MQTT_TOPICS["solar"], str(insol), qos=0, retain=False)
                print("[MQTT] Published to topics:")
                print(f"  {MQTT_TOPICS['temp']}: {temp}")
                print(f"  {MQTT_TOPICS['umid']}: {hum}")
                print(f"  {MQTT_TOPICS['solar']}: {insol}")
            except Exception as e:
                print(f"[MQTT] Publish error: {e}")
            
            # Wait for 15 seconds before next update
            # ThingSpeak has a limitation of 15 seconds between updates
            time.sleep(15)
    except KeyboardInterrupt:
        print("\nExiting...")
    except Exception as e:
        print(f"Error occurred: {e}")
    finally:
        # Graceful MQTT shutdown
        try:
            mqtt_client.loop_stop()
            mqtt_client.disconnect()
        except Exception:
            pass

if __name__ == "__main__":
    main()