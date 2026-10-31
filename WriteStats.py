import psutil
from serial.tools import list_ports
import serial
import time as time

PORT = "/dev/cu.ESP32-Personality"
BAUD = 115200

def readStats():
    cpu = psutil.cpu_percent(interval=1)

    battery = psutil.sensors_battery()
    if battery is None:              # desktop with no battery
        battery_percent = 100
    else:
        battery_percent = battery.percent

    uptime_minutes = int((time.time() - psutil.boot_time()) / 60)
    uptime_minutes = min(uptime_minutes, 99999)   # keep the message short

    return f"{int(cpu)},{int(battery_percent)},{uptime_minutes}\n"

def connect():
     while True:
          try:
               return serial.Serial(PORT, BAUD, timeout=1)
          except (serial.SerialException, OSError):
               print(f"Failed to connect to port. Retrying in 3 seconds...")
               time.sleep(3) # wait for 3 seconds before trying to reconnect

def main():
    port = connect()
    print("Connect to port")

    while True:
        try:
            port.write(readStats().encode()) # write the data to the serial port in bytes
        except (serial.SerialException, OSError):
            print(f"Lost connection to port. Reconnecting...")
            port.close()
            port = connect() # reconnect to the serial port

main()