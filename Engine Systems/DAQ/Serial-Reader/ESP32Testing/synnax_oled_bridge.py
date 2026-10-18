# SEDS UNLV SRADS Liquids Engine Systems
# SPARK-2 Daedalus - Torch Igniter
#
# synnax_oled_bridge.py - laptop half of the esp32-synnax-oled bench test
#
#   READ  : ESP32 test rows   ->  Synnax pressure_1..pressure_5 (indexed by torch_time)
#   WRITE : Synnax *_cmd      ->  ESP32 OLED "Command <n>: <channel> received"
#
# Same job as esp32-bridge.py, but for the test board instead of the Mega. Commands go
# down as "C,<channel>,<value>" instead of single letters, so the ESP32 can show which
# channel was written.
#
# The test rows go into the real pressure channels, so the existing schematic and line
# plot show them. Rows are timestamped when they reach the laptop.
#
# RUN (from Serial-Reader, with its venv - its synnax client matches the server)
#   WiFi: join the ESP32's network (AP_SSID in esp32-synnax-oled/secrets.h), then
#         venv/bin/python "Updated/ESP32Testing/synnax_oled_bridge.py"
#   USB:  venv/bin/python "Updated/ESP32Testing/synnax_oled_bridge.py" --usb

import argparse
import socket
import threading
import time

import serial
import synnax as sy

# =========================
# CONFIG
# =========================
# SERIAL_PORT = "COM3" #for windows
SERIAL_PORT = "/dev/cu.usbserial-0001"  # for linux/mac
BAUD = 115200                # must match USB_BAUD in the sketch

BRIDGE_HOST = "192.168.4.1"
BRIDGE_PORT = 8080           # must match TCP_PORT in the sketch

HOST = "localhost"
PORT_API = 9090

HEARTBEAT_INTERVAL = 0.5     # seconds; the ESP32 marks the link dead after 2 s
READ_TIMEOUT = 2.0           # seconds without a line before WiFi reconnects
STATS_INTERVAL = 5.0

INDEX_CHANNEL = "torch_time"
PRESSURE_CHANNELS = [f"pressure_{i}" for i in range(1, 6)]
COMMAND_CHANNELS = [
    "fuel_line_cmd",
    "fuel_vent_cmd",
    "ox_vent_cmd",
    "n2_tank_cmd",
    "n2_line_cmd",
    "ox_line_cmd",
    "ox_tank_cmd",
    "sequence_cmd",
]

parser = argparse.ArgumentParser(description="Bridge the esp32-synnax-oled test board to Synnax.")
parser.add_argument("--usb", action="store_true", help="use the USB cable instead of WiFi")
parser.add_argument("--port", default=SERIAL_PORT, help=f"serial port for --usb (default {SERIAL_PORT})")
parser.add_argument("--synnax", default=f"{HOST}:{PORT_API}", help="Synnax server host:port")
args = parser.parse_args()


# =========================
# CONNECT TO SYNNAX
# =========================
synnax_host, synnax_port = args.synnax.rsplit(":", 1)
client = sy.Synnax(
    host=synnax_host,
    port=int(synnax_port),
    username="synnax",
    password="seldon",
    secure=False,
)

time_channel = client.channels.create(
    name=INDEX_CHANNEL,
    data_type=sy.DataType.TIMESTAMP,
    is_index=True,
    retrieve_if_name_exists=True,
)
for name in PRESSURE_CHANNELS:
    client.channels.create(
        name=name,
        data_type=sy.DataType.FLOAT32,
        index=time_channel.key,
        retrieve_if_name_exists=True,
    )
for name in COMMAND_CHANNELS:
    # Virtual: not stored, they only carry a click from the Console to this script.
    client.channels.create(
        name=name,
        data_type=sy.DataType.UINT8,
        virtual=True,
        retrieve_if_name_exists=True,
    )

print(f"[INFO] Connected to Synnax at {args.synnax}")


# =========================
# TRANSPORT
# =========================
class Link:
    """Wraps either the TCP socket or the serial port behind readline()/write()."""

    def __init__(self, use_usb, port):
        self.use_usb = use_usb
        self.port = port
        self.lock = threading.Lock()
        self.ser = None
        self.sock = None
        self.stream = None
        self.connect()

    def connect(self):
        while True:
            try:
                if self.use_usb:
                    ser = serial.Serial()
                    ser.port = self.port
                    ser.baudrate = BAUD
                    ser.timeout = READ_TIMEOUT
                    # Leave DTR/RTS released: on ESP32 dev boards they drive EN and GPIO0,
                    # and toggling them can reset the board or hold it in the bootloader.
                    ser.dtr = False
                    ser.rts = False
                    ser.open()
                    self.ser = ser
                    print(f"[INFO] Connected to ESP32 on {self.port}")
                else:
                    sock = socket.create_connection((BRIDGE_HOST, BRIDGE_PORT), timeout=5)
                    sock.settimeout(READ_TIMEOUT)
                    # Send each heartbeat/command immediately instead of letting Nagle
                    # hold small writes back.
                    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    self.sock = sock
                    self.stream = sock.makefile("rb")
                    print(f"[INFO] Connected to ESP32 at {BRIDGE_HOST}:{BRIDGE_PORT}")
                return
            except Exception as e:
                print("[WARN] Link connection failed, retrying...", e)
                time.sleep(2)

    def readline(self):
        """One line from the ESP32. b"" means the serial read timed out or TCP closed."""
        if self.use_usb:
            return self.ser.readline()
        return self.stream.readline()

    def write(self, data: bytes):
        with self.lock:
            if self.use_usb:
                self.ser.write(data)
            else:
                self.sock.sendall(data)

    def reconnect(self):
        with self.lock:
            try:
                if self.use_usb:
                    self.ser.close()
                else:
                    self.stream.close()
                    self.sock.close()
            except Exception:
                pass
        time.sleep(1.0)
        self.connect()


link = Link(args.usb, args.port)


# =========================
# COMMAND THREAD  (Synnax -> ESP32)
# =========================
# Set while the command stream from Synnax is open. Heartbeats only go out while it is,
# so the OLED shows the link as down when Synnax is, not just when the laptop is.
synnax_ok = threading.Event()


def format_value(value):
    v = float(value)
    return str(int(v)) if v.is_integer() else f"{v:g}"


def command_worker():
    """Forward every value written to a command channel as C,<channel>,<value>."""
    while True:
        try:
            with client.open_streamer(COMMAND_CHANNELS) as streamer:
                synnax_ok.set()
                print(f"[INFO] Listening on {len(COMMAND_CHANNELS)} command channels")
                for frame in streamer:
                    for ch in dict.fromkeys(frame.channels):
                        for value in frame[ch].to_numpy():
                            text = format_value(value)
                            try:
                                link.write(f"C,{ch},{text}\n".encode())
                                print(f"[CMD] {ch} = {text} -> ESP32")
                            except Exception as e:
                                print(f"[WARN] {ch} = {text} not sent, link down:", e)
        except Exception as e:
            synnax_ok.clear()
            print("[ERROR] Command stream died, restarting:", e)
            time.sleep(2)


# =========================
# HEARTBEAT THREAD
# =========================
def heartbeat_worker():
    """Keep the ESP32's link timeout fed while both the link and Synnax are up."""
    while True:
        if synnax_ok.is_set():
            try:
                link.write(b"~\n")
            except Exception:
                pass
        time.sleep(HEARTBEAT_INTERVAL)


threading.Thread(target=command_worker, daemon=True).start()
threading.Thread(target=heartbeat_worker, daemon=True).start()


# =========================
# READER LOOP  (ESP32 -> Synnax)
# =========================
WRITE_CHANNELS = [INDEX_CHANNEL] + PRESSURE_CHANNELS

with client.open_writer(start=sy.TimeStamp.now(), channels=WRITE_CHANNELS) as writer:
    expected_row = None   # next row number; a jump means rows were lost on the way
    last_ts = 0
    rows = lost = 0
    last_stats = time.time()
    latest = None

    try:
        while True:
            try:
                raw = link.readline()
                if not raw.endswith(b"\n"):
                    if not args.usb:
                        raise ConnectionResetError("ESP32 closed the connection")
                    if raw:
                        print("[WARN] Partial line dropped:", raw)
                    else:
                        print(f"[WARN] Nothing from the ESP32 for {READ_TIMEOUT:.0f} s")
                    continue

                line = raw.decode(errors="replace").strip()
                if not line:
                    continue

                # Status lines from the ESP32 ("[ESP32] Command 3: ...") share the stream.
                if not line.startswith("D,"):
                    print(line)
                    continue

                parts = line.split(",")
                try:
                    if len(parts) != 7:
                        raise ValueError
                    row = int(parts[1])
                    pressures = [float(p) for p in parts[2:]]
                except ValueError:
                    print("[WARN] Bad row:", line)
                    continue

                if expected_row is not None and row != expected_row:
                    lost += max(row - expected_row, 0)
                expected_row = row + 1

                # Index timestamps must strictly increase, even for rows in one burst.
                ts = sy.TimeStamp(max(sy.TimeStamp.now(), last_ts + 1))
                last_ts = ts
                frame = {INDEX_CHANNEL: ts}
                frame.update(zip(PRESSURE_CHANNELS, pressures))
                writer.write(frame)
                rows += 1
                latest = pressures

                now = time.time()
                if now - last_stats >= STATS_INTERVAL:
                    print(f"[DATA] {rows / (now - last_stats):.0f} rows/s, {lost} lost, "
                          f"latest p1-p5 = {latest}")
                    rows = lost = 0
                    last_stats = now

            except (serial.SerialException, OSError) as e:
                # OSError covers socket timeouts and resets.
                print("[ERROR] Link lost, reconnecting...", e)
                link.reconnect()
                expected_row = None

    except KeyboardInterrupt:
        print("\n[INFO] Shutting down.")
