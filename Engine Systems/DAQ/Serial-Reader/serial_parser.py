# SEDS UNLV SRADS Liquids Engine Systems
# 4/8/2026

# Arduino Serial Parser
# This program reads serial data from the SPARK-2-Daedalus Torch Igniter Arduino
# and sends it to Synnax using the Synnax API for visualization in Synnax UI.
#
# Steps of this program:
# > Opens the Arduino serial port
# > Reads each CSV line
# > Parses the values
# > Connects to Synnax at localhost:9090
# > Writes those values into Synnax channels
#
# The outgoing serial data from the Arduino is in CSV-style and looks like this:
# > time_or_dash,pressure_1,pressure_2,pressure_3,pressure_4,pressure_5,temperature_f
# where:
# > time_or_dash = - when idle, or milliseconds since sequence start when running
# > pressure values from A0–A4
# > 1 temperature value from A5 converted to Fahrenheit

import time
import serial
import synnax as sy

# =========================
# CONFIG
# =========================
SERIAL_PORT = "COM3"  # check if this needs to be changed
BAUD = 115200

HOST = "localhost"
PORT_API = 9090

# =========================
# CONNECT TO SYNNAX
# =========================
client = sy.Synnax(
  host=HOST,
  port=PORT_API,
  username="synnax",
  password="seldon",
  secure=False,
)

def get_or_create_index_channel(name):
  try:
    return client.channels.retrieve(name)
  except Exception:
    return client.channels.create(
      name=name,
      is_index=True,
      data_type="timestamp",
      retrieve_if_name_exists=True,
    )

def get_or_create_data_channel(name, index_key):
  try:
    return client.channels.retrieve(name)
  except Exception:
    return client.channels.create(
      name=name,
      index=index_key,
      data_type="float32",
      retrieve_if_name_exists=True,
    )

time_channel = get_or_create_index_channel("torch_time")

channels = {
  "seq_time_ms": get_or_create_data_channel("seq_time_ms", time_channel.key),
  "p1": get_or_create_data_channel("pressure_1", time_channel.key),
  "p2": get_or_create_data_channel("pressure_2", time_channel.key),
  "p3": get_or_create_data_channel("pressure_3", time_channel.key),
  "p4": get_or_create_data_channel("pressure_4", time_channel.key),
  "p5": get_or_create_data_channel("pressure_5", time_channel.key),
  "temp": get_or_create_data_channel("temperature_f", time_channel.key),
}

print("[INFO] Connected to Synnax")

# =========================
# CONNECT TO SERIAL
# =========================
def connect_serial():
  while True:
    try:
      ser = serial.Serial(SERIAL_PORT, BAUD, timeout=1)
      print(f"[INFO] Connected to Arduino on {SERIAL_PORT}")
      return ser
    except Exception as e:
      print("[WARN] Serial connection failed, retrying...", e)
      time.sleep(2)

# =========================
# MAIN LOOP
# =========================
ser = connect_serial()

with client.open_writer(
  start=sy.TimeStamp.now(),
  channels=[
    "torch_time",
    "seq_time_ms",
    "pressure_1",
    "pressure_2",
    "pressure_3",
    "pressure_4",
    "pressure_5",
    "temperature_f",
  ],
) as writer:
  while True:
    try:
      line = ser.readline().decode(errors="ignore").strip()

      if not line:
        continue

      parts = line.split(",")

      if len(parts) < 7:
        print("[WARN] Bad line:", line)
        continue

      try:
        frame = {
          "torch_time": sy.TimeStamp.now(),
          "pressure_1": float(parts[1]),
          "pressure_2": float(parts[2]),
          "pressure_3": float(parts[3]),
          "pressure_4": float(parts[4]),
          "pressure_5": float(parts[5]),
          "temperature_f": float(parts[6]),
        }

        if parts[0] != "-":
          frame["seq_time_ms"] = float(parts[0])
        else:
          frame["seq_time_ms"] = -1.0

      except Exception as e:
        print("[WARN] Parse error:", line, e)
        continue

      writer.write(frame)

      if parts[0] != "-":
        print("[DATA]", frame)

    except serial.SerialException:
      print("[ERROR] Serial disconnected. Reconnecting...")
      try:
        ser.close()
      except Exception:
        pass
      ser = connect_serial()

    except Exception as e:
      print("[ERROR]", e)