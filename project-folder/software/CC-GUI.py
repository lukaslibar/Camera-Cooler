import tkinter as tk
import serial
import threading

PORT = "/dev/ttyACM0"
BAUD = 115200

ser = serial.Serial(PORT, BAUD, timeout=1)

def send_settings():

    voltage = voltage_entry.get()
    fan = fan_entry.get()

    cmd = f"SET,{voltage},{fan}\n"

    ser.write(cmd.encode())


def receiver():

    while True:

        try:

            line = ser.readline().decode().strip()

            if not line.startswith("DATA"):
                continue

            parts = line.split(",")

            measured_voltage = parts[1]
            current = parts[2]
            power = parts[3]
            temp = parts[4]

            voltage_label.config(
                text=f"Measured voltage: {measured_voltage} V"
            )

            current_label.config(
                text=f"Current: {current} mA"
            )

            power_label.config(
                text=f"Powerč: {power} mW"
            )

            temp_label.config(
                text=f"Temperature: {temp} °C"
            )

        except:
            pass


threading.Thread(
    target=receiver,
    daemon=True
).start()

# =====================
# GUI
# =====================

root = tk.Tk()
root.title("Camera Cooler GUI")

tk.Label(root, text="DAC voltage [V]").pack()

voltage_entry = tk.Entry(root)
voltage_entry.insert(0, "2.50")
voltage_entry.pack()

tk.Label(root, text="Fen [%]").pack()

fan_entry = tk.Entry(root)
fan_entry.insert(0, "50")
fan_entry.pack()

tk.Button(
    root,
    text="Send",
    command=send_settings
).pack(pady=10)

voltage_label = tk.Label(
    root,
    text="Measured voltage: ---"
)
voltage_label.pack()

current_label = tk.Label(
    root,
    text="Current: ---"
)
current_label.pack()

power_label = tk.Label(
    root,
    text="Power: ---"
)
power_label.pack()

temp_label = tk.Label(
    root,
    text="Temperature: ---"
)
temp_label.pack()

root.mainloop()
