# SPARK-2 Daedalus

CAD, code, and other analysis for **SPARK-2 Daedalus**.

This page will cover basic Github, Python, and terminal commands to let you start looking over the repo.

## Getting the files

- **Easy way (if you just want to look at files):** click the green **Code** button at the top of this page, **Download ZIP**, and unzip it.
- **With git:**
  ```bash
  git clone https://github.com/SEDS-UNLV1/SPARK-2-Daedalus.git
  ```

## What's in here

### Combustion Devices:

- **`Injector/`**: SolidWorks CAD for the injector, **Read the `README.txt` in a folder before you open its assembly.**
- **`Heat Transfer/`**: MATLAB scripts that estimate wall temperatures in the main combustion chamber. 

### Engine Systems: the electronics and software that run the engine

- **`GUI and Controls/`**: Arduino code for valve actuation and the torch igniter (in `Torch Igniter/`, Simulink models.
- **`DAQ/`** (data acquisition stuff):
  - `Serial-Reader/`: Python scripts that read pressure and temperature from the Arduino and send them to [Synnax](https://www.synnaxlabs.com/) for live graphs. `Updated/` adds valve control and an optional ESP32 Wi-Fi link.
  - `T7/`: Lua script for the LabJack T7 DAQ.

## Running a Python script

A **virtual environment** ("venv") is a private folder of Python packages for one project, so projects don't break each other. You just need to create it once, then turn it on ("activate" it) each time you open a new terminal.

**1. Install Python 3** from [python.org](https://www.python.org/downloads/). On Windows, check **"Add python.exe to PATH"** in the installer.

**2. Open a terminal.** On Mac, open the **Terminal** app. On Windows, search the Start menu for **Command Prompt**.

**3. Go to the script's folder.** Type `cd ` (with a space), drag the folder into the terminal window, and press Enter. It should look like this:

```bash
cd "/path/to/SPARK-2-Daedalus/Engine Systems/DAQ/Serial-Reader"
```

**4. Create the venv.** You only do this once per folder.

| Mac | Windows |
|---|---|
| `python3 -m venv venv` | `python -m venv venv` |

**5. Activate the venv.** Do this every time you open a new terminal.

| Mac                        | Windows                 |
| -------------------------- | ----------------------- |
| `source venv/bin/activate` | `venv\Scripts\activate` |

You'll see `(venv)` at the start of the line when it's on.

**6. Install the packages the script needs:**

**EXAMPLE:**

| Script | Command |
|---|---|
| `DAQ/Serial-Reader/` scripts | `pip install pyserial synnax` |
| `GUI and Controls/gui.py` | `pip install pyserial PyQt6 matplotlib pymongo` |
| `GUI and Controls/spark2.py` | `pip install pyserial matplotlib pymongo` |

(The package is called `pyserial`, not `serial`. Installing `serial` gets you the wrong one.)

**7. Run something:**

```bash
python serial_parser_test.py
```

Press **Ctrl+C** to stop it. Type `deactivate` to turn the venv off.
