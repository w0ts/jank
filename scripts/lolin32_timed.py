"""Send timed control lines to the Lolin32 and print its answers with timestamps.
usage: jank_exp.py PORT "DELAY_S:LINE" ...   e.g. jank_exp.py /dev/cu.usbserial-110 20:"set peer off" 30:"set peer on" 20:status"""
import sys
import time

import serial

port, steps = sys.argv[1], sys.argv[2:]
s = serial.Serial(port, 115200, timeout=0.2)
t0 = time.time()


def drain(until: float) -> None:
    while time.time() < until:
        chunk = s.read(4096)
        if chunk:
            for ln in chunk.decode(errors="replace").splitlines():
                if ln and not ln.startswith("# up="):
                    print(f"{time.time() - t0:7.1f}  {ln}", flush=True)


for step in steps:
    delay, line = step.split(":", 1)
    drain(time.time() + float(delay))
    print(f"{time.time() - t0:7.1f}> {line}", flush=True)
    s.write((line + "\n").encode())
drain(time.time() + 3.0)
s.close()
