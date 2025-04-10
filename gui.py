import sys
import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from datetime import datetime
from pymongo import MongoClient
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QLabel, 
    QSlider, QLineEdit, QFrame
)
from PyQt6.QtCore import Qt
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas

client = MongoClient("mongodb://localhost:27017/")
db = client["sensor_database"]
collection = db["sensor_data"]


# defining a fake serial port for testing
class FakeSerial:
    def __init__(self, *args, **kwargs):
        self.buffer = b""

    def write(self, data):
        print(f"Fake write: {data}")
        self.buffer = b"ACK\n"

    def read(self, size=1):
        return self.buffer[:size]

    def readline(self):
        return b"ACK\n"

    def close(self):
        print("Fake port closed")

# uncomment below for Arduino
# ser = serial.Serial('COM3', 9600)
# uncomment below for software testing (no arduino)
ser = FakeSerial()

plt.style.use('fivethirtyeight')
fig, ax = plt.subplots()
x_vals, y_vals = [], []

def animate(i):
    global x_vals, y_vals

    try:
        line = ser.readline().decode('utf-8').strip()

        if line.startswith("Temp: "):
            temperature = float(line.split(": ")[1])

            x_vals.append(datetime.now().strftime("%H:%M:%S"))
            y_vals.append(temperature)

            x_vals = x_vals[-50:]
            y_vals = y_vals[-50:]

            sensor_data = {"temperature": temperature, "timestamp": datetime.now()}
            collection.insert_one(sensor_data)
            ax.clear()
            ax.plot(x_vals, y_vals, label="Temperature (°C)")
            ax.legend(loc="upper left")
            ax.set_title("Sensor Data Over Time")
            ax.set_xlabel("Time")
            ax.set_ylabel("Temperature (°C)")

    except Exception as e:
        print("Error:", e)

ani = FuncAnimation(fig, animate, interval=1000)


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Rocket Engine Control Panel")
        self.setGeometry(100, 100, 1200, 800)

        # Main Layout
        main_layout = QVBoxLayout()

        # Top Panels Layout
        top_layout = QHBoxLayout()

        # Torch Igniter Panel
        igniter_panel = self.create_igniter_panel()
        top_layout.addWidget(igniter_panel)

        # Rocket Engine Diagram Panel
        engine_panel = self.create_diagram_panel()
        top_layout.addWidget(engine_panel)

        # Control Panel
        control_panel = self.create_control_panel()
        top_layout.addWidget(control_panel)

        # Motor Panel
        motor_panel = self.create_motor_panel()
        top_layout.addWidget(motor_panel)

        # Graph Panel
        graph_panel = self.create_graph_panel()
        main_layout.addLayout(top_layout)
        main_layout.addWidget(graph_panel)

        # Set Central Widget
        central_widget = QWidget()
        central_widget.setLayout(main_layout)
        self.setCentralWidget(central_widget)

    def create_igniter_panel(self):
        frame = QFrame()
        frame.setFrameShape(QFrame.Shape.Box)
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Torch Igniter"))
        duration_label = QLabel("Duration:")
        duration_input = QLineEdit()
        ignite_button = QPushButton("Ignite")

        layout.addWidget(duration_label)
        layout.addWidget(duration_input)
        layout.addWidget(ignite_button)

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
        for i in range(1, 6):
            valve_label = QLabel(f"Valve {i}")
            valve_slider = QSlider(Qt.Orientation.Horizontal)
            layout.addWidget(valve_label)
            layout.addWidget(valve_slider)

        frame.setLayout(layout)
        return frame

    def create_motor_panel(self):
        frame = QFrame()
        frame.setFrameShape(QFrame.Shape.Box)
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Motor Panel"))
        battery_label = QLabel("Battery %: 100")
        motor_slider = QSlider(Qt.Orientation.Horizontal)

        layout.addWidget(battery_label)
        layout.addWidget(motor_slider)

        frame.setLayout(layout)
        return frame

    def create_graph_panel(self):
        frame = QFrame()
        frame.setFrameShape(QFrame.Shape.Box)
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Sensor Data Graph"))
        canvas = FigureCanvas(fig)
        layout.addWidget(canvas)

        frame.setLayout(layout)
        return frame


# Main Application
app = QApplication(sys.argv)
window = MainWindow()
window.show()
sys.exit(app.exec())