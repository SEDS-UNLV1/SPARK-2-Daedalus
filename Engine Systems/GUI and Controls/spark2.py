import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from pymongo import MongoClient
from datetime import datetime

plt.style.use('fivethirtyeight')

ser = serial.Serial('COM3', 9600)  

client = MongoClient("mongodb://localhost:27017/")  
db = client["sensor_database"]
collection = db["sensor_data"]

x_vals = []
y_vals = []

def animate(i):
    global x_vals, y_vals 

    try:
        line = ser.readline().decode('utf-8').strip() 

        if line.startswith("Temp: "):  
            temperature = float(line.split(": ")[1])  

            x_vals.append(i) 
            y_vals.append(temperature)

            x_vals = x_vals[-50:] 
            y_vals = y_vals[-50:]

            sensor_data = {
                "temperature": temperature,
                "timestamp": datetime.now()
            }
            collection.insert_one(sensor_data) 
            print("Data inserted into MongoDB:", sensor_data)

            plt.cla()
            plt.plot(x_vals, y_vals, label='Temperature')
            plt.xlabel('Time')
            plt.ylabel('Temperature (°C)')
            plt.legend(loc='upper left')
            plt.tight_layout()

    except KeyboardInterrupt:
        print("Exiting...")
        plt.close()
        ser.close()
        exit()

ani = FuncAnimation(plt.gcf(), animate, interval=1000, save_count=50)  

plt.tight_layout()
plt.show()