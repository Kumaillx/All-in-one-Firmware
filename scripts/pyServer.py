import paho.mqtt.client as mqtt
import threading

MQTT_HOST = "203.135.63.22"
# MQTT_HOST = "203.135.63.48"
MQTT_PORT = 1883
SENSOR_ID = "testkit_atta"
MQTT_SEND_TOPIC = f"pull/{SENSOR_ID}"
MQTT_RECEIVE_TOPIC = f"push/{SENSOR_ID}"
files = list()


class File:
    def __init__(self, name):
        self.name = name
        self.size = None
        self.bytesRead = 0
        self.isFirstReceive = True
        open(f"{self.name}.txt", "w")

    def processMessage(self, payload: str):
        if self.isFirstReceive:
            if payload == "Not Found":
                print(f"File {self.name} does not exist")
                return
            assert payload.isdecimal()
            self.size = int(payload)
            self.isFirstReceive = False
            print(f"File {self.name} has size {self.size}")
        elif self.bytesRead < self.size:
            with open(
                f"{self.name}.txt", "a", newline="\n"
            ) as f:  # newline='\n' removes duplicate newlines
                f.write(payload)
            self.bytesRead += len(payload)

    @property
    def isWritten(self):
        return self.bytesRead == self.size


def isValidDate(date: str):
    """Checks if date in YYYYMMDD format is valid*. All checks not implemented."""
    if len(date) != 8 or not date.isdecimal():
        return False
    year = int(date[:4])
    month = int(date[4:6])
    day = int(date[6:8])
    if year < 2000 or month > 12 or day > 31:
        return False
    return True


def onDataReceive(client, userdata, msg):
    for file in files:
        if msg.topic == f"{MQTT_RECEIVE_TOPIC}/{file.name}":
            file.processMessage(msg.payload.decode("utf-8"))
            if file.isWritten:
                client.unsubscribe(f"{MQTT_RECEIVE_TOPIC}/{file.name}")
                print(f"File {file.name} written successfully")
                files.remove(file)
                return


def onConnect(client, userdata, flags, rc):
    client.subscribe(MQTT_RECEIVE_TOPIC)


client = mqtt.Client()
client.on_message = onDataReceive
client.on_connect = onConnect
client.connect(MQTT_HOST, MQTT_PORT, 60)


def mqttProcess():
    client.loop_forever()


mqttBackgroundThread = threading.Thread(target=mqttProcess)
mqttBackgroundThread.start()

while True:
    request = input("Date(s): ")
    dates = [date.strip() for date in request.split(",") if isValidDate(date.strip())]
    print(f"Requested data for {dates}")
    for date in dates:
        files.append(File(date))
        client.subscribe(f"{MQTT_RECEIVE_TOPIC}/{date}")
    client.publish(MQTT_SEND_TOPIC, request)
