# voltages 0-5
# 5 senders straight binary

#Python bridge between T7 and Synnax server
"""
    Set up connection to the synnax server hosted at 9090

    retreive data stream from T7 via ethernet connection

    output data to the synnax server
"""

# good examples for using the T7 with Python, interacting with AIN ports
# https://github.com/labjack/labjack-ljm-python/blob/master/Examples/More/AIN/simple_log.py

import time
import synnax as sy
from labjack import ljm #    pip install labjack-ljm synnax

# CONFIG
# set static IPs on the T7 (Kipling over USB) and on this laptop's Ethernet first in network settings
# T7_IP = "LJM_DEMO_MODE" runs with no hardware, every read returns 0
T7_IP = "ANY"  # "ANY" will find T7 on the network, will put its IP here (from Kipling) 
#check https://support.labjack.com/docs/3-1-modbus-map-t-series-datasheet for AIN addresses ( Address = StartingAddress + 2*AIN#) 
AIN = ["AIN0", "AIN1", "AIN2", "AIN3", "AIN4"]  # same pins as test.lua
READ_INTERVAL = 0.01  # in seconds, results in 100 Hz
# reads are timed by this laptop, some samples may be skipped
# ljm.eStreamStart would fix that, but can only use if the test.lua kill switch stops reading AIN,
# a stream locks out all AIN reads while it runs ;p

# rename these when the i find out what the pins are actually measuring
TIME_CHANNEL = "t7_time"
CHANNELS = [f"t7_{pin.lower()}" for pin in AIN]

# CONFIG MUST BE written on every connect because a power cycle resets them
AIN_CONFIG = {}
for pin in AIN:
    AIN_CONFIG[f"{pin}_NEGATIVE_CH"] = ljm.constants.GND
    AIN_CONFIG[f"{pin}_RANGE"] = 10.0
    AIN_CONFIG[f"{pin}_RESOLUTION_INDEX"] = 6

HOST = "localhost"
PORT_API = 9090

# CONNECT TO SYNNAX
# sy.Synnax logs in right away, make sure server is running before this
# shortcut only reaches a standalone Core on 9090. Synnax Desktop 1.0.2 picks its own port and rotates its password,
# switch to plain sy.Synnax() once a Desktop and client release include synnax PR #4244 
client = sy.Synnax(
    host=HOST,
    port=PORT_API,
    username="synnax",
    password="seldon",
    secure=False,
)
print("[INFO] Connected to Synnax")

# time and data channel creation if they aren't already on synnax server.
time_channel = client.channels.create(
    name=TIME_CHANNEL,
    is_index=True,
    data_type=sy.DataType.TIMESTAMP,
    retrieve_if_name_exists=True,
)
for name in CHANNELS:
    client.channels.create(
        name=name,
        index=time_channel.key,
        data_type=sy.DataType.FLOAT32,
        retrieve_if_name_exists=True,
    )


# CONNECT TO T7 
def connect_t7():
    while True:
        try:
            handle = ljm.openS("T7", "ETHERNET", T7_IP)
            ljm.eWriteNames(handle, len(AIN_CONFIG), list(AIN_CONFIG), list(AIN_CONFIG.values()))
            #TUPLE info contains (deviceType, connectionType, serialNumber, ipAddress, port, maxBytesPerMB)
            info = ljm.getHandleInfo(handle) 
            # print(f"[INFO] Connected to T7 #{info[2]} at {ljm.numberToIP(info[3])}")
            print(f"[INFO] Connected to {info[0]}#{info[2]} via {info[1]} at {ljm.numberToIP(info[3])}, PORT:{info[4]}")
            return handle
        except ljm.LJMError as e:
            print("[WARN] T7 connection failed, retrying...", e)
            ljm.closeAll()  # a failed config write would otherwise leave the handle open
            time.sleep(2)

#main, the core execution logic
def main():
    handle = connect_t7()
    interval = 1  # any id, LJM only uses it to name this timer
    ljm.startInterval(interval, round(READ_INTERVAL * 1_000_000))

    while True:
        try:
            with client.open_writer(start=sy.TimeStamp.now(), channels=[TIME_CHANNEL, *CHANNELS]) as writer:
                while True:
                    skipped = ljm.waitForNextInterval(interval)
                    if skipped:
                        print(f"[WARN] Fell behind, skipped {skipped} reads")
                    volts = ljm.eReadNames(handle, len(AIN), AIN)
                    writer.write({TIME_CHANNEL: sy.TimeStamp.now(), **dict(zip(CHANNELS, volts))})
        except ljm.LJMError as e:
            print("[ERROR] T7 read failed, reconnecting...", e)
            ljm.closeAll()
            handle = connect_t7()

if __name__ == "__main__":
    main()