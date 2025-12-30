import serial
import time

# Set your COM port here
ser = serial.Serial("COM6", 9600, timeout=2)

def calculate_checksum(data):
    return sum(data) & 0xFF

def decode_0x90_response(response):
    if len(response) != 13 or response[0] != 0xA5:
        print("❌ Invalid response format.")
        return

    # Check checksum
    received_checksum = response[-1]
    calculated_checksum = calculate_checksum(response[:-1])
    if received_checksum != calculated_checksum:
        print(f"⚠️ Checksum mismatch! Got {received_checksum:02X}, expected {calculated_checksum:02X}")
        return

    print("✅ Checksum OK")

    # Decode only what matters
    cumulative_voltage_raw = response[4] << 8 | response[5]
    current_raw = response[8] << 8 | response[9]
    soc_raw = response[10] << 8 | response[11]

    cumulative_voltage = cumulative_voltage_raw * 0.1
    current = (current_raw - 30000) * 0.1
    soc = soc_raw * 0.1

    print("🔍 Decoded Values:")
    print(f"🔋 Voltage: {cumulative_voltage:.1f} V")
    print(f"⚡ Current: {current:.1f} A")
    print(f"📊 SOC:     {soc:.1f} %")

# Request to read register 0x90 (Voltage, Current, SOC)
request = [0xA5, 0x40, 0x90, 0x08] + [0x00]*8
request.append(calculate_checksum(request))

print("📡 Polling BMS every 10 seconds...\n")

while True:
    ser.write(bytearray(request))
    time.sleep(0.3)
    response = ser.read(13)

    if len(response) == 13:
        print("\n🧾 Raw Hex Response:", ' '.join(f'{b:02X}' for b in response))
        decode_0x90_response(response)
    else:
        print("❌ No data or incomplete response")

    time.sleep(10)
