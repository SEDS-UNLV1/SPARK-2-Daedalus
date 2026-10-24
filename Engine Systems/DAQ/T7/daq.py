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
T7_IP = "ANY"  # "ANY" will find T7 on the network, will put its IP here (from Kipling) 
AIN_NAMES = ["AIN0", "AIN1", "AIN2", "AIN3", "AIN4"]  # same pins as test.lua
READ_INTERVAL = 0.01  # seconds, 100 Hz

HOST = "localhost"
PORT_API = 9090

# CONNECT TO SYNNAX
# sy.Synnax logs in right away, make sure server is running before this
client = sy.Synnax(
  host=HOST,
  port=PORT_API,
  username="synnax",
  password="seldon",
  secure=False,
)
print("[INFO] Connected to Synnax")

# CONNECT TO T7 
def connect_t7():
  while True:
    try:
      handle = ljm.openS("T7", "ETHERNET", T7_IP)
      info = ljm.getHandleInfo(handle)
      print(f"[INFO] Connected to T7 #{info[2]} at {ljm.numberToIP(info[3])}")
      return handle
    except ljm.LJMError as e:
      print("[WARN] T7 connection failed, retrying...", e)
      time.sleep(2)


#set up in synnax Labjack Readtask (read through ethernet)