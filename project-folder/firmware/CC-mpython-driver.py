from machine import Pin, I2C, PWM
from ina219 import INA219
import _thread
from time import sleep
import machine
import onewire
import ds18x20
import sys
import uselect

DAC_ADDR = 0x60
IDLE_VOLTAGE = 3.29

running = True

i2c = I2C(
    0,
    sda=Pin(4),
    scl=Pin(5),
    freq=400000
)

lock = _thread.allocate_lock()

ina = INA219(0.02, i2c)
ina.configure()

ds_pin = Pin(8)
ds_sensor = ds18x20.DS18X20(
    onewire.OneWire(ds_pin)
)
roms = ds_sensor.scan()

pwm = PWM(Pin(10))
pwm.freq(25000)

fan_enable = Pin(6, Pin.IN, Pin.PULL_DOWN)

last_voltage = 0.0
last_current = 0.0
last_power = 0.0
last_temp = 0.0

set_voltage_value = 0.0
set_fan_value = 0

def set_voltage(v):

    global set_voltage_value

    if v < 0:
        v = 0

    if v > 3.3:
        v = 3.3

    value = round((v * 4095) / 3.3)

    d4 = (value >> 8) & 0x0F
    d8 = value & 0xFF

    data = bytes([d4, d8])

    with lock:
        i2c.writeto(DAC_ADDR, data)

    set_voltage_value = v

def set_fan(percent):

    global set_fan_value

    if percent < 0:
        percent = 0

    if percent > 100:
        percent = 100

    duty = 100 - percent

    pwm.duty_u16(
        int(duty * 65535 / 100)
    )

    fan_enable.init(Pin.IN)

    set_fan_value = percent

def measure_task():

    global last_voltage
    global last_current
    global last_power
    global last_temp

    while running:

        try:

            with lock:
                last_voltage = ina.voltage()
                last_current = ina.current()
                last_power = ina.power()

            if len(roms) > 0:

                ds_sensor.convert_temp()
                sleep(0.8)

                t_sum = 0

                for rom in roms:
                    t_sum += ds_sensor.read_temp(rom)

                last_temp = t_sum / len(roms)

        except Exception as e:
            print("ERR,MEASURE,", e)

        sleep(0.2)

poll = uselect.poll()
poll.register(sys.stdin, uselect.POLLIN)

def process_usb():

    if not poll.poll(0):
        return

    try:

        line = sys.stdin.readline()

        if not line:
            return

        line = line.strip()

        if line.startswith("SET"):

            parts = line.split(",")

            if len(parts) != 3:
                return

            voltage = float(parts[1])
            fan = int(parts[2])

            set_voltage(voltage)
            set_fan(fan)

            print(
                "ACK,{:.3f},{}".format(
                    set_voltage_value,
                    set_fan_value
                )
            )

    except Exception as e:

        print(
            "ERR,USB,{}".format(e)
        )

_thread.start_new_thread(
    measure_task,
    ()
)

set_voltage(IDLE_VOLTAGE)
Pin(6, Pin.IN, Pin.PULL_DOWN)
set_fan(0)

print("READY")

try:

    while True:

        process_usb()

        print(
            "DATA,{:.3f},{:.2f},{:.2f},{:.2f}".format(
                last_voltage,
                last_current,
                last_power,
                last_temp
            )
        )

        sleep(1)

except KeyboardInterrupt:

    pass

finally:

    running = False

    pwm.deinit()

    set_voltage(IDLE_VOLTAGE)

    Pin(6, Pin.IN, Pin.PULL_DOWN)

    print("STOP")