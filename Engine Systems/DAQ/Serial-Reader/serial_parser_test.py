# SEDS UNLV SRADS Liquids Engine Systems 
# 4/8/2026

# Arduino Serial Parser TEST
# This program reads serial data from a test.csv file sends it to a Synnax port using the Synnax API, 
# which displays data using Synnax UI.

# Steps of this program:
# > Opens and reads each line of test.csv
# > Parses the values
# > Connects to Synnax at localhost:9090
# > Writes those values into Synnax channels

# The outgoing serial data from the arduino is in CSV-style and looks like this:
# > time_or_dash,pressure_1,pressure_2,pressure_3,pressure_4,pressure_5,temperature_f
# where:
# > time_or_dash = - when idle, or milliseconds since sequence start when running
# > pressure values from A0–A4
# > 1 temperature value from A5 converted to Fahrenheit

# TEST VERSION

import csv
import time
import synnax as sy

# =========================
# CONFIG
# =========================
HOST = "localhost"
PORT_API = 9090
CSV_FILE = "test.csv"
WRITE_INTERVAL_SEC = 0.5

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
  "active": get_or_create_data_channel("sequence_active", time_channel.key),
}

print("[INFO] Connected to Synnax")
print(f"[INFO] Reading simulated data from {CSV_FILE}")

# =========================
# REPLAY CSV INTO SYNNAX
# =========================
log_file = open("torch_log.csv", "a")

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
    "sequence_active"
  ],
) as writer:
  with open(CSV_FILE, "r", newline="") as f:
    reader = csv.reader(f)
    next(reader, None)

    for parts in reader:

      if len(parts) != 7:
        print("[WARN] Bad row:", parts)
        continue

      now_ts = sy.TimeStamp.now()

      try:
        frame = {
          "torch_time": now_ts,
          "pressure_1": float(parts[1]),
          "pressure_2": float(parts[2]),
          "pressure_3": float(parts[3]),
          "pressure_4": float(parts[4]),
          "pressure_5": float(parts[5]),
          "temperature_f": float(parts[6]),
        }

        if parts[0] != "-":
          frame["seq_time_ms"] = float(parts[0])
          frame["sequence_active"] = 1.0
        else:
          frame["seq_time_ms"] = -1.0
          frame["sequence_active"] = 0.0

      except Exception as e:
        print("[WARN] Parse error:", parts, e)
        continue

      writer.write(frame)

      log_file.write(",".join(parts) + "\n")
      log_file.flush()

      print(",".join(parts))

      if parts[0] != "-":
        print("[DATA]", frame)

      time.sleep(WRITE_INTERVAL_SEC)

  writer.commit()

log_file.close()
print("[INFO] Finished replaying test.csv")