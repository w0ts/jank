"""Listen on the Lolin32's console. usage: console.py PORT SECONDS [LINE ...]: each LINE is sent after 3 s, 1 s apart."""
import sys
import time

import serial

port, seconds, lines = sys.argv[1], float(sys.argv[2]), sys.argv[3:]
s = serial.Serial(port, 115200, timeout=0.2)
end = time.time() + seconds
sent_at = time.time() + 3.0
while time.time() < end:
    chunk = s.read(4096)
    if chunk:
        sys.stdout.write(chunk.decode(errors="replace"))
        sys.stdout.flush()
    if lines and time.time() >= sent_at:
        s.write((lines.pop(0) + "\n").encode())
        sent_at = time.time() + 1.0
s.close()
