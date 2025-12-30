import serial
import time

# Set your serial port here (e.g., 'COM6' on Windows or '/dev/ttyUSB0' on Linux)
ser = serial.Serial("COM6", 9600, timeout=2)

def calculate_checksum(data):
    return sum(data) & 0xFF

def send_request(register):
    ser.reset_input_buffer()  # Clear any old serial data
    request = [0xA5, 0x40, register, 0x08] + [0x00] * 8
    request.append(calculate_checksum(request))
    ser.write(bytearray(request))
    time.sleep(0.3)

def read_response(num_bytes=13):
    return ser.read(num_bytes)

# ========== Decode 0x90 (Voltage, Current, SOC) ==========
def decode_0x90(response):
    if len(response) != 13 or response[0] != 0xA5 or response[2] != 0x90:
        print("❌ Invalid 0x90 response.")
        return

    if calculate_checksum(response[:-1]) != response[-1]:
        print("⚠️ Checksum mismatch in 0x90.")
        return

    voltage = ((response[4] << 8) | response[5]) * 0.1
    current = (((response[8] << 8) | response[9]) - 30000) * 0.1
    soc = ((response[10] << 8) | response[11]) * 0.1

    print("🔋 Battery Info:")
    print(f"  Voltage : {voltage:.1f} V")
    print(f"  Current : {current:.1f} A")
    print(f"  SOC     : {soc:.1f} %")

# ========== Decode 0x95 (Cell Voltages) ==========
def decode_0x95():
    print("\n🔍 Cell Voltages:")
    cell_voltages = []

    for frame in range(7):  # Up to 19 cells
        send_request(0x95)
        response = read_response(13)

        if len(response) != 13 or response[2] != 0x95:
            print(f"❌ Invalid frame from 0x95 (frame {frame})")
            continue

        try:
            # ✅ Correct offsets based on spec
            voltage1 = ((response[5] << 8) | response[6]) / 1000.0
            voltage2 = ((response[7] << 8) | response[8]) / 1000.0
            voltage3 = ((response[9] << 8) | response[10]) / 1000.0

            base = frame * 3
            if base + 1 <= 19:
                cell_voltages.append((base + 1, voltage1))
            if base + 2 <= 19:
                cell_voltages.append((base + 2, voltage2))
            if base + 3 <= 19:
                cell_voltages.append((base + 3, voltage3))

        except Exception as e:
            print(f"⚠️ Error decoding frame {frame}: {e}")

    if not cell_voltages:
        print("⚠️ No cell voltages found.")
    else:
        for idx, voltage in cell_voltages:
            print(f"  Cell {idx:02}: {voltage:.3f} V")

# ========== Decode 0x96 (NTC Temperatures) ==========
def decode_0x96():
    print("\n🌡️ NTC Temperatures (First 4):")
    temps = []
    for frame in range(3):  # Max 3 frames to cover 16 NTCs
        send_request(0x96)
        response = read_response(9)
        if len(response) != 9 or response[2] != 0x96:
            print(f"❌ Invalid frame from 0x96 (frame {frame})")
            continue

        # Read bytes 3 to 8 (6 temperatures per frame max)
        for i in range(3, 9):
            if len(temps) >= 4:
                break
            raw = response[i]
            temp = raw - 40
            if temp > -20:  # Filter out noise/empty sensors
                temps.append(temp)

    for i, t in enumerate(temps):
        print(f"  NTC {i+1}: {t} °C")

# ========== Main Loop ==========
print("📡 Polling Daly BMS...\n")

while True:
    # Step 1: Voltage, Current, SOC
    send_request(0x90)
    res_90 = read_response(13)
    print("\n🧾 Response 0x90:", ' '.join(f'{b:02X}' for b in res_90))
    decode_0x90(res_90)

    # Step 2: Cell Voltages
    decode_0x95()

    # Step 3: NTC Temperatures
    decode_0x96()

    print("\n⏳ Waiting 10s...\n" + "-" * 40)
    time.sleep(10)
