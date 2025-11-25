"""
Visual Verification Viewer for eyes.h library. Requires laptop.ino to be uploaded to ESP32.

Usage:
1. Upload phase1_verification_wraparound.ino to ESP32
2. Run this script
3. Type 'snap' to capture one frame
4. Type 'auto' for continuous viewing (2 FPS)
5. Type 'stop' to stop auto mode
6. Type 'q' to quit

Featured Modes: 
1. Raw
2. Overlays
3. Mask
"""

import serial
import serial.tools.list_ports
import struct
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.animation import FuncAnimation
import threading
from datetime import datetime

# Configuration
PORT = "COM5"  # Fixed port
BAUD = 115200
IMG_WIDTH = 160
IMG_HEIGHT = 120

# Global state
running = True
latest_frame = None
latest_metadata = None
frame_lock = threading.Lock()
frame_updated = False
auto_mode = False
display_mode = 2  # 1=Raw, 2=Overlays, 3=Mask
ser_global = None
cursor_hsv_text = None

# HSV Ranges (matching ESP32 eyes.h)
YELLOW_H_MIN, YELLOW_H_MAX = 15, 40
YELLOW_S_MIN, YELLOW_V_MIN = 80, 80  # Working thresholds
PINK_H_MIN, PINK_H_MAX = 145, 175
PINK_S_MIN, PINK_V_MIN = 140, 50

def find_esp32_port():
    """Auto-detect ESP32 serial port"""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if any(x in port.description.lower() for x in ['ch340', 'cp210', 'usb-serial', 'uart']):
            print(f"✓ Found ESP32 at: {port.device}")
            return port.device

    # Show all ports
    print("Available ports:")
    for port in ports:
        print(f"  {port.device}: {port.description}")
    return None

def rgb565_to_rgb888(data):
    """Convert RGB565 bytes to RGB888 numpy array"""
    raw_bytes = np.frombuffer(data, dtype=np.uint8)

    # Byte swap for ESP32
    swapped = np.empty_like(raw_bytes)
    swapped[0::2] = raw_bytes[1::2]
    swapped[1::2] = raw_bytes[0::2]

    pixels = np.frombuffer(swapped.tobytes(), dtype=np.uint16).astype(np.uint32)
    pixels = pixels.reshape((IMG_HEIGHT, IMG_WIDTH))

    # Extract RGB components
    r5 = (pixels >> 11) & 0x1F
    g6 = (pixels >> 5) & 0x3F
    b5 = pixels & 0x1F

    # Expand to 8-bit
    r8 = ((r5 << 3) | (r5 >> 2)).astype(np.uint8)
    g8 = ((g6 << 2) | (g6 >> 4)).astype(np.uint8)
    b8 = ((b5 << 3) | (b5 >> 2)).astype(np.uint8)

    # Combine
    rgb = np.zeros((IMG_HEIGHT, IMG_WIDTH, 3), dtype=np.uint8)
    rgb[:,:,0] = r8
    rgb[:,:,1] = g8
    rgb[:,:,2] = b8

    return rgb

def rgb_to_hsv_numpy(rgb_img):
    """Convert RGB to HSV"""
    r, g, b = rgb_img[:,:,0], rgb_img[:,:,1], rgb_img[:,:,2]

    max_val = np.maximum(np.maximum(r, g), b).astype(np.float32)
    min_val = np.minimum(np.minimum(r, g), b).astype(np.float32)
    delta = max_val - min_val

    v = max_val.astype(np.uint8)

    s = np.zeros_like(max_val, dtype=np.uint8)
    mask = max_val > 0
    s[mask] = ((delta[mask] / max_val[mask]) * 255).astype(np.uint8)

    h = np.zeros_like(max_val, dtype=np.float32)
    mask_r = (max_val == r) & (delta > 0)
    mask_g = (max_val == g) & (delta > 0)
    mask_b = (max_val == b) & (delta > 0)

    h[mask_r] = 30 * ((g[mask_r] - b[mask_r]) / delta[mask_r])
    h[mask_g] = 60 + 30 * ((b[mask_g] - r[mask_g]) / delta[mask_g])
    h[mask_b] = 120 + 30 * ((r[mask_b] - g[mask_b]) / delta[mask_b])

    h[h < 0] += 180
    h = h.astype(np.uint8)

    return h, s, v

def create_color_masks(rgb_img):
    """Create yellow and pink masks"""
    h, s, v = rgb_to_hsv_numpy(rgb_img)

    yellow_mask = ((h >= YELLOW_H_MIN) & (h <= YELLOW_H_MAX) &
                   (s >= YELLOW_S_MIN) & (v >= YELLOW_V_MIN))

    if PINK_H_MIN > PINK_H_MAX: # Wraps
        pink_mask = (((h >= PINK_H_MIN) | (h <= PINK_H_MAX)) &
                     (s >= PINK_S_MIN) & (v >= PINK_V_MIN))
    else: # Normal
        pink_mask = ((h >= PINK_H_MIN) & (h <= PINK_H_MAX) &
                     (s >= PINK_S_MIN) & (v >= PINK_V_MIN))

    return yellow_mask.astype(np.uint8) * 255, pink_mask.astype(np.uint8) * 255

def print_detection_results(metadata):
    """Print nice formatted text output"""
    print(f"\n{'='*60}")
    print(f"Frame {metadata['frame_num']} | Process Time: {metadata['process_ms']}ms")
    print(f"{'='*60}")

    # Yellow blob
    if metadata['yellow_on_screen']:
        offset = metadata['yellow_offset_x']
        direction = "CENTER" if offset == 0 else ("RIGHT" if offset > 0 else "LEFT")
        print(f" Yellow: FOUND")
        print(f"   Offset from center: {offset:+4d} pixels ({direction})")
        print(f"   Blob area: {metadata['yellow_area']} pixels")
    else:
        print(f" Yellow: NOT FOUND")

    # Pink blobs
    print(f"\n Pink: {metadata['num_pink']} blob(s) detected")

    if metadata['num_pink'] >= 1 and metadata['pink0_on_screen']:
        offset = metadata['pink0_offset_x']
        direction = "CENTER" if offset == 0 else ("RIGHT" if offset > 0 else "LEFT")
        print(f"   Pink [0]:")
        print(f"      Offset from center: {offset:+4d} pixels ({direction})")
        print(f"      Blob area: {metadata['pink0_area']} pixels")

    if metadata['num_pink'] >= 2 and metadata['pink1_on_screen']:
        offset = metadata['pink1_offset_x']
        direction = "CENTER" if offset == 0 else ("RIGHT" if offset > 0 else "LEFT")
        print(f"   Pink [1]:")
        print(f"      Offset from center: {offset:+4d} pixels ({direction})")
        print(f"      Blob area: {metadata['pink1_area']} pixels")

    print(f"{'='*60}")

def find_viz_header(ser):
    """Find VIZ marker in serial stream"""
    while True:
        if ser.read(1) == b'V':
            if ser.read(1) == b'I' and ser.read(1) == b'Z':
                return True

def read_exact(ser, n):
    """Read exactly n bytes"""
    buf = bytearray()
    while len(buf) < n:
        chunk = ser.read(n - len(buf))
        if not chunk:
            return None
        buf.extend(chunk)
    return bytes(buf)

def serial_reader_thread(ser):
    """Background thread to read frames from serial"""
    global latest_frame, latest_metadata, running, frame_updated

    try:
        print(f"Serial reader thread started")

        while running:
            # Find VIZ header
            if not find_viz_header(ser):
                continue

            # Read metadata (60 bytes) - matches Arduino struct
            meta_bytes = read_exact(ser, 60)
            if not meta_bytes:
                print("Timeout reading metadata")
                continue

            # Parse metadata - must match phase1_verification_wraparound.ino
            meta = struct.unpack('<HH BhhhH BBhhhH BhhhH II 20x', meta_bytes)
            metadata = {
                'width': meta[0],
                'height': meta[1],

                # Yellow
                'yellow_on_screen': bool(meta[2]),
                'yellow_offset_x': meta[3],  # signed
                'yellow_centroid_x': meta[4],
                'yellow_centroid_y': meta[5],
                'yellow_area': meta[6],

                # Pink
                'num_pink': meta[7],
                'pink0_on_screen': bool(meta[8]),
                'pink0_offset_x': meta[9],  # signed
                'pink0_centroid_x': meta[10],
                'pink0_centroid_y': meta[11],
                'pink0_area': meta[12],

                'pink1_on_screen': bool(meta[13]),
                'pink1_offset_x': meta[14],  # signed
                'pink1_centroid_x': meta[15],
                'pink1_centroid_y': meta[16],
                'pink1_area': meta[17],

                'frame_num': meta[18],
                'process_ms': meta[19]
            }

            # Read image data (RGB565)
            img_size = IMG_WIDTH * IMG_HEIGHT * 2
            img_bytes = read_exact(ser, img_size)
            if not img_bytes:
                print("Timeout reading image")
                continue

            # Convert to RGB888
            rgb_img = rgb565_to_rgb888(img_bytes)

            # Print detection results to console
            print_detection_results(metadata)

            # Update global state
            with frame_lock:
                latest_frame = rgb_img
                latest_metadata = metadata
                frame_updated = True

    except serial.SerialException as e:
        print(f"Serial error: {e}")
    except Exception as e:
        print(f"Error in reader thread: {e}")
        import traceback
        traceback.print_exc()
    finally:
        running = False

def on_key_press(event):
    """Handle keyboard shortcuts"""
    global running, auto_mode, display_mode, ser_global

    if event.key == ' ':  # Space - snap
        ser_global.write(b"SNAP\n")
        print("📸 Snap!")
    elif event.key == 'a':  # Auto mode
        auto_mode = not auto_mode
        cmd = "AUTO" if auto_mode else "STOP"
        ser_global.write(f"{cmd}\n".encode())
        print(f"Auto mode: {'ON' if auto_mode else 'OFF'}")
    elif event.key == 's':  # Stop
        auto_mode = False
        ser_global.write(b"STOP\n")
        print("Stopped")
    elif event.key == '1':  # Raw view
        display_mode = 1
        print("Mode: Raw Camera")
    elif event.key == '2':  # Overlay view
        display_mode = 2
        print("Mode: Detection Overlays")
    elif event.key == '3':  # Mask view
        display_mode = 3
        print("Mode: Color Mask Only")
    elif event.key == 'q':  # Quit
        running = False
        plt.close('all')
        print("Quitting...")

def update_plot(frame_num):
    """Update matplotlib display"""
    global frame_updated, display_mode

    with frame_lock:
        if not frame_updated:
            return
        frame = latest_frame
        metadata = latest_metadata
        frame_updated = False

    if frame is None:
        return

    ax_img.clear()
    ax_info.clear()

    # Mode 1: Raw camera feed (2x scale)
    if display_mode == 1:
        display = np.repeat(np.repeat(frame, 2, axis=0), 2, axis=1)
        ax_img.imshow(display)
        ax_img.set_title('Mode 1: Raw Camera', fontsize=14, fontweight='bold', color='white')

    # Mode 2: Detection overlays
    elif display_mode == 2:
        display = np.repeat(np.repeat(frame, 2, axis=0), 2, axis=1)
        ax_img.imshow(display)
        ax_img.set_title('Mode 2: Detection Overlays', fontsize=14, fontweight='bold', color='lime')

        # Draw yellow blob
        if metadata['yellow_on_screen']:
            cx = metadata['yellow_centroid_x'] * 2
            cy = metadata['yellow_centroid_y'] * 2
            ax_img.plot([cx-10, cx+10], [cy, cy], 'y-', linewidth=2)
            ax_img.plot([cx, cx], [cy-10, cy+10], 'y-', linewidth=2)
            ax_img.plot(cx, cy, 'yo', markersize=8)

        # Draw pink blobs
        if metadata['pink0_on_screen']:
            cx = metadata['pink0_centroid_x'] * 2
            cy = metadata['pink0_centroid_y'] * 2
            ax_img.plot([cx-10, cx+10], [cy, cy], 'm-', linewidth=2)
            ax_img.plot([cx, cx], [cy-10, cy+10], 'm-', linewidth=2)
            ax_img.plot(cx, cy, 'mo', markersize=8)

        if metadata['pink1_on_screen']:
            cx = metadata['pink1_centroid_x'] * 2
            cy = metadata['pink1_centroid_y'] * 2
            ax_img.plot([cx-10, cx+10], [cy, cy], 'm-', linewidth=2)
            ax_img.plot([cx, cx], [cy-10, cy+10], 'm-', linewidth=2)
            ax_img.plot(cx, cy, 'mo', markersize=8)

        # Draw center line
        ax_img.axvline(x=IMG_WIDTH, color='lime', linestyle='--', linewidth=1, alpha=0.5)

    # Mode 3: Color mask only
    elif display_mode == 3:
        yellow_mask, pink_mask = create_color_masks(frame)

        mask_display = np.zeros((IMG_HEIGHT, IMG_WIDTH, 3), dtype=np.uint8)
        mask_display[pink_mask > 0] = [255, 0, 255]
        mask_display[yellow_mask > 0] = [255, 255, 0]

        mask_display = np.repeat(np.repeat(mask_display, 2, axis=0), 2, axis=1)
        ax_img.imshow(mask_display)
        ax_img.set_title('Mode 3: Color Mask', fontsize=14, fontweight='bold', color='yellow')

        ax_img.axvline(x=IMG_WIDTH, color='lime', linestyle='--', linewidth=1, alpha=0.5)

    ax_img.axis('off')

    # Info panel
    ax_info.axis('off')
    info_text = []

    info_text.append(f"Frame: {metadata['frame_num']:4d}  |  Process: {metadata['process_ms']:3d}ms")
    info_text.append("")

    # Yellow
    if metadata['yellow_on_screen']:
        offset = metadata['yellow_offset_x']
        direction = "CENTER" if offset == 0 else ("RIGHT" if offset > 0 else "LEFT")
        info_text.append(f"YELLOW: FOUND")
        info_text.append(f"   Offset: {offset:+4d} px ({direction})")
        info_text.append(f"   Area:   {metadata['yellow_area']:4d} px")
    else:
        info_text.append(" YELLOW: NOT FOUND")

    info_text.append("")

    # Pink
    info_text.append(f" PINK: {metadata['num_pink']} blob(s)")
    if metadata['pink0_on_screen']:
        offset = metadata['pink0_offset_x']
        direction = "CENTER" if offset == 0 else ("RIGHT" if offset > 0 else "LEFT")
        info_text.append(f"   [0] Offset: {offset:+4d} px ({direction})")
        info_text.append(f"       Area:   {metadata['pink0_area']:4d} px")

    if metadata['pink1_on_screen']:
        offset = metadata['pink1_offset_x']
        direction = "CENTER" if offset == 0 else ("RIGHT" if offset > 0 else "LEFT")
        info_text.append(f"   [1] Offset: {offset:+4d} px ({direction})")
        info_text.append(f"       Area:   {metadata['pink1_area']:4d} px")

    text = '\n'.join(info_text)
    ax_info.text(0.1, 0.95, text,
                 transform=ax_info.transAxes,
                 fontsize=11,
                 verticalalignment='top',
                 fontfamily='monospace',
                 bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.3))

    # Draw HSV debug text if available
    if cursor_hsv_text:
        ax_img.text(5, 5, cursor_hsv_text, color='lime', fontsize=10, 
                   fontweight='bold', bbox=dict(facecolor='black', alpha=0.7))

def main():
    global running, ax_img, ax_info, ser_global

    print("=" * 70)
    print("  PHASE 1: ESP32 BLOB DETECTION VIEWER")
    print("=" * 70)

    # Open serial
    try:
        ser = serial.Serial(PORT, BAUD, timeout=5)
        ser.setDTR(False)
        ser.setRTS(False)
        print(f"✓ Connected to {PORT}")
        ser_global = ser
    except Exception as e:
        print(f"✗ ERROR: Could not open {PORT}")
        print(f"  {e}")
        return

    # Start reader thread
    reader = threading.Thread(target=serial_reader_thread, args=(ser,), daemon=True)
    reader.start()

    # Setup matplotlib
    fig = plt.figure(figsize=(14, 7))
    fig.patch.set_facecolor('#1e1e1e')
    ax_img = plt.subplot(1, 2, 1)
    ax_info = plt.subplot(1, 2, 2)

    fig.canvas.mpl_connect('key_press_event', on_key_press)
    
    # Add mouse hover for HSV inspection
    def on_move(event):
        global cursor_hsv_text
        if event.inaxes == ax_img and latest_frame is not None:
            x, y = int(event.xdata), int(event.ydata)
            # Handle 2x scaling in modes 1 and 2
            if display_mode in [1, 2]:
                x //= 2
                y //= 2
            
            if 0 <= x < IMG_WIDTH and 0 <= y < IMG_HEIGHT:
                # Cast to int to avoid overflow
                r = int(latest_frame[y, x][0])
                g = int(latest_frame[y, x][1])
                b = int(latest_frame[y, x][2])
                
                # Calculate HSV manually
                mx = max(r, g, b)
                mn = min(r, g, b)
                df = mx - mn
                if mx == mn: h = 0
                elif mx == r: h = (60 * ((g - b) / df) + 360) % 360
                elif mx == g: h = (60 * ((b - r) / df) + 120) % 360
                elif mx == b: h = (60 * ((r - g) / df) + 240) % 360
                h = int(h / 2) # Scale to 0-179
                s = int(0 if mx == 0 else (df / mx) * 255)
                v = int(mx)
                
                cursor_hsv_text = f"HSV: {h:3d}, {s:3d}, {v:3d}\nRGB: {r:3d}, {g:3d}, {b:3d}"
        else:
            cursor_hsv_text = None

    fig.canvas.mpl_connect('motion_notify_event', on_move)
    plt.tight_layout()

    print("\n" + "=" * 70)
    print("  KEYBOARD SHORTCUTS:")
    print("=" * 70)
    print("  SPACE   - Snap single frame")
    print("  A       - Toggle auto mode")
    print("  S       - Stop auto mode")
    print("  1/2/3   - View modes")
    print("  Q       - Quit")
    print("=" * 70)
    print("\n Press SPACE or A to start...\n")

    # Animation
    ani = FuncAnimation(fig, update_plot, interval=100, cache_frame_data=False)

    try:
        plt.show()
    except KeyboardInterrupt:
        print("\n⚠ Interrupted")
    finally:
        running = False
        ser.close()
        print("✓ Viewer closed.")

if __name__ == "__main__":
    main()
