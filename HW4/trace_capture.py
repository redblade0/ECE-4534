"""Capture labeled MSPM0 tilt-lock telemetry over the XDS110 UART.

Example: python trace_capture.py --port COM5 --output trace.csv
Requires pyserial. Hold each pose before pressing Enter for that row group.
"""

import argparse
import csv
import time

import serial


FIELDS = [
    "recorded_s", "physical_pose", "tick", "x", "y", "z", "roll10",
    "pitch10", "candidate", "stable", "step", "event", "led", "valid",
    "cal_x", "cal_y", "cal_z_level",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--seconds", type=float, default=3.0)
    parser.add_argument("--output", default="tilt-trace.csv")
    parser.add_argument(
        "--dynamic", action="store_true",
        help="Record movement immediately after Enter instead of a steady hold",
    )
    parser.add_argument(
        "--poses",
        default="level,left_edge_down,level,right_edge_down,level,far_edge_down,level,usb_side_edge_down",
        help="Comma-separated physical poses to label and capture in order",
    )
    args = parser.parse_args()

    poses = tuple(pose.strip() for pose in args.poses.split(",") if pose.strip())
    with serial.Serial(args.port, args.baud, timeout=0.25) as port, open(
        args.output, "w", newline="", encoding="utf-8"
    ) as output:
        writer = csv.writer(output)
        writer.writerow(FIELDS)
        input("Keep the board level. Press Enter, then press RESET within ten seconds... ")
        port.reset_input_buffer()
        calibration = None
        deadline = time.monotonic() + 10.0
        while time.monotonic() < deadline:
            line = port.readline().decode("ascii", errors="replace").strip()
            parts = line.split(",")
            if len(parts) == 4 and parts[0] == "CAL":
                calibration = parts[1:]
                break
        if calibration is None:
            raise RuntimeError("CAL line not found. Restart and press RESET after the Enter prompt.")
        print("Calibration X, Y, Z-level:", ", ".join(calibration))
        start = time.monotonic()
        for pose in poses:
            if args.dynamic:
                input(f"Prepare {pose}; press Enter, then move immediately for "
                      f"the {args.seconds:g}-second recording... ")
            else:
                input(f"Hold {pose}, then press Enter to record "
                      f"{args.seconds:g} seconds... ")
            port.reset_input_buffer()
            deadline = time.monotonic() + args.seconds
            count = 0
            while time.monotonic() < deadline:
                line = port.readline().decode("ascii", errors="replace").strip()
                parts = line.split(",")
                if len(parts) != 13 or parts[0] != "D":
                    continue
                writer.writerow([f"{time.monotonic() - start:.3f}", pose,
                                 *parts[1:], *calibration])
                count += 1
            output.flush()
            print(f"Captured {count} samples for {pose}.")
    print(f"Saved {args.output}")


if __name__ == "__main__":
    main()
