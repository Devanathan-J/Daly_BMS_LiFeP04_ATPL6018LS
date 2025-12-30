import serial
import time
import requests

# === CONFIGURATION ===
SERIAL_PORT = "COM6"
BAUD_RATE = 9600
WEB_APP_URL = ""  # Replace with your actual Web App URL

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)

def calculate_checksum(data):
    return sum(data) & 0xFF

def send_request(register):
    ser.reset_input_buffer()
    request = [0xA5, 0x40, register, 0x08] + [0x00] * 8
    request.append(calculate_checksum(request))
    ser.write(bytearray(request))
    time.sleep(0.3)

def read_response(num_bytes):
    return ser.read(num_bytes)

def send_to_google_script(voltage, current, soc, cells, temps):
    payload = {
        "voltage": round(voltage, 2),
        "current": round(current, 2),
        "soc": round(soc, 2),
        "cells": [round(v, 3) for _, v in cells],
        "temps": temps
    }

    try:
        res = requests.post(WEB_APP_URL, json=payload)
        if res.status_code == 200:
            print("✅ Sent to Google Sheet")
        else:
            print(f"❌ Google Sheet Error: {res.status_code}")
    except Exception as e:
        print(f"⚠️ Exception: {e}")

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
            # ✅ Your custom format
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
    
    return cell_voltages

print("📡 Polling Daly BMS...\n")

while True:
    # === 0x90: Voltage, Current, SOC
    send_request(0x90)
    res_90 = read_response(13)
    print("\n🧾 Response 0x90:", ' '.join(f'{b:02X}' for b in res_90))

    if len(res_90) == 13 and res_90[2] == 0x90:
        voltage = ((res_90[4] << 8) | res_90[5]) * 0.1
        current = (((res_90[8] << 8) | res_90[9]) - 30000) * 0.1
        soc = ((res_90[10] << 8) | res_90[11]) * 0.1

        print("🔋 Battery Info:")
        print(f"  Voltage : {voltage:.1f} V")
        print(f"  Current : {current:.1f} A")
        print(f"  SOC     : {soc:.1f} %")
    else:
        voltage = current = soc = 0
        print("❌ Invalid 0x90 response.")

    # === 0x95: Cell Voltages
    cell_voltages = decode_0x95()

    # === 0x96: Temperatures
    print("\n🌡️ NTC Temperatures:")
    temps = []
    for frame in range(3):
        send_request(0x96)
        res_96 = read_response(9)
        if len(res_96) == 9 and res_96[2] == 0x96:
            for i in range(3, 9):
                if len(temps) >= 4:
                    break
                raw = res_96[i]
                temp = raw - 40
                if -20 < temp < 100:
                    temps.append(temp)

    for i, t in enumerate(temps):
        print(f"  NTC {i+1}: {t} °C")

    # === Send to Sheets
    send_to_google_script(voltage, current, soc, cell_voltages, temps)

    print("\n⏳ Waiting 10s...\n" + "-" * 40)
    time.sleep(10)
