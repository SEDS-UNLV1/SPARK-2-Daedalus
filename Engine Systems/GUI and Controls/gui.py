import sys
import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
from datetime import datetime
from pymongo import MongoClient
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QLabel, QSlider, QLineEdit, QFrame, QComboBox, QSizePolicy
)
from PyQt6.QtCore import Qt, QTimer
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas

# --- Setup Database ---
client = MongoClient("mongodb://localhost:27017/")
db = client["sensor_database"]
collection = db["sensor_data"]

# --- Serial Setup Placeholder ---
ser = None

# --- Setup Matplotlib Figure ---
plt.style.use('fivethirtyeight')
fig, ax = plt.subplots()
x_vals, y_vals = [], []

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Rocket Engine Control Panel")
        self.setGeometry(100, 100, 1200, 800)

        # Main Layout
        main_layout = QVBoxLayout()

        # Top Panels Layout
        top_layout = QHBoxLayout()
        for panel_creator in [
            self.create_port_select_panel,
            self.create_igniter_panel,
            self.create_diagram_panel,
            self.create_control_panel,
            self.create_motor_panel
        ]:
            panel = panel_creator()
            panel.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
            top_layout.addWidget(panel)

        main_layout.addLayout(top_layout)
        main_layout.addWidget(self.create_graph_panel())

        # Set Central Widget
        central_widget = QWidget()
        central_widget.setLayout(main_layout)
        self.setCentralWidget(central_widget)

        # Timer for reading serial data every 100ms
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.read_serial_data)
        self.timer.start(100)

    def create_port_select_panel(self):
        frame = QFrame()
        frame.setFrameShape(QFrame.Shape.Box)
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Serial Port Selector"))
        self.port_combo = QComboBox()
        self.baud_combo = QComboBox()
        self.baud_combo.addItems(["9600", "19200", "38400", "57600", "115200"])
        refresh_button = QPushButton("Refresh Ports")
        connect_button = QPushButton("Connect")

        refresh_button.clicked.connect(self.refresh_ports)
        connect_button.clicked.connect(self.connect_serial)

        layout.addWidget(QLabel("Port:"))
        layout.addWidget(self.port_combo)
        layout.addWidget(QLabel("Baud Rate:"))
        layout.addWidget(self.baud_combo)
        layout.addWidget(refresh_button)
        layout.addWidget(connect_button)

        frame.setLayout(layout)
        self.refresh_ports()
        return frame

    def refresh_ports(self):
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_combo.addItem(f"{port.device} - {port.description}")

    def connect_serial(self):
        global ser
        selected = self.port_combo.currentText().split(" - ")[0]
        baud = int(self.baud_combo.currentText())
        try:
            # Close existing connection if any
            if ser and ser.is_open:
                ser.close()
                
            ser = serial.Serial(selected, baud, timeout=1)
            print(f"Connected to {selected} at {baud} baud")
            print("Waiting for Arduino to initialize...")
            
            # Clear any data in buffer
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            
            # Wait for Arduino to send "ARDUINO READY"
            start_time = datetime.now()
            while (datetime.now() - start_time).total_seconds() < 5:
                if ser.in_waiting > 0:
                    line = ser.readline().decode('utf-8').strip()
                    print(f"Arduino says: {line}")
                    if line == "ARDUINO READY":
                        print("Arduino is ready!")
                        break
            else:
                print("Arduino did not respond with READY in 5 seconds")
                
        except Exception as e:
            print("Failed to connect:", e)

    def create_igniter_panel(self):
        frame = QFrame()
        frame.setFrameShape(QFrame.Shape.Box)
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Torch Igniter"))
        layout.addWidget(QLabel("Duration:"))
        layout.addWidget(QLineEdit())
        layout.addWidget(QPushButton("Ignite"))

        frame.setLayout(layout)
        return frame

    def create_diagram_panel(self):
        frame = QFrame()
        frame.setFrameShape(QFrame.Shape.Box)
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Rocket Engine Diagram"))
        layout.addWidget(QLabel("Don't have yet (need feed to pull up)"))

        frame.setLayout(layout)
        return frame

    def create_control_panel(self):
        frame = QFrame()
        frame.setFrameShape(QFrame.Shape.Box)
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Control Panel"))
        self.valve_buttons = []
        for i in range(1, 5):
            hbox = QHBoxLayout()
            label = QLabel(f"Valve {i}:")
            button = QPushButton("OFF")
            button.setStyleSheet("background-color: red; color: white;")
            button.setCheckable(False)  # Don't use checkable since we'll update based on Arduino response
            button.clicked.connect(lambda checked, v=i: self.send_valve_command(v))
            self.valve_buttons.append(button)
            hbox.addWidget(label)
            hbox.addWidget(button)
            layout.addLayout(hbox)

        frame.setLayout(layout)
        return frame

    def send_valve_command(self, valve_num):
        global ser
        if not ser or not ser.is_open:
            print("Serial not connected")
            return
            
        button = self.valve_buttons[valve_num - 1]
        
        # Toggle the command to send
        if button.text() == "OFF":
            command = f"VALVE{valve_num}:HI\n"
        else:
            command = f"VALVE{valve_num}:LO\n"
        
        try:
            print(f"Sending command: {command.strip()}")
            ser.write(command.encode())
            ser.flush()
        except Exception as e:
            print("Error sending command:", e)

    def create_motor_panel(self):
        frame = QFrame()
        frame.setFrameShape(QFrame.Shape.Box)
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Motor Panel"))
        layout.addWidget(QLabel("Battery %: 100"))
        layout.addWidget(QSlider(Qt.Orientation.Horizontal))

        frame.setLayout(layout)
        return frame

    def create_graph_panel(self):
        frame = QFrame()
        frame.setFrameShape(QFrame.Shape.Box)
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Sensor Data Graph"))
        self.canvas = FigureCanvas(fig)
        layout.addWidget(self.canvas)

        frame.setLayout(layout)
        return frame

    def read_serial_data(self):
        global x_vals, y_vals, ser
        try:
            if not ser or not ser.is_open:
                return
                
            while ser.in_waiting > 0:
                line = ser.readline().decode('utf-8').strip()
                print(f"Received: '{line}'")
                
                # Handle temperature data
                if line.startswith("Temp:"):
                    try:
                        temp_str = line.split("Temp:")[1]
                        temperature = float(temp_str)
                        print(f"Parsed temperature: {temperature}")
                        
                        now = datetime.now()
                        x_vals.append(now.strftime("%H:%M:%S"))
                        y_vals.append(temperature)

                        x_vals = x_vals[-50:]
                        y_vals = y_vals[-50:]

                        collection.insert_one({"temperature": temperature, "timestamp": now})

                        ax.clear()
                        ax.plot(x_vals, y_vals, label="Temperature (°C)")
                        ax.legend(loc="upper left")
                        ax.set_title("Sensor Data Over Time")
                        ax.set_xlabel("Time")
                        ax.set_ylabel("Temperature (°C)")
                        self.canvas.draw()
                    except Exception as e:
                        print(f"Could not parse temperature: {e}")
                
                # Handle valve status responses
                elif line.startswith("VALVE") and ("OPENED" in line or "CLOSED" in line):
                    try:
                        # Parse "VALVE1:OPENED" or "VALVE1:CLOSED"
                        valve_num = int(line[5])  # Get the valve number
                        status = "ON" if "OPENED" in line else "OFF"
                        
                        # Update the corresponding button
                        if 1 <= valve_num <= 4:
                            button = self.valve_buttons[valve_num - 1]
                            button.setText(status)
                            if status == "ON":
                                button.setStyleSheet("background-color: green; color: white;")
                            else:
                                button.setStyleSheet("background-color: red; color: white;")
                            print(f"Updated Valve {valve_num} to {status}")
                    except Exception as e:
                        print(f"Could not parse valve status: {e}")
                        
        except Exception as e:
            print("Error reading serial:", e)

# --- Main Application Launch ---
if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())