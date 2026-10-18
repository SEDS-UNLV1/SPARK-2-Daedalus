# ESP32 + OLED Synnax bench test

  

This folder (ESP32Testing) will showcase a ESP32 link and Synnax without any interaction with the mega and valves. The ESP32 will replay recorded coldflow pressures into Synnax, and shows every command you click in the Synnax Console on a small screen. There is an alterantive that will blink the ESP32's onboard led if you don't have an i2c oled module.

  

```

ESP32 --Wi-Fi or USB--> synnax_oled_bridge.py --> Synnax (pressure_1..5)

OLED (or LED alternative) <-- ESP32 <------ synnax_oled_bridge.py <-- Synnax (valve/sequence clicks)

```

  

## 0. Install Synnax (once)

Synnax is on 0.59 now, and updating seemgly breaks synnax on my end, for now I would recommend downloadng the 0.58 files below rather than "latest". This test has only been run with these versions together:

| Piece | Version |
|-------|---------|
| Synnax server (Core) | 0.58.2 |
| Synnax Console | 0.58.3 |
| Python `synnax` package | 0.58.1 (installed in step 3) |

There is a chance this works on a fresh install of the latest build. Otherwise downgrading is required.

**Server, macOS:**

```bash
curl -LO https://github.com/synnaxlabs/synnax/releases/download/core/v0.58.2/synnax-v0.58.2-macos
sudo mv synnax-v0.58.2-macos /usr/local/bin/synnax
sudo chmod +x /usr/local/bin/synnax
synnax --version
```

It should print `Synnax 0.58.2`. If it prints another version, an older copy is earlier on your PATH: find it with `which -a synnax` and remove it.

**Server, Windows:** download and run [synnax-setup-v0.58.2.exe](https://github.com/synnaxlabs/synnax/releases/download/core/v0.58.2/synnax-setup-v0.58.2.exe). Then check with `synnax --version` in PowerShell.

**Console:**
- **macOS (Apple Silicon):** download [Synnax_0.58.3_aarch64.dmg](https://github.com/synnaxlabs/synnax/releases/download/console/v0.58.3/Synnax_0.58.3_aarch64.dmg), open it, and drag Synnax into Applications.
- **Windows:** download and run [Synnax_0.58.3_x64-setup.exe](https://github.com/synnaxlabs/synnax/releases/download/console/v0.58.3/Synnax_0.58.3_x64-setup.exe).

If the Console offers to update itself, just skip it, so it stays on 0.58 like the server.

## 1. Start the Synnax server

  

```bash OR zsh

cd "Engine Systems/DAQ/Serial-Reader"

synnax start --insecure

```

  

Leave it running. It listens on `localhost:9090` (user `synnax`, password `seldon`).

  

Then open the **Synnax Console** app, connect to `localhost:9090` with secure off, and import `SynnaxTestSchematic.zip` (schematic + pressure plot).

  

## 2. Flash the ESP32 (Arduino IDE)

  

**File to flash:** `esp32-synnax-oled/esp32-synnax-oled.ino`

  

**Test screen:** a 0.96" 128x64 I2C OLED (SSD1306) stands in for the Mega's output. It shows link status, a rows-sent counter, and each command received (e.g. `Command 3: fuel_line_cmd received`). The sketch still runs without the screen.

  

| OLED | ESP32  |
|------|--------|
| VCC  | 3V3    |
| GND  | GND    |
| SDA  | GPIO21 |
| SCL  | GPIO22 |

  

1. **Boards Manager:** install **esp32 by Espressif Systems**.

2. **Library Manager:** install **Adafruit SSD1306**, and accept **Adafruit GFX** when it asks.

3. Make sure `esp32-synnax-oled/secrets.h` exists. It sets the ESP32's Wi-Fi network name and password.

```cpp

#define AP_SSID "your-network-name"

#define AP_PASSWORD "8-to-63-characters"

```

4. **Tools → Board:** ESP32 Dev Module. **Tools → Port:** `/dev/cu.usbserial-0001` (Mac) or `COMx` (Windows).

5. Click **Upload**. If it hangs at `Connecting...`, hold the **BOOT** button on the board until the upload starts.

6. Optional: open the Serial Monitor at **115200** to see `[ESP32]` status lines. Close it before running the bridge with `--usb`.

  

### Alternative: on board LED only (if no OLED)

  

Flash `esp32-synnax-led/esp32-synnax-led.ino` instead. It will need no OLED, libraries, or wiring. It sends the same test data as the OLED version. In place of the screen, the ESP32's on-board LED (GPIO2) flashes twice at boot, then once for every command sent from Synnax.

Change wifi details in secret.h if desired.

Everything else (Synnax, step 3) is the same.

  

## 3. Run the bridge on the laptop

Everything here runs from `Serial-Reader`:

```bash or zsh
cd "Engine Systems/DAQ/Serial-Reader"
```

**First time only:** create the venv.

```bash or zsh
python3 -m venv venv
```

**Every time:** activate the venv. Your prompt will start with `(venv)`.

```bash or zsh
source venv/bin/activate
```

**First time only:** with the venv active, install the modules.

```bash or zsh
pip install synnax==0.58.1 pyserial
```

Keep `==0.58.1`: a plain `pip install synnax` gets 0.59, which hasn't been tested with the 0.58 server (see step 0).

**Then run the bridge:**

- **Wi-Fi:** first join the ESP32's network (the name in `secrets.h`; the OLED shows it too). Then run:
  ```bash
  python ESP32Testing/synnax_oled_bridge.py
  ```
- **USB cable:**
  ```bash
  python ESP32Testing/synnax_oled_bridge.py --usb
  ```

Stop the bridge with Ctrl+C. Leave the venv with `deactivate`.

On Windows, activate with `venv\Scripts\activate`, and add `--port COM3` for USB.

If `python` isn't found after activating, the repo folder was probably moved or renamed: a venv only works where it was created. Delete the `venv` folder and redo the first-time steps.

## What you should see

  

- **OLED:** `WiFi ok` (or `USB ok`) and `rows sent` counting up.

- **Bridge terminal:** a line about every 5 s like `[DATA] 100 rows/s, 0 lost, latest p1-p5 = [...]`.

- **Console:** `pressure_1`..`pressure_5` moving. The recording loops about every 49 s.

- **Clicking a valve:** the terminal prints `[CMD] fuel_line_cmd = 1 -> ESP32`, and the OLED shows `Command 1: fuel_line_cmd received`.

  

## If something's off

  

- **OLED blank:** check the wiring. Flash `oled_test.ino` on its own to test just the screen.

- **Test the OLED without the laptop:** in the Serial Monitor (line ending: Newline), type `C,fuel_line_cmd,1`.

- **Replay a different CSV:** run `python3 csv_to_header.py "<file>.csv"`. It updates both sketches. Then re-flash.