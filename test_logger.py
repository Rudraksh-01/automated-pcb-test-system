"""PC-side logger for the Automated PCB Test Station.

For every board: asks for a serial number (type it or scan a barcode),
tells the ESP32 to start the test, reads the result, appends it to a CSV
file and prints running yield and a failure-code count.

Usage:
    python test_logger.py --port COM3
    python test_logger.py --port /dev/ttyUSB0 --out results.csv
"""
import argparse
import csv
import datetime as dt
import os
from collections import Counter

import serial

FAIL_CODES = {
    "E00": "No failure",
    "E01": "Voltage on rail before power-up (leakage / short)",
    "E02": "Overcurrent / short circuit",
    "E03": "5 V rail out of tolerance",
    "E04": "3.3 V rail out of tolerance",
    "E05": "Board draws too little current",
    "E06": "Functional test failed",
}
HEADER = ["timestamp", "serial", "result", "v5_V", "v3v3_V", "current_mA", "fail_code", "fail_reason"]


def run_one(ser, serial_no, timeout=15):
    ser.reset_input_buffer()
    ser.write(f"START,{serial_no}\n".encode())
    end = dt.datetime.now() + dt.timedelta(seconds=timeout)
    while dt.datetime.now() < end:
        line = ser.readline().decode(errors="ignore").strip()
        if line.startswith("RESULT,"):
            return line.split(",")[1:]
    return None


def summarize(total, passed, failures):
    if total == 0:
        return
    print(f"\nTested: {total}  Pass: {passed}  Fail: {total - passed}  Yield: {100 * passed / total:.1f}%")
    for code, n in failures.most_common():
        print(f"  {code} x{n}  {FAIL_CODES.get(code, '')}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="serial port of the ESP32, e.g. COM3 or /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", default="results.csv")
    args = ap.parse_args()

    new_file = not os.path.exists(args.out)
    total = passed = 0
    failures = Counter()

    with serial.Serial(args.port, args.baud, timeout=1) as ser, open(args.out, "a", newline="") as f:
        writer = csv.writer(f)
        if new_file:
            writer.writerow(HEADER)
        print("Ready. Enter/scan a serial number per board. Type 'q' to quit.")
        try:
            while True:
                sn = input("Serial number: ").strip()
                if sn.lower() == "q":
                    break
                if not sn:
                    continue
                res = run_one(ser, sn)
                if res is None:
                    print("  No response from tester (check port / power).")
                    continue
                serial_no, verdict, v5, v33, i_ma, code = res
                total += 1
                if verdict == "PASS":
                    passed += 1
                else:
                    failures[code] += 1
                writer.writerow([dt.datetime.now().isoformat(timespec="seconds"), serial_no, verdict,
                                 v5, v33, i_ma, code, FAIL_CODES.get(code, "")])
                f.flush()
                print(f"  {verdict}  5V={v5}  3V3={v33}  I={i_ma} mA  {FAIL_CODES.get(code, code)}")
        except KeyboardInterrupt:
            pass
    summarize(total, passed, failures)


if __name__ == "__main__":
    main()
