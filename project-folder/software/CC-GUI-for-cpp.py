import tkinter as tk
from tkinter import ttk
import serial
import serial.tools.list_ports
import threading
import queue
import time

BAUD = 115200
ser = None
gui_queue = queue.Queue()
ack_event = threading.Event()


def read_value():
    try:
        temp = float(temp_entry.get())
        fan = int(fan_entry.get())
        fan_speed = int(fan_speed_entry.get())
        return temp, fan, fan_speed
    except ValueError:
        print("Insert a number. Only temp supports float.")
        return None

def connect():
    port = selected_port.get()
    print("Selected port:", port)
    if not port:
        print("Select a USB port.")
        return

    connect_button.config(state="disabled", text="Connecting...")
    threading.Thread(target=_connect_worker, args=(port,), daemon=True).start()


def _connect_worker(port):
    global ser
    try:
        print("Opening serial port...")
        new_ser = serial.Serial()
        new_ser.port = port
        new_ser.baudrate = BAUD
        new_ser.timeout = 0.1
        print("Calling open()...")
        new_ser.open()
        ser = new_ser  # globalno spremenljivko posodobimo šele ko je open() uspel
        gui_queue.put(("connect_success", port))
    except serial.SerialException as e:
        gui_queue.put(("connect_error", str(e)))


def send_settings():
    if ser is None or not ser.is_open:
        print("Not connected.")
        return

    values = read_value()
    if values is None:
        return

    temp, fan, fan_speed = values
    if fan not in (1, 2):
        print("Fan must be 1 or 2.")
        return

    fan_speed = max(0, min(100, fan_speed))
    cmd = f"SET,{temp},{fan},{fan_speed}\n"

    ack_event.clear()
    send_button.config(state="disabled", text="Sending...")
    threading.Thread(target=_send_worker, args=(cmd,), daemon=True).start()


def _send_worker(cmd):
    max_attempts = 30  # 30 x 100ms = 3 sekunde skupnega timeouta

    for _ in range(max_attempts):
        if not (ser is not None and ser.is_open):
            gui_queue.put(("send_error", "Serial port closed."))
            return

        try:
            ser.write(cmd.encode())
        except serial.SerialException as e:
            gui_queue.put(("send_error", str(e)))
            return

        # čaka do 100ms, a se vrne takoj, ko receiver() pokliče ack_event.set()
        if ack_event.wait(timeout=0.1):
            gui_queue.put(("send_success", None))
            return

    gui_queue.put(("send_timeout", None))


def receiver():
    if ser is not None and ser.is_open:
        try:
            while ser.in_waiting:
                line = ser.readline().decode(errors="replace").strip()

                if line.startswith("ACK"):
                    ack_event.set()
                    print("ACK received.")
                    continue

                if not line.startswith("DATA"):
                    if line:
                        print("MCU:", line)
                    continue

                parts = line.split(",")
                if len(parts) != 6:
                    continue

                temp, voltage, current, power, dac_voltage = parts[1:6]
                gui_queue.put(("data", temp, voltage, current, power, dac_voltage))

        except (UnicodeDecodeError, serial.SerialException) as e:
            print("Receive error:", e)

    root.after(50, receiver)


def process_gui_queue():
    try:
        while True:
            message = gui_queue.get_nowait()
            handle_message(message)
    except queue.Empty:
        pass

    root.after(50, process_gui_queue)


def handle_message(message):
    kind = message[0]

    if kind == "connect_success":
        port = message[1]
        print(f"Connected to {port}")
        connect_button.config(state="normal", text="Connect")

    elif kind == "connect_error":
        error_text = message[1]
        print("Connection error:", error_text)
        connect_button.config(state="normal", text="Connect")

    elif kind == "send_success":
        print("ACK received. Stopped sending.")
        send_button.config(state="normal", text="Send")
        ack_event.clear()

    elif kind == "send_timeout":
        print("No ACK received, timeout.")
        send_button.config(state="normal", text="Send")
        ack_event.clear()

    elif kind == "send_error":
        error_text = message[1]
        print("Send error:", error_text)
        send_button.config(state="normal", text="Send")
        ack_event.clear()

    elif kind == "data":
        _, temp, voltage, current, power, dac_voltage = message
        real_temp_label.config(text=f"Temperature: {temp} °C")
        voltage_label.config(text=f"Voltage: {voltage} V")
        current_label.config(text=f"Current: {current} mA")
        power_label.config(text=f"Power: {power} mW")
        DAC_voltage_label.config(text=f"DAC voltage: {dac_voltage} V")


def get_ports():
    return [port.device for port in serial.tools.list_ports.comports()]


# GUI
root = tk.Tk()
root.title("TEC driver GUI")
root.geometry("400x500")

tk.Label(root, text="Select a correct USB port.").pack()
selected_port = tk.StringVar()
port_menu = ttk.Combobox(root, textvariable=selected_port, values=get_ports(), state="readonly")
port_menu.pack(padx=20, pady=20)

connect_button = tk.Button(root, text="Connect", command=connect)
connect_button.pack(pady=5)

tk.Label(root, text="Insert desired temperature [°C].").pack()
temp_entry = tk.Entry(root)
temp_entry.insert(0, "16")
temp_entry.pack()

tk.Label(root, text="Select a fan [1 or 2].").pack()
fan_entry = tk.Entry(root)
fan_entry.insert(0, "2")
fan_entry.pack()

tk.Label(root, text="Select the fan speed [%].").pack()
fan_speed_entry = tk.Entry(root)
fan_speed_entry.insert(0, "50")
fan_speed_entry.pack()

send_button = tk.Button(root, text="Send", command=send_settings)
send_button.pack(pady=10)

real_temp_label = tk.Label(root, text="Temperature: ---")
real_temp_label.pack()
voltage_label = tk.Label(root, text="Voltage: ---")
voltage_label.pack()
current_label = tk.Label(root, text="Current: ---")
current_label.pack()
power_label = tk.Label(root, text="Power: ---")
power_label.pack()
DAC_voltage_label = tk.Label(root, text="DAC voltage: ---")
DAC_voltage_label.pack()

receiver()
process_gui_queue()
root.mainloop()
