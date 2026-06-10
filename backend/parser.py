#!/usr/bin/env python3
"""
parser.py -- minimal host-side reader for the IAQ gateway.

Reads newline-delimited JSON from the gateway's serial port, validates each
line, prints it, and appends it to a CSV log. This is the "vertical slice"
tool: get this printing real numbers before you bother with the dashboard.

Usage:
    python parser.py --port COM7            # Windows
    python parser.py --port /dev/ttyACM0    # Linux
    python parser.py                        # auto-detect first likely port

Find your port:
    python -m serial.tools.list_ports -v
"""
import argparse
import csv
import json
import sys
import time
from datetime import datetime

import serial
from serial.tools import list_ports

FIELDS = ["co2", "temp", "rh", "pm25", "tvoc", "eco2", "lux", "rssi", "pkt"]


def autodetect_port():
    for p in list_ports.comports():
        desc = (p.description or "").lower()
        if any(k in desc for k in ("acm", "cdc", "iaq", "usb serial", "jlink")):
            return p.device
    ports = list_ports.comports()
    return ports[0].device if ports else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", help="serial port (e.g. COM7 or /dev/ttyACM0)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--csv", default="iaq_log.csv")
    args = ap.parse_args()

    port = args.port or autodetect_port()
    if not port:
        sys.exit("No serial port found. Plug in the gateway and pass --port.")

    print(f"Opening {port} @ {args.baud} ...")
    ser = serial.Serial(port, args.baud, timeout=2)

    with open(args.csv, "a", newline="") as f:
        writer = csv.writer(f)
        if f.tell() == 0:
            writer.writerow(["timestamp"] + FIELDS)

        while True:
            try:
                raw = ser.readline().decode("utf-8", "ignore").strip()
            except serial.SerialException as e:
                print(f"Serial error: {e}; retrying in 2 s")
                time.sleep(2)
                continue
            if not raw:
                continue
            try:
                rec = json.loads(raw)
            except json.JSONDecodeError:
                continue  # ignore boot noise / partial lines

            if "co2" not in rec:
                print("status:", rec)
                continue

            ts = datetime.now().isoformat(timespec="seconds")
            print(ts, rec)
            writer.writerow([ts] + [rec.get(k, "") for k in FIELDS])
            f.flush()


if __name__ == "__main__":
    main()
