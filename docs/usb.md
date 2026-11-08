# USB

An S3 build is one USB device with three functions: a network adapter (CDC-NCM), a MIDI port called jank, and a small
read-only drive (label JANK) holding `jank.html`, the control page.

- **No serial port.** The S3 has 5 IN endpoints including EP0; NCM takes 2, MIDI 1 and the drive 1. The control line
  goes as SysEx on the MIDI port instead (`core/jank_sysex.h`: `F0 7D 'J' 'K'`, 7-bit text, `F7`, 30 characters a
  frame).
- **Clock to the computer.** The timer interrupt that writes the UART also queues the bytes for USB; a task hands them
  to TinyUSB, which cannot be called from an interrupt.
- **USB ID.** Espressif's VID with a made-up PID, `0x4A4B`: fine on one's own desk, not for distribution.

## Host mode

With no computer on the port, the stick sends its clock to USB-MIDI gear: up to 4 devices, through a hub too. It starts
as a device; when no computer has spoken within 3 s (`HOST_WAIT_MS`) it restarts once as host, so a clock on a charger
starts about 3 s late. The choice lasts until the next start. Host mode has no network adapter, drive or control page.

jank does not switch power to the device. On a bare XIAO the 5V pin is the USB-C socket's VBUS with nothing between
them (v1.2 schematic), so 5 V on that pin powers the device, or a powered hub does. The XIAO presents 5.1 k on both CC
pins (a sink): USB-A gear behind an adapter does not care; USB-C gear plugged straight in may not power up.

**Never feed a bare XIAO's 5V pin with a computer on its USB port**: nothing separates the two supplies. The PCB adds an
ideal diode and a firmware cut for this (`hardware/pcb/README.md`, "The power path"); that cut is not in the firmware
yet.
