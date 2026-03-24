import customtkinter as ctk
import json
import paho.mqtt.client as mqtt
from tkinter import ttk  # for table widget

# ---------------- MQTT CONFIG ---------------- #
MQTT_BROKER = "203.135.63.47"
MQTT_PORT = 1883

# ---------------- GUI SETUP ---------------- #
ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("blue")

app = ctk.CTk()
app.title("Smart Meter MQTT GUI")
app.geometry("750x500")

# ---------------- GUI ELEMENTS ---------------- #
ctk.CTkLabel(app, text="Enter Sensor ID:", text_color="white").pack(pady=(10, 2))
sensor_id_entry = ctk.CTkEntry(app, width=200, placeholder_text="e.g. SM9-000")
sensor_id_entry.pack()

status_label = ctk.CTkLabel(app, text="Disconnected", text_color="yellow")
status_label.pack(pady=(5, 10))

output_box = ctk.CTkTextbox(app, width=700, height=120, wrap="word")
output_box.pack(pady=10)

# ---------------- TABLE FRAME ---------------- #
table_frame = ctk.CTkFrame(app)
table_frame.pack(pady=10, fill="both", expand=True)

columns = ("Slot", "Slot Factor", "Voltage Calib", "Current Calib")
tree = ttk.Treeview(table_frame, columns=columns, show="headings", height=8)

for col in columns:
    tree.heading(col, text=col)
    tree.column(col, width=150, anchor="center")

tree.pack(side="left", fill="both", expand=True)

# Add scrollbar
scrollbar = ttk.Scrollbar(table_frame, orient="vertical", command=tree.yview)
scrollbar.pack(side="right", fill="y")
tree.configure(yscroll=scrollbar.set)

# ---------------- MQTT CALLBACKS ---------------- #
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        status_label.configure(text="Connected to MQTT Broker ✅", text_color="lightgreen")
        output_box.insert("end", f"Connected to {MQTT_BROKER}:{MQTT_PORT}\n\n")
    else:
        status_label.configure(text=f"Connection failed (code {rc})", text_color="red")

def on_message(client, userdata, msg):
    message = msg.payload.decode()
    output_box.insert("end", f"Received → {msg.topic}: {message}\n\n")
    output_box.see("end")

    try:
        data = json.loads(message)
        update_table(data)
    except json.JSONDecodeError:
        pass  # not a JSON message, ignore

# ---------------- UPDATE TABLE ---------------- #
def update_table(data):
    # Clear previous rows
    for row in tree.get_children():
        tree.delete(row)

    slot_factors = data.get("slotFactors", [])
    voltage_cal = data.get("voltageCalibration", [])
    current_cal = data.get("currentCalibration", [])
    sd_f = data.get("sd_f", None)

    for i in range(len(slot_factors)):
        v = voltage_cal[i] if i < len(voltage_cal) else ""
        c = current_cal[i] if i < len(current_cal) else ""
        tree.insert("", "end", values=(i + 1, slot_factors[i], v, c))

    # Show sd_f flag at bottom
    if sd_f is not None:
        output_box.insert("end", f"SD Flag: {sd_f}\n\n")

# ---------------- MQTT SETUP ---------------- #
mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

def connect_mqtt():
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_start()
        status_label.configure(text="Connecting...", text_color="yellow")
    except Exception as e:
        status_label.configure(text=f"Connection Error: {e}", text_color="red")

connect_mqtt()

# ---------------- FETCH PARAMS FUNCTION ---------------- #
def fetch_params():
    sensor_id = sensor_id_entry.get().strip()
    if not sensor_id:
        status_label.configure(text="Please enter a Sensor ID first!", text_color="orange")
        return

    pull_topic = f"pull/{sensor_id}"
    push_topic = f"push/{sensor_id}/params"

    mqtt_client.subscribe(push_topic)
    output_box.insert("end", f"Subscribed to: {push_topic}\n")

    mqtt_client.publish(pull_topic, "/params")
    output_box.insert("end", f"Published '/params' → {pull_topic}\n\n")
    output_box.see("end")

# ---------------- BUTTON ---------------- #
fetch_button = ctk.CTkButton(app, text="Fetch Params", command=fetch_params)
fetch_button.pack(pady=10)

app.mainloop()
