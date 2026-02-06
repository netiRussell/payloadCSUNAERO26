"""
Camera viewer for ESP32. Shows what the robot sees.

Usage:
  1. Upload laptop.ino to ESP32
  2. python viewer.py
  3. Press SPACE to snap, A for auto, Q to quit

Install: pip install pyserial numpy pillow
"""

import serial
import struct
import numpy as np
import tkinter as tk
from PIL import Image, ImageTk, ImageDraw

# Config - change COM port if needed
PORT = "COM10"
BAUD = 115200
WIDTH = 160
HEIGHT = 120
SCALE = 4

class Viewer:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("ESP32 Camera")
        self.root.configure(bg='black')

        # Main frame to hold canvas and stats side by side
        main_frame = tk.Frame(self.root, bg='black')
        main_frame.pack()

        # Canvas for camera image
        self.canvas = tk.Canvas(main_frame, width=WIDTH*SCALE, height=HEIGHT*SCALE, bg='gray20')
        self.canvas.pack(side=tk.LEFT)

        # Stats panel on the right
        self.stats = tk.Label(main_frame, text="", fg='white', bg='gray10',
                              font=('Consolas', 11), justify=tk.LEFT, anchor='nw',
                              width=20, height=15, padx=10, pady=10)
        self.stats.pack(side=tk.LEFT, fill=tk.Y)

        # Controls label at bottom
        self.label = tk.Label(self.root, text="Connecting...", fg='gray', bg='black', font=('Consolas', 9))
        self.label.pack()

        print(f"Opening {PORT}...")
        try:
            self.ser = serial.Serial(PORT, BAUD, timeout=0.1)
            print(f"Connected to {PORT}")
        except Exception as e:
            print(f"ERROR: {e}")
            self.label.config(text=f"ERROR: {e}")
            self.ser = None
            self.root.mainloop()
            return

        self.auto = False
        self.buffer = b''
        self.frame_count = 0

        self.root.bind('<space>', lambda e: self.snap())
        self.root.bind('a', self.toggle_auto)
        self.root.bind('q', lambda e: self.root.quit())

        self.label.config(text="SPACE=snap  A=auto  Q=quit")
        self.update()
        self.root.mainloop()
        if self.ser:
            self.ser.close()

    def snap(self):
        self.ser.write(b"SNAP\n")

    def toggle_auto(self, e):
        self.auto = not self.auto
        self.ser.write(b"AUTO\n" if self.auto else b"STOP\n")
        self.label.config(text=f"Auto: {'ON' if self.auto else 'OFF'}  |  SPACE=snap  Q=quit")

    def update(self):
        if not self.ser:
            return

        # Read all available serial data
        waiting = self.ser.in_waiting
        if waiting:
            self.buffer += self.ser.read(waiting)

        # Look for VIZ header
        idx = self.buffer.find(b'VIZ')
        if idx >= 0:
            data_after_viz = len(self.buffer) - idx - 3
            if data_after_viz >= 60 + WIDTH * HEIGHT * 2:
                start = idx + 3
                meta = self.buffer[start:start+60]
                img_bytes = self.buffer[start+60:start+60+WIDTH*HEIGHT*2]
                self.buffer = self.buffer[start+60+WIDTH*HEIGHT*2:]

                self.frame_count += 1

                # Parse metadata
                try:
                    m = struct.unpack('<HH BhhhH BBhhhH BhhhH II 20x', meta)
                    yellow_found = m[2]
                    yellow_offset = m[3]
                    yellow_x = m[4]
                    yellow_area = m[6]
                    pink_count = m[7]
                    pink0_offset = m[9]
                    pink0_x = m[10]
                    pink0_area = m[12]
                    pink1_offset = m[14]
                    pink1_x = m[15]
                    pink1_area = m[17]
                    process_ms = m[19]
                except:
                    self.root.after(30, self.update)
                    return

                # Convert RGB565 to RGB
                raw = np.frombuffer(img_bytes, dtype=np.uint8)
                swapped = np.empty_like(raw)
                swapped[0::2] = raw[1::2]
                swapped[1::2] = raw[0::2]
                pixels = np.frombuffer(swapped.tobytes(), dtype=np.uint16).reshape((HEIGHT, WIDTH))

                r = (((pixels >> 11) & 0x1F) << 3).astype(np.uint8)
                g = (((pixels >> 5) & 0x3F) << 2).astype(np.uint8)
                b = ((pixels & 0x1F) << 3).astype(np.uint8)

                img = Image.fromarray(np.stack([r, g, b], axis=-1))
                img = img.transpose(Image.ROTATE_180)
                img = img.resize((WIDTH*SCALE, HEIGHT*SCALE), Image.NEAREST)

                draw = ImageDraw.Draw(img)

                # Flip offsets and positions for 180 rotation
                yellow_offset = -yellow_offset
                yellow_x = WIDTH - yellow_x
                pink0_offset = -pink0_offset
                pink0_x = WIDTH - pink0_x
                pink1_offset = -pink1_offset
                pink1_x = WIDTH - pink1_x

                # Center line (green)
                cx = WIDTH*SCALE // 2
                draw.line([(cx, 0), (cx, HEIGHT*SCALE)], fill='green', width=2)

                # Yellow cross at top
                if yellow_found:
                    x = yellow_x * SCALE
                    y = 30  # near top
                    size = 15
                    draw.line([(x-size, y), (x+size, y)], fill='yellow', width=3)
                    draw.line([(x, y-size), (x, y+size)], fill='yellow', width=3)

                # Pink crosses at top
                if pink_count >= 1:
                    x = pink0_x * SCALE
                    y = 30
                    size = 15
                    draw.line([(x-size, y), (x+size, y)], fill='magenta', width=3)
                    draw.line([(x, y-size), (x, y+size)], fill='magenta', width=3)
                if pink_count >= 2:
                    x = pink1_x * SCALE
                    y = 30
                    size = 15
                    draw.line([(x-size, y), (x+size, y)], fill='magenta', width=3)
                    draw.line([(x, y-size), (x, y+size)], fill='magenta', width=3)

                self.photo = ImageTk.PhotoImage(img)
                self.canvas.delete("all")
                self.canvas.create_image(0, 0, anchor='nw', image=self.photo)

                # Update stats
                stats_text = f"Frame: {self.frame_count}\n"
                stats_text += f"Process: {process_ms}ms\n"
                stats_text += f"\n--- YELLOW ---\n"
                if yellow_found:
                    direction = "CENTER" if yellow_offset == 0 else ("RIGHT" if yellow_offset > 0 else "LEFT")
                    stats_text += f"Found: YES\n"
                    stats_text += f"Offset: {yellow_offset:+d}px\n"
                    stats_text += f"Dir: {direction}\n"
                    stats_text += f"Area: {yellow_area}px\n"
                else:
                    stats_text += f"Found: NO\n"

                stats_text += f"\n--- PINK ---\n"
                stats_text += f"Count: {pink_count}\n"
                if pink_count >= 1:
                    stats_text += f"[0] {pink0_offset:+d}px\n"
                    stats_text += f"    Area: {pink0_area}px\n"
                if pink_count >= 2:
                    stats_text += f"[1] {pink1_offset:+d}px\n"
                    stats_text += f"    Area: {pink1_area}px\n"

                self.stats.config(text=stats_text)

        self.root.after(30, self.update)

if __name__ == "__main__":
    Viewer()
