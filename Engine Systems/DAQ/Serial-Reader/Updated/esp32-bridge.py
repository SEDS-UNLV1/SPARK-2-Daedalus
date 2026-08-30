# SEDS UNLV SRADS Liquids Engine Systems
# SPARK-2 Daedalus - Torch Igniter
#
# torch_bridge.py - bidirectional Synnax bridge
#
# Replaces serial_parser.py. Does two jobs at once:
#
#   READ  : Arduino CSV  ->  Synnax  (pressures, temp, AND valve states)
#   WRITE : Synnax valve commands  ->  Arduino serial chars
#
# This is what makes the valve symbols on the schematic actually work instead of
# being decorative.
#
# REQUIRES FIRMWARE_PATCH.txt to be applied first. Without Patch 3 the Arduino
# sends 7 fields instead of 14 and the valve states will never populate.
#
# TRANSPORT
#   Set USE_WIFI below. False = USB cable (COM3). True = ESP32 bridge over TCP.
#   Everything else is identical. Get it working on USB first.

import socket
import threading
import time

import serial
import synnax as sy

# =========================
# CONFIG
# =========================
USE_WIFI = False            # False = USB, True = ESP32 bridge

SERIAL_PORT = "COM3"
BAUD = 115200

BRIDGE_HOST = "192.168.4.1"
BRIDGE_PORT = 8080

HOST = "localhost"
PORT_API = 9090

HEARTBEAT_INTERVAL = 0.25   # seconds; must be well under the Arduino's 1s timeout


# =========================
# VALVE MAP
# =========================
# Maps each valve to its Arduino serial commands and its P&ID behavior.
#
# open_cmd / close_cmd come from handleSerialCommands() in the master sketch.
#
# normally_open: TRUE for the two vent solenoids. On those, energizing the SSR
# CLOSES the valve. If you do not account for this the schematic shows vents
# backwards - green when they are actually shut. Both vents on the P&ID are
# marked NO; every other valve is NC.

VALVES = [
    # name              open_cmd  close_cmd  normally_open  pid_note
    ("fuel_line",       "a",      "b",       False,  "Kero feed to manifold, NC"),
    ("fuel_vent",       "c",      "d",       True,   "Kero tank ullage vent, NO"),
    ("ox_vent",         "e",      "f",       True,   "GOx line vent, NO"),
    ("n2_tank",         "g",      "h",       False,  "N2 tank iso solenoid, NC"),
    ("n2_line",         "i",      "j",       False,  "N2 purge to manifold, NC"),
    ("ox_line",         "k",      "l",       False,  "GOx feed to manifold, NC"),
    ("ox_tank",         "m",      "n",       False,  "GOx tank iso solenoid, NC"),
]

# Field index in the patched 14-field CSV row, in the same order the firmware
# prints them (see FIRMWARE_PATCH.txt Patch 3).
VALVE_FIELD_START = 7


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

time_channel = client.channels.create(
    name="torch_time",
    data_type=sy.DataType.TIMESTAMP,
    is_index=True,
    retrieve_if_name_exists=True,
)


def data_channel(name, dtype=sy.DataType.FLOAT32):
    return client.channels.create(
        name=name,
        data_type=dtype,
        index=time_channel.key,
        retrieve_if_name_exists=True,
    )


def command_channel(name):
    # Virtual channels are not persisted - they exist only to carry a command
    # from the console to this script.
    return client.channels.create(
        name=name,
        data_type=sy.DataType.UINT8,
        virtual=True,
        retrieve_if_name_exists=True,
    )


# --- sensor channels ---
SENSOR_CHANNELS = [
    "seq_time_ms",
    "pressure_1",
    "pressure_2",
    "pressure_3",
    "pressure_4",
    "pressure_5",
    "temperature_f",
    "sequence_active",
]
for name in SENSOR_CHANNELS:
    data_channel(name)

# --- valve state channels (what the board reports) ---
STATE_CHANNELS = [f"{v[0]}_state" for v in VALVES]
for name in STATE_CHANNELS:
    data_channel(name, sy.DataType.UINT8)

# --- valve command channels (what the console clicks) ---
CMD_CHANNELS = [f"{v[0]}_cmd" for v in VALVES]
for name in CMD_CHANNELS:
    command_channel(name)

# --- sequence control ---
command_channel("sequence_cmd")   # 1 = startSeq, 0 = abort

print("[INFO] Connected to Synnax")
print(f"[INFO] {len(CMD_CHANNELS)} valve command channels ready")


# =========================
# TRANSPORT
# =========================
# One lock guards the link. The reader thread and the command thread both touch it.
link_lock = threading.Lock()


class Link:
    """Wraps either a serial port or a TCP socket behind readline()/write()."""

    def __init__(self):
        self.ser = None
        self.sock = None
        self.stream = None
        self.connect()

    def connect(self):
        while True:
            try:
                if USE_WIFI:
                    self.sock = socket.create_connection(
                        (BRIDGE_HOST, BRIDGE_PORT), timeout=5
                    )
                    self.sock.settimeout(2.0)
                    self.stream = self.sock.makefile("rb")
                    print(f"[INFO] Connected to ESP32 at {BRIDGE_HOST}:{BRIDGE_PORT}")
                else:
                    self.ser = serial.Serial(SERIAL_PORT, BAUD, timeout=1)
                    print(f"[INFO] Connected to Arduino on {SERIAL_PORT}")
                return
            except Exception as e:
                print("[WARN] Link connection failed, retrying...", e)
                time.sleep(2)

    def readline(self):
        if USE_WIFI:
            return self.stream.readline()
        return self.ser.readline()

    def write(self, data: bytes):
        with link_lock:
            if USE_WIFI:
                self.sock.sendall(data)
            else:
                self.ser.write(data)

    def reconnect(self):
        try:
            if USE_WIFI:
                self.stream.close()
                self.sock.close()
            else:
                self.ser.close()
        except Exception:
            pass
        self.connect()


link = Link()

# Give the Arduino a moment - opening a serial port toggles DTR and resets it.
if not USE_WIFI:
    time.sleep(2.0)


# =========================
# COMMAND THREAD  (Synnax -> Arduino)
# =========================
def command_worker():
    """Watch the valve command channels and translate clicks into serial chars."""
    lookup = {f"{v[0]}_cmd": (v[1], v[2], v[0]) for v in VALVES}

    while True:
        try:
            with client.open_streamer(CMD_CHANNELS + ["sequence_cmd"]) as streamer:
                for frame in streamer:
                    for ch in frame.channels:
                        series = frame[ch]
                        if len(series) == 0:
                            continue
                        value = int(series[-1])

                        if ch == "sequence_cmd":
                            cmd = b"startSeq\n" if value else b"x\n"
                            link.write(cmd)
                            print(f"[CMD] sequence -> {cmd.decode().strip()}")
                            continue

                        open_cmd, close_cmd, name = lookup[ch]
                        char = open_cmd if value else close_cmd
                        link.write((char + "\n").encode())
                        print(f"[CMD] {name} -> {'OPEN' if value else 'CLOSE'} ({char})")

        except Exception as e:
            print("[ERROR] Command stream died, restarting:", e)
            time.sleep(2)


# =========================
# HEARTBEAT THREAD
# =========================
def heartbeat_worker():
    """Keep the Arduino's link watchdog fed (Patch 2)."""
    while True:
        try:
            link.write(b"~\n")
        except Exception:
            pass
        time.sleep(HEARTBEAT_INTERVAL)


threading.Thread(target=command_worker, daemon=True).start()
threading.Thread(target=heartbeat_worker, daemon=True).start()


# =========================
# READER LOOP  (Arduino -> Synnax)
# =========================
ALL_WRITE_CHANNELS = ["torch_time"] + SENSOR_CHANNELS + STATE_CHANNELS

warned_short_row = False

with client.open_writer(
    start=sy.TimeStamp.now(),
    channels=ALL_WRITE_CHANNELS,
) as writer:

    last_commit = time.time()

    while True:
        try:
            raw = link.readline()
            if not raw:
                if USE_WIFI:
                    raise ConnectionResetError("bridge closed")
                continue

            line = raw.decode(errors="ignore").strip()
            if not line:
                continue

            parts = line.split(",")

            # Status text from the Arduino ("PREP: ...", "ABORT: ...", etc.)
            # shares this stream. Surface it instead of discarding it.
            if len(parts) < 7:
                print("[MEGA]", line)
                continue

            if len(parts) < 14 and not warned_short_row:
                print("[WARN] Row has %d fields, expected 14." % len(parts))
                print("[WARN] Valve states will stay blank until FIRMWARE_PATCH")
                print("[WARN] Patch 3 is applied.")
                warned_short_row = True

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
                    frame["sequence_active"] = 1.0
                else:
                    frame["seq_time_ms"] = -1.0
                    frame["sequence_active"] = 0.0

                # Valve states, if the firmware is patched
                for i, (name, _, _, normally_open, _) in enumerate(VALVES):
                    idx = VALVE_FIELD_START + i
                    if idx < len(parts):
                        pin = int(parts[idx])
                        # Normalize so 1 ALWAYS means "flowing / open" on the
                        # schematic, regardless of NC vs NO wiring.
                        is_open = (not pin) if normally_open else bool(pin)
                        frame[f"{name}_state"] = 1 if is_open else 0
                    else:
                        frame[f"{name}_state"] = 0

            except ValueError:
                print("[WARN] Parse error:", line)
                continue

            writer.write(frame)

            if time.time() - last_commit > 1.0:
                writer.commit()
                last_commit = time.time()

        except (serial.SerialException, ConnectionResetError, OSError) as e:
            print("[ERROR] Link lost, reconnecting...", e)
            link.reconnect()

        except KeyboardInterrupt:
            print("\n[INFO] Shutting down. Sending abort to be safe.")
            try:
                link.write(b"x\n")
            except Exception:
                pass
            writer.commit()
            break

        except Exception as e:
            print("[ERROR]", e)