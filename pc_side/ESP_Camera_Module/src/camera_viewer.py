import argparse
import socket
import cv2
import numpy as np
import struct
import time
from collections import defaultdict
from datetime import datetime, timedelta


# === CONFIGURATION ===
# UDP port to listen on (must match ESP32 configuration)
UDP_PORT = 5000
DISCOVERY_PORT = 5001  # Port pour la découverte
PACKET_HEADER_SIZE = 8
FRAME_TIMEOUT_SECONDS = 5.0  # Discard incomplete frames after 5 seconds (increased for unstable connections)
STATS_INTERVAL_SECONDS = 5.0  # Print statistics every 5 seconds
DISCOVERY_TIMEOUT = 5.0  # Timeout pour la découverte en secondes
FRAME_DISPLAY_WAIT_MS = 30  # Wait 30ms between frame displays (matches ESP32 frame rate)


def parse_args():
    parser = argparse.ArgumentParser(
        description="UDP viewer for ESP32-S3 OV2640 camera stream"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=UDP_PORT,
        help=f"UDP port to listen on (default: {UDP_PORT})",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Enable verbose debugging output",
    )
    parser.add_argument(
        "--no-discovery",
        action="store_true",
        help="Skip automatic ESP32 discovery (use broadcast mode)",
    )
    return parser.parse_args()


def discover_esp32(timeout=DISCOVERY_TIMEOUT, verbose=False):
    """
    Découvre l'ESP32 en envoyant un message de découverte en broadcast.
    Retourne l'IP de l'ESP32 si trouvée, None sinon.
    """
    try:
        # Créer un socket pour la découverte
        discovery_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        discovery_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        discovery_sock.settimeout(timeout)
        
        # Envoyer le message de découverte en broadcast
        message = "DISCOVER_CAMERA_VIEWER"
        broadcast_addr = ("255.255.255.255", DISCOVERY_PORT)
        
        if verbose:
            print(f"[Discovery] Sending discovery message to {broadcast_addr[0]}:{broadcast_addr[1]}")
        
        discovery_sock.sendto(message.encode(), broadcast_addr)
        
        # Attendre la réponse
        try:
            data, addr = discovery_sock.recvfrom(1024)
            response = data.decode('utf-8', errors='ignore')
            
            esp32_ip = None
            if response.startswith("CAMERA_IP:"):
                esp32_ip = response.split(":", 1)[1].strip()
            elif response.startswith("ESP32-CAM:"):
                parts = response.split(":")
                if len(parts) >= 2:
                    esp32_ip = parts[1].strip()
            
            if esp32_ip:
                if verbose:
                    print(f"[Discovery] ESP32 found! IP: {esp32_ip} (from {addr[0]})")
                
                # Envoyer notre IP à l'ESP32 pour qu'il l'apprenne
                try:
                    # Obtenir notre IP locale
                    temp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    temp_sock.connect(("8.8.8.8", 80))
                    local_ip = temp_sock.getsockname()[0]
                    temp_sock.close()
                    
                    # Envoyer notre IP à l'ESP32
                    viewer_ip_msg = f"VIEWER_IP:{local_ip}"
                    discovery_sock.sendto(viewer_ip_msg.encode(), (esp32_ip, DISCOVERY_PORT))
                    
                    if verbose:
                        print(f"[Discovery] Sent our IP ({local_ip}) to ESP32")
                except Exception as e:
                    if verbose:
                        print(f"[Discovery] Could not send our IP to ESP32: {e}")
                
                discovery_sock.close()
                return esp32_ip
            else:
                if verbose:
                    print(f"[Discovery] Received unexpected response: {response}")
        except socket.timeout:
            if verbose:
                print(f"[Discovery] Timeout waiting for ESP32 response")
        
        discovery_sock.close()
        return None
        
    except Exception as e:
        if verbose:
            print(f"[Discovery] Error during discovery: {e}")
        return None


def is_valid_jpeg(data):
    """Validate JPEG data by checking markers"""
    if len(data) < 2:
        return False
    # Check JPEG start marker (0xFFD8)
    if data[0] != 0xFF or data[1] != 0xD8:
        return False
    # Check JPEG end marker (0xFFD9) - should be near the end
    if len(data) < 4:
        return False
    # Look for end marker in last 1024 bytes
    end_marker_pos = data.rfind(b'\xFF\xD9')
    if end_marker_pos == -1:
        return False
    # End marker should be within last 10% of data (allowing for some trailing data)
    if end_marker_pos < len(data) * 0.9:
        return False
    return True


def main():
    args = parse_args()
    port = args.port
    verbose = args.verbose

    # Découvrir l'ESP32 si demandé
    esp32_ip = None
    if not args.no_discovery:
        print("[Discovery] Attempting to discover ESP32 camera...")
        esp32_ip = discover_esp32(timeout=DISCOVERY_TIMEOUT, verbose=verbose)
        if esp32_ip:
            print(f"[Discovery] Successfully discovered ESP32 at {esp32_ip}")
            print("[Discovery] ESP32 will now send stream to this PC")
        else:
            print("[Discovery] ESP32 not found, will listen for broadcast stream")
            print("[Discovery] Make sure ESP32 is connected to the same WiFi network")
    else:
        print("[Discovery] Discovery skipped (--no-discovery flag)")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Increase socket buffer size to handle bursts
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536 * 2)  # 128KB buffer
    sock.bind(("0.0.0.0", port))
    sock.settimeout(0.1)

    print(f"\n[Listening] UDP packets on port {port}")
    print("Waiting for ESP32 camera stream...")

    window_name = "ESP32 Car Camera"
    frame_count = 0
    frames_decoded = 0
    frames_corrupted = 0
    frames_timeout = 0
    packets_received = 0
    window_created = False
    
    fragment_buffers = defaultdict(dict)  # frame_id -> {fragment_id -> data}
    frame_timestamps = defaultdict(lambda: datetime.now())  # frame_id -> first_packet_time
    last_complete_frame_id = 0
    last_stats_time = datetime.now()

    while True:
        try:
            data, addr = sock.recvfrom(65507)
            packets_received += 1
            
            if len(data) < PACKET_HEADER_SIZE:
                if verbose:
                    print(f"[WARN] Packet too small: {len(data)} bytes")
                continue
            
            frame_id, fragment_id, total_fragments = struct.unpack('<IHH', data[:PACKET_HEADER_SIZE])
            fragment_data = data[PACKET_HEADER_SIZE:]

            # Ignore header-only packet (fragment_id 0xFFFF) used as frame start marker
            if fragment_id == 0xFFFF:
                continue
            
            # Track when we first see this frame
            if frame_id not in fragment_buffers:
                frame_timestamps[frame_id] = datetime.now()
            
            # Store fragment (overwrites if duplicate)
            fragment_buffers[frame_id][fragment_id] = fragment_data
            
            # Check if frame is complete
            if len(fragment_buffers[frame_id]) == total_fragments:
                # Calculate actual frame size from fragments (fix bug)
                frame_parts = [fragment_buffers[frame_id][i] for i in range(total_fragments)]
                complete_frame = b''.join(frame_parts)
                actual_frame_size = len(complete_frame)
                
                # Cleanup old incomplete frames (more aggressive cleanup)
                if frame_id > last_complete_frame_id + 3:
                    frames_to_remove = [fid for fid in fragment_buffers.keys() if fid < frame_id - 3]
                    for fid in frames_to_remove:
                        frames_timeout += 1  # Count as timeout
                        del fragment_buffers[fid]
                        if fid in frame_timestamps:
                            del frame_timestamps[fid]
                
                last_complete_frame_id = frame_id
                frame_count += 1
                
                # Validate JPEG before decoding
                if not is_valid_jpeg(complete_frame):
                    frames_corrupted += 1
                    if verbose:
                        print(f"[WARN] Frame {frame_id} failed JPEG validation")
                    del fragment_buffers[frame_id]
                    if frame_id in frame_timestamps:
                        del frame_timestamps[frame_id]
                    continue
                
                # Decode frame
                try:
                    nparr = np.frombuffer(complete_frame, np.uint8)
                    frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
                    
                    if frame is not None:
                        frames_decoded += 1
                        cv2.imshow(window_name, frame)
                        window_created = True
                    else:
                        frames_corrupted += 1
                        if verbose:
                            print(f"[WARN] Frame {frame_id} decode returned None")
                except cv2.error as e:
                    frames_corrupted += 1
                    if verbose:
                        print(f"[WARN] Frame {frame_id} decode error: {e}")
                except Exception as e:
                    frames_corrupted += 1
                    if verbose:
                        print(f"[WARN] Frame {frame_id} unexpected error: {e}")
                
                # Cleanup
                del fragment_buffers[frame_id]
                if frame_id in frame_timestamps:
                    del frame_timestamps[frame_id]
            
        except socket.timeout:
            # Check for timeout frames
            now = datetime.now()
            timeout_frames = []
            for frame_id, first_seen in frame_timestamps.items():
                if (now - first_seen).total_seconds() > FRAME_TIMEOUT_SECONDS:
                    timeout_frames.append(frame_id)
            
            for frame_id in timeout_frames:
                frames_timeout += 1
                if verbose:
                    print(f"[WARN] Frame {frame_id} timed out (incomplete)")
                del fragment_buffers[frame_id]
                del frame_timestamps[frame_id]
            
            pass
        except KeyboardInterrupt:
            print("\n[INFO] Interrupted by user")
            break
        except socket.error as e:
            # Network errors - log but don't break
            if verbose:
                print(f"[ERROR] Socket error: {e}")
            time.sleep(0.1)  # Brief pause on network errors
        except struct.error as e:
            # Packet parsing errors - log but continue
            if verbose:
                print(f"[ERROR] Packet parsing error: {e}")
        except Exception as e:
            # Other errors - log but continue (don't break stream)
            if verbose:
                print(f"[ERROR] Unexpected error: {e}")
            time.sleep(0.1)  # Brief pause on errors

        # Periodic statistics
        now = datetime.now()
        if (now - last_stats_time).total_seconds() >= STATS_INTERVAL_SECONDS:
            total_frames = frame_count
            success_rate = (frames_decoded / total_frames * 100) if total_frames > 0 else 0
            loss_rate = (frames_timeout / total_frames * 100) if total_frames > 0 else 0
            corruption_rate = (frames_corrupted / total_frames * 100) if total_frames > 0 else 0
            
            print(f"[Stats] Frames: {total_frames} | Decoded: {frames_decoded} ({success_rate:.1f}%) | "
                  f"Timeout: {frames_timeout} ({loss_rate:.1f}%) | Corrupted: {frames_corrupted} ({corruption_rate:.1f}%) | "
                  f"Packets: {packets_received}")
            last_stats_time = now

        if window_created:
            key = cv2.waitKey(1) & 0xFF
            if key == ord("q"):
                print("[INFO] Quit key pressed")
                break

            try:
                if cv2.getWindowProperty(window_name, cv2.WND_PROP_VISIBLE) < 1:
                    print("[INFO] Window closed")
                    break
            except cv2.error:
                # Window might be destroyed, continue
                pass
        else:
            time.sleep(0.01)

    sock.close()
    try:
        cv2.destroyWindow(window_name)
    except:
        pass
    
    print(f"\n[INFO] Stream ended. Final stats:")
    print(f"  Total frames received: {frame_count}")
    print(f"  Frames decoded: {frames_decoded}")
    print(f"  Frames timed out: {frames_timeout}")
    print(f"  Frames corrupted: {frames_corrupted}")
    print(f"  Total packets: {packets_received}")


if __name__ == "__main__":
    main()


