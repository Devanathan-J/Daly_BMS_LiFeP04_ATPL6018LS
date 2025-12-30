import serial
import time

# Set your correct COM port
ser = serial.Serial("COM6", 9600, timeout=1)

def calculate_checksum(data):
    return sum(data) & 0xFF

def parse_response(response):
    if len(response) != 13 or response[0] != 0xA5:
        return None

    voltage_raw = response[3] << 8 | response[2]
    current_raw = response[6] << 8 | response[5]
    soc_raw     = response[8] << 8 | response[7]
    temp_raw    = response[9]

    # Refined scaling
    voltage = round(voltage_raw * 0.028528, 2)
    soc     = round(soc_raw * 0.001926, 1)
    current = round(current_raw * 0.1, 1)      # 0x71 → 11.3 A, as before
    temperature = temp_raw - 15               # 48 → ~33°C

    return {
        "voltage": voltage,
        "current": current,
        "soc": soc,
        "temperature": temperature
    }

# Build request
request = [0xA5, 0x40, 0x90, 0x08] + [0x00] * 8
request.append(calculate_checksum(request))

while True:
    ser.write(bytearray(request))
    time.sleep(0.3)
    response = ser.read(13)

    if len(response) == 13:
        decoded = parse_response(response)
        if decoded:
            print(decoded)
        else:
            print("Invalid response")
    else:
        print("No or incomplete data")
    time.sleep(10)
