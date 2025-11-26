import serial
from constant import COM_PORT, BAUD_RATE

# Open serial connection to ESP32
ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)

print(f"Listening to ESP32 on {COM_PORT}...\n")

while True:
    try:
        line = ser.readline().decode('utf-8').strip()
        if line:
            print(f"ESP32: {line}")
    except Exception as e:
        print("Error:", e)
        break
