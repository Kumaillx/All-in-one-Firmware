import os
import threading
from datetime import datetime
import paho.mqtt.client as mqtt

# MQTT_HOST = "203.135.63.48"
MQTT_HOST = "203.135.63.22"
MQTT_PORT = 1883
MQTT_RECEIVE_TOPIC = "test/restart/#"  # appended by sensorID and fileName (# is wild)
PARENT_DIRECTORY = r"/home/neubolt/Python Scripts"  # replace with directory where script is located
file_objects = {}  # Dictionary to store File objects with keys as sensorName/fileName


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


def onDataReceive(client, userdata, msg):
    # extract directory and file name from topic
    subTopics = msg.topic.split("/")
    sensorName = subTopics[-2]  # directory name is sensorID
    fileName = subTopics[-1]
    # create or get the File object
    file_key = f"{sensorName}/{fileName}"
    if file_key not in file_objects:
        timeStamp = datetime.now().strftime("%Y-%m-%d--%H-%M-%S")
        # create sensor_id directory if it doesn't exist
        sensorDirPath = os.path.join(PARENT_DIRECTORY, sensorName)
        if not os.path.exists(sensorDirPath):
            os.mkdir(sensorDirPath)
        # create timestamp directory if it doesn't exist
        dirPath = os.path.join(sensorDirPath, timeStamp)
        if not os.path.exists(dirPath):
            os.mkdir(dirPath)
        # create file if it doesn't exist
        filePath = os.path.join(dirPath, fileName) + ".txt"
        try:
            with open(filePath, "x"):
                print(f"File '{filePath}' created successfully.")
        except FileExistsError:
            pass
        file_objects[file_key] = File(os.path.join(dirPath, fileName))
    # process the message using the File object
    file_objects[file_key].processMessage(msg.payload.decode("utf-8"))
    if file_objects[file_key].isWritten:
        del file_objects[file_key]


def onConnect(client, userdata, flags, rc):
    client.subscribe(MQTT_RECEIVE_TOPIC)


client = mqtt.Client()
client.on_connect = onConnect
client.on_message = onDataReceive
client.connect(MQTT_HOST, MQTT_PORT, 60)


def mqttProcess():
    client.loop_forever()


mqttBackgroundThread = threading.Thread(target=mqttProcess)
mqttBackgroundThread.start()
print("MQTT Process Started")
