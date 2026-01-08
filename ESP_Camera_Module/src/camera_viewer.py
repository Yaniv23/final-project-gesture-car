import argparse
import socket
import cv2
import numpy as np
import struct
from collections import defaultdict


# === CONFIGURATION ===
# UDP port to listen on (must match ESP32 configuration)
UDP_PORT = 5000

# Packet header structure (8 bytes: uint32_t frame_id, uint16_t fragment_id, uint16_t total_fragments)
PACKET_HEADER_SIZE = 8


def parse_args():
    """
    Optional command line argument for UDP port:
      python camera_viewer.py --port 5000
    """
    parser = argparse.ArgumentParser(
        description="UDP viewer for ESP32-S3 OV2640 camera stream"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=UDP_PORT,
        help=f"UDP port to listen on (default: {UDP_PORT})",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    port = args.port

    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", port))
    sock.settimeout(0.1)  # Non-blocking with timeout for keyboard input

    print(f"[INFO] Listening for UDP packets on port {port}")
    print("[INFO] Waiting for ESP32 camera stream...")
    print("[INFO] Press 'q' to quit, or close the window.")

    window_name = "ESP32 Car Camera"
    frame_count = 0
    window_created = False
    last_status_time = 0
    import time
    
    # Fragment reassembly buffers
    fragment_buffers = defaultdict(dict)  # {frame_id: {fragment_id: data}}
    frame_sizes = defaultdict(int)  # {frame_id: total_size}
    last_complete_frame_id = 0

    while True:
        try:
            # Receive UDP packet (max UDP size is 65507 bytes)
            data, addr = sock.recvfrom(65507)
            
            if len(data) < PACKET_HEADER_SIZE:
                continue
            
            # Parse packet header (little-endian to match ESP32)
            frame_id, fragment_id, total_fragments = struct.unpack('<IHH', data[:PACKET_HEADER_SIZE])
            fragment_data = data[PACKET_HEADER_SIZE:]
            
            # Store fragment
            fragment_buffers[frame_id][fragment_id] = fragment_data
            frame_sizes[frame_id] += len(fragment_data)
            
            # Check if we have all fragments for this frame
            if len(fragment_buffers[frame_id]) == total_fragments:
                # Reassemble frame
                frame_parts = [fragment_buffers[frame_id][i] for i in range(total_fragments)]
                complete_frame = b''.join(frame_parts)
                
                # Clean up old frames (keep only last 5 frames in memory)
                if frame_id > last_complete_frame_id + 5:
                    frames_to_remove = [fid for fid in fragment_buffers.keys() if fid < frame_id - 5]
                    for fid in frames_to_remove:
                        del fragment_buffers[fid]
                        if fid in frame_sizes:
                            del frame_sizes[fid]
                
                last_complete_frame_id = frame_id
                
                # Decode JPEG frame
                try:
                    # Convert bytes to numpy array
                    nparr = np.frombuffer(complete_frame, np.uint8)
                    # Decode JPEG
                    frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
                    
                    if frame is not None:
                        frame_count += 1
                        if frame_count == 1:
                            print(f"[INFO] First frame received! Starting stream from {addr[0]}")
                        
                        if frame_count % 30 == 0:
                            print(f"[INFO] Received frame #{frame_count} from {addr[0]} ({len(complete_frame)} bytes)")
                        
                        # Note: Camera already applies vflip and hmirror in main.cpp
                        # No additional flip needed here, or adjust based on your needs
                        # frame = cv2.flip(frame, 0)  # Commented out to avoid double flip
                        
                        cv2.imshow(window_name, frame)
                        window_created = True
                    else:
                        print("[WARNING] Failed to decode JPEG frame")
                except Exception as e:
                    print(f"[ERROR] Frame decode error: {e}")
                
                # Clean up this frame's fragments
                del fragment_buffers[frame_id]
                if frame_id in frame_sizes:
                    del frame_sizes[frame_id]
        except socket.timeout:
            # Timeout is expected for non-blocking socket
            # Print status message every 5 seconds
            current_time = time.time()
            if current_time - last_status_time > 5:
                print("[INFO] Still waiting for ESP32 camera stream... (Press 'q' to quit)")
                last_status_time = current_time
        except KeyboardInterrupt:
            print("\n[INFO] Interrupted by user")
            break
        except Exception as e:
            print(f"[ERROR] Socket error: {e}")
            break

        # Check for quit key (only if window exists)
        if window_created:
            key = cv2.waitKey(1) & 0xFF
            if key == ord("q"):
                break

            # Check if window was closed
            try:
                if cv2.getWindowProperty(window_name, cv2.WND_PROP_VISIBLE) < 1:
                    break
            except cv2.error:
                # Window doesn't exist or was closed
                break
        else:
            # No window yet, just check for keyboard interrupt
            time.sleep(0.01)  # Small delay to prevent CPU spinning

    print(f"\n[INFO] Closing viewer... (received {frame_count} frames)")
    sock.close()
    try:
        cv2.destroyWindow(window_name)
    except:
        pass


if __name__ == "__main__":
    main()


