#!/usr/bin/env python3
"""
Simple UART monitor / terminal.

- Continuously reads and prints whatever comes in over serial.
- Typing "clear" (then Enter) clears your LOCAL terminal only —
  nothing is sent over the wire.
- Typing anything else (then Enter) sends it over UART, followed by
  a newline (\\r\\n), same as a normal serial terminal.
- Ctrl+C to quit.

Install dependency once:
    pip install pyserial

Usage:
    python3 uart_monitor.py /dev/cu.usbmodemL450027H1 115200
    python3 uart_monitor.py COM5 115200
"""

import sys
import os
import threading
import serial
from datetime import datetime


def clear_screen():
    os.system("cls" if os.name == "nt" else "clear")


def reader_thread(ser: serial.Serial, stop_event: threading.Event):
    """Continuously read from the serial port and print to stdout,
    prefixing each new line of incoming data with a timestamp."""
    at_line_start = True
    while not stop_event.is_set():
        try:
            data = ser.read(ser.in_waiting or 1)
            if data:
                try:
                    text = data.decode(errors="replace")
                except Exception:
                    text = repr(data)

                for ch in text:
                    if at_line_start:
                        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                        sys.stdout.write(f"[{ts}] ")
                        at_line_start = False
                    sys.stdout.write(ch)
                    if ch == "\n":
                        at_line_start = True
                sys.stdout.flush()
        except serial.SerialException as e:
            print(f"\n[serial error: {e}]")
            stop_event.set()
            break


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 uart_monitor.py <port> [baud]")
        print("Example: python3 uart_monitor.py /dev/cu.usbmodemL450027H1 115200")
        sys.exit(1)

    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

    try:
        ser = serial.Serial(port, baud, timeout=0.1)
    except serial.SerialException as e:
        print(f"Could not open {port}: {e}")
        sys.exit(1)

    print(f"Connected to {port} @ {baud} baud.")
    print("Type a line and press Enter to send it over UART.")
    print("Type 'clear' and press Enter to clear the local screen only.")
    print("Ctrl+C to quit.\n")

    stop_event = threading.Event()
    t = threading.Thread(target=reader_thread, args=(ser, stop_event), daemon=True)
    t.start()

    try:
        while not stop_event.is_set():
            try:
                line = input()
            except EOFError:
                break

            if line.strip().lower() == "clear":
                clear_screen()
                continue

            try:
                ser.write((line + "\r\n").encode())
            except serial.SerialException as e:
                print(f"[write error: {e}]")
                break
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        ser.close()
        print("\nConnection closed.")


if __name__ == "__main__":
    main()
