import sys
import time

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM28"
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 4.0

with serial.Serial(port, 115200, timeout=0.2) as ser:
    ser.reset_input_buffer()
    end = time.time() + seconds
    chunks = []
    while time.time() < end:
        data = ser.read(4096)
        if data:
            chunks.append(data)
    out = b"".join(chunks)
    print(f"--- {len(out)} bytes from {port} @115200 ---")
    print(out.decode("utf-8", "replace"))
