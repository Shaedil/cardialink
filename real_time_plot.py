#!/usr/bin/env python3
"""
real_time_plot.py

Reads newline-terminated numeric values from a serial port and
plots them in real time using matplotlib.animation.
"""

import argparse
import time
from collections import deque

import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation

def main():
    p = argparse.ArgumentParser(description="Real-time plot of serial data")
    p.add_argument("-p", "--port",  default="COM17",
                   help="Serial port (e.g. COM3 or /dev/ttyUSB0)")
    p.add_argument("-b", "--baud",  type=int, default=115200,
                   help="Baud rate")
    p.add_argument("-w", "--window", type=int, default=200,
                   help="Number of points to show in rolling window")
    p.add_argument("-y", "--ylim",  type=float, nargs=2, default=[0, 4095],
                   help="Y-axis limits (min max)")
    args = p.parse_args()

    # -- open serial port --
    ser = serial.Serial(args.port, args.baud, timeout=1)
    time.sleep(2)  # give MCU time to reset & start sending

    # -- prepare data buffer & plot --
    data = deque([0.0]*args.window, maxlen=args.window)
    fig, ax = plt.subplots()
    line, = ax.plot(data)
    ax.set_ylim(*args.ylim)
    ax.set_xlim(0, args.window-1)
    ax.set_xlabel("Sample #")
    ax.set_ylabel("Raw ADC value")
    ax.set_title(f"Real‐time data from {args.port} @ {args.baud} baud")

    # -- animation callback --
    def update(frame):
        try:
            raw = ser.readline().decode("utf-8").strip()
            if raw:
                val = float(raw)
                data.append(val)
                line.set_ydata(data)
        except Exception as e:
            # you can uncomment next line to debug parsing issues
            # print("Parse error:", e, "line:", raw)
            pass
        return line,

    ani = animation.FuncAnimation(
        fig,
        update,
        interval=20,   # redraw every 20 ms → ~50 Hz
        blit=True
    )

    plt.show()
    ser.close()

if __name__ == "__main__":
    main()
