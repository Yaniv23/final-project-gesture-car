import serial

# Open COM11 at 115200 baud
ser = serial.Serial('COM11', 115200, timeout=1)

print("Listening to ESP32 on COM11...\n")

while True:
    try:
        line = ser.readline().decode('utf-8').strip()
        if line:
            print(f"ESP32: {line}")
    except Exception as e:
        print("Error:", e)
        break
