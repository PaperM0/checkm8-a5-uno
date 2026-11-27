import sys
import time

import serial

port = sys.argv[1]
seconds = float(sys.argv[2])
outfile = sys.argv[3]
open_tries = int(sys.argv[4]) if len(sys.argv) > 4 else 30

f = open(outfile, "a", encoding="utf-8")


def note(msg):
    line = f"{time.strftime('%H:%M:%S')} [logger] {msg}"
    print(line, flush=True)
    f.write(line + "\n")
    f.flush()


def open_port(tries=30):
    for attempt in range(1, tries + 1):
        try:
            handle = serial.Serial(port, 115200, timeout=0.2)
            note(f"port opened on attempt {attempt}")
            return handle
        except serial.SerialException as exc:
            if attempt == 1 or attempt % 10 == 0:
                note(f"open attempt {attempt}/{tries} failed: {exc}")
            time.sleep(1)
    return None


f.write(f"\n=== monitor start {time.strftime('%Y-%m-%d %H:%M:%S')} ({seconds:.0f}s) ===\n")
f.flush()
note(f"monitoring {port} for {seconds:.0f}s -> {outfile}")

ser = open_port(open_tries)
if ser is None:
    note("could not open port, giving up")
    sys.exit(1)

deadline = time.time() + seconds

while time.time() < deadline:
    try:
        raw = ser.readline()
    except serial.SerialException as exc:
        note(f"serial lost: {exc}")
        try:
            ser.close()
        except Exception:
            pass
        ser = open_port()
        if ser is None:
            note("reconnect failed, giving up")
            break
        note("reconnected")
        continue

    if not raw:
        continue
    line = f"{time.strftime('%H:%M:%S')} {raw.decode('utf-8', 'replace').rstrip()}"
    print(line, flush=True)
    f.write(line + "\n")
    f.flush()

note("capture done")
f.close()
