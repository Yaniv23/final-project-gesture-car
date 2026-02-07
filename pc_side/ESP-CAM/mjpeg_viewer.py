#!/usr/bin/env python3
"""
ESP32-CAM MJPEG Viewer with Flip and Rotation Controls

Connects to ESP32-CAM MJPEG stream and displays it with real-time flip and rotation controls.
Keyboard shortcuts:
  H/h - Toggle horizontal flip
  V/v - Toggle vertical flip
  B/b - Toggle both flips
  L/l - Rotate clockwise 90°
  R/r - Reset all flips and rotation
  Q/q or ESC - Quit
"""

import os
# Set OpenCV to use a backend that works on Linux
os.environ['QT_QPA_PLATFORM'] = 'xcb'  # Use X11 backend for Qt

import cv2
import numpy as np
import requests
import re
import sys
import time

# Configuration
DEFAULT_IP = "172.20.10.2"
STREAM_PATH = "/mjpeg/1"
BOUNDARY = "+++===123454321===+++"
CONNECTION_TIMEOUT = 5
RECONNECT_DELAY = 2
WINDOW_NAME = "ESP32-CAM MJPEG Viewer"


class MJPEGViewer:
    def __init__(self, ip_address=DEFAULT_IP):
        self.ip_address = ip_address
        self.stream_url = f"http://{ip_address}{STREAM_PATH}"
        self.flip_horizontal = False
        self.flip_vertical = False
        self.rotation_angle = 0  # 0, 90, 180, 270 degrees (clockwise)
        self.running = True
        self.frame_count = 0
        
    def get_flip_code(self):
        """Calculate OpenCV flip code based on current flip state."""
        if self.flip_horizontal and self.flip_vertical:
            return -1  # Both flips
        elif self.flip_horizontal:
            return 1   # Horizontal flip
        elif self.flip_vertical:
            return 0   # Vertical flip
        else:
            return None  # No flip
    
    def apply_flip(self, frame):
        """Apply flip transformation to frame."""
        flip_code = self.get_flip_code()
        if flip_code is not None:
            frame = cv2.flip(frame, flip_code)
        return frame
    
    def apply_rotation(self, frame):
        """Apply rotation transformation to frame (clockwise)."""
        if self.rotation_angle == 0:
            return frame
        
        # Try using cv2.rotate (available in OpenCV 3.2+)
        # Constants: ROTATE_90_CLOCKWISE=0, ROTATE_180=1, ROTATE_90_COUNTERCLOCKWISE=2
        if hasattr(cv2, 'rotate'):
            try:
                if self.rotation_angle == 90:
                    return cv2.rotate(frame, getattr(cv2, 'ROTATE_90_CLOCKWISE', 0))
                elif self.rotation_angle == 180:
                    return cv2.rotate(frame, getattr(cv2, 'ROTATE_180', 1))
                elif self.rotation_angle == 270:
                    return cv2.rotate(frame, getattr(cv2, 'ROTATE_90_COUNTERCLOCKWISE', 2))
            except Exception:
                pass  # Fall through to manual rotation
        
        # Fallback for older OpenCV versions or if cv2.rotate fails
        if self.rotation_angle == 90:
            # 90° clockwise = transpose + flip horizontally
            return cv2.flip(cv2.transpose(frame), 1)
        elif self.rotation_angle == 180:
            # 180° = flip both axes
            return cv2.flip(cv2.flip(frame, 0), 1)
        elif self.rotation_angle == 270:
            # 270° clockwise = transpose + flip vertically
            return cv2.flip(cv2.transpose(frame), 0)
        
        return frame
    
    def apply_transforms(self, frame):
        """Apply all transformations (flip then rotation)."""
        frame = self.apply_flip(frame)
        frame = self.apply_rotation(frame)
        return frame
    
    def get_window_title(self):
        """Generate window title with current flip and rotation state."""
        status_parts = []
        if self.flip_horizontal:
            status_parts.append("H-Flip")
        if self.flip_vertical:
            status_parts.append("V-Flip")
        if self.rotation_angle != 0:
            status_parts.append(f"Rot{self.rotation_angle}°")
        
        status_str = " | ".join(status_parts) if status_parts else "Normal"
        return f"{WINDOW_NAME} - {status_str} - Frames: {self.frame_count}"
    
    def parse_multipart_stream(self, response):
        """Parse multipart/x-mixed-replace stream and yield JPEG frames."""
        # Determine multipart boundary from the HTTP Content-Type header, falling back to default.
        content_type = response.headers.get("Content-Type", "")
        boundary_str = None
        if content_type:
            match = re.search(r'boundary="?([^";]+)"?', content_type, re.IGNORECASE)
            if match:
                boundary_str = match.group(1)
        if not boundary_str:
            boundary_str = BOUNDARY
        boundary_bytes = boundary_str.encode()
        boundary_marker = b"--" + boundary_bytes
        boundary_with_crlf = b"\r\n--" + boundary_bytes + b"\r\n"
        boundary_end = b"\r\n--" + boundary_bytes + b"--\r\n"
        
        buffer = b""
        content_length = None
        frame_data = b""
        headers_processed = False
        
        try:
            for chunk in response.iter_content(chunk_size=16384):
                if not self.running:
                    break
                    
                if not chunk:
                    continue
                    
                buffer += chunk
                
                # Process frames in a loop until no more complete frames can be extracted
                while True:
                    # Skip HTTP headers on first connection
                    if not headers_processed:
                        # Look for the first boundary to skip HTTP response headers
                        boundary_idx = buffer.find(boundary_marker)
                        if boundary_idx == -1:
                            # No boundary found yet, need more data
                            break
                        
                        # Skip everything up to and including the boundary
                        # Find the end of the boundary line
                        after_boundary = buffer[boundary_idx:]
                        crlf_pos = after_boundary.find(b"\r\n", len(boundary_marker))
                        if crlf_pos == -1:
                            # Boundary line not complete yet
                            break
                        
                        # Skip boundary and move to headers
                        buffer = after_boundary[crlf_pos + 2:]
                        headers_processed = True
                    
                    # If we're collecting frame data and have content length
                    if content_length is not None:
                        needed = content_length - len(frame_data)
                        if needed <= 0:
                            # We have a complete frame
                            try:
                                frame_array = np.frombuffer(frame_data, dtype=np.uint8)
                                frame = cv2.imdecode(frame_array, cv2.IMREAD_COLOR)
                                if frame is not None:
                                    self.frame_count += 1
                                    yield frame
                                else:
                                    print(f"Warning: Failed to decode frame (size: {len(frame_data)})")
                            except Exception as e:
                                print(f"Warning: Error decoding frame: {e}")
                            
                            frame_data = b""
                            content_length = None
                            continue
                        
                        # Need more data for current frame
                        available = len(buffer)
                        if available >= needed:
                            frame_data += buffer[:needed]
                            buffer = buffer[needed:]
                            continue
                        else:
                            # Accumulate what we have
                            frame_data += buffer
                            buffer = b""
                            break
                    
                    # Look for boundary marker (with or without leading \r\n)
                    boundary_pos = buffer.find(boundary_with_crlf)
                    boundary_len = len(boundary_with_crlf)
                    
                    if boundary_pos == -1:
                        # Check if boundary might be at start without leading \r\n
                        if buffer.startswith(boundary_marker):
                            # Find the end of boundary line
                            crlf_pos = buffer.find(b"\r\n", len(boundary_marker))
                            if crlf_pos != -1:
                                boundary_pos = 0
                                boundary_len = crlf_pos + 2  # Include the \r\n
                    
                    if boundary_pos != -1:
                        # Found boundary - move past it
                        buffer = buffer[boundary_pos + boundary_len:]
                    
                    # Look for Content-Length header
                    header_end = buffer.find(b"\r\n\r\n")
                    if header_end == -1:
                        # Headers not complete
                        break
                    
                    # Parse headers
                    headers_text = buffer[:header_end].decode('utf-8', errors='ignore')
                    buffer = buffer[header_end + 4:]  # Skip \r\n\r\n
                    
                    # Extract Content-Length
                    match = re.search(r'Content-Length:\s*(\d+)', headers_text, re.IGNORECASE)
                    if match:
                        content_length = int(match.group(1))
                        frame_data = b""
                        if self.frame_count == 0:
                            print(f"Found first frame header, Content-Length: {content_length}")
                    else:
                        # No content length found
                        print(f"Warning: No Content-Length in headers: {headers_text[:100]}")
                        content_length = None
                        frame_data = b""
                    
                    # Check for stream end
                    if boundary_end in buffer:
                        break
                
                # Prevent buffer from growing too large
                if len(buffer) > 100000:  # 100KB max buffer
                    print("Warning: Buffer too large, clearing...")
                    # Try to find a JPEG start marker as fallback
                    jpeg_start = buffer.find(b'\xff\xd8\xff')
                    if jpeg_start != -1:
                        buffer = buffer[jpeg_start:]
                    else:
                        buffer = b""
                    
        except Exception as e:
            print(f"Error parsing stream: {e}")
            import traceback
            traceback.print_exc()
            raise
    
    def connect_stream(self):
        """Connect to MJPEG stream."""
        try:
            print(f"Connecting to {self.stream_url}...")
            response = requests.get(
                self.stream_url,
                stream=True,
                timeout=(CONNECTION_TIMEOUT, None),  # Finite connect timeout, no read timeout for streaming
                headers={'Connection': 'keep-alive'}
            )
            response.raise_for_status()
            print("Connected successfully!")
            print(f"Content-Type: {response.headers.get('Content-Type', 'Unknown')}")
            return response
        except requests.exceptions.RequestException as e:
            print(f"Connection error: {e}")
            return None
    
    def handle_keyboard_input(self, key):
        """Handle keyboard input for flip controls."""
        key = key & 0xFF  # Get ASCII value
        
        if key == ord('q') or key == ord('Q') or key == 27:  # Q or ESC
            self.running = False
            return True
        elif key == ord('h') or key == ord('H'):  # Horizontal flip
            self.flip_horizontal = not self.flip_horizontal
            print(f"Horizontal flip: {'ON' if self.flip_horizontal else 'OFF'}")
            return False
        elif key == ord('v') or key == ord('V'):  # Vertical flip
            self.flip_vertical = not self.flip_vertical
            print(f"Vertical flip: {'ON' if self.flip_vertical else 'OFF'}")
            return False
        elif key == ord('b') or key == ord('B'):  # Both flips
            if self.flip_horizontal and self.flip_vertical:
                # If both are on, turn both off
                self.flip_horizontal = False
                self.flip_vertical = False
                print("Both flips: OFF")
            else:
                # Turn both on
                self.flip_horizontal = True
                self.flip_vertical = True
                print("Both flips: ON")
            return False
        elif key == ord('r') or key == ord('R'):  # Reset
            self.flip_horizontal = False
            self.flip_vertical = False
            self.rotation_angle = 0
            print("Reset: All flips and rotation OFF")
            return False
        elif key == ord('l') or key == ord('L'):  # Rotate clockwise 90 degrees
            self.rotation_angle = (self.rotation_angle + 90) % 360
            print(f"Rotation: {self.rotation_angle}° clockwise")
            return False
        
        return False
    
    def run(self):
        """Main viewer loop."""
        print("\n" + "="*60)
        print("ESP32-CAM MJPEG Viewer")
        print("="*60)
        print(f"Stream URL: {self.stream_url}")
        print("\nControls:")
        print("  H/h - Toggle horizontal flip")
        print("  V/v - Toggle vertical flip")
        print("  B/b - Toggle both flips")
        print("  L/l - Rotate clockwise 90°")
        print("  R/r - Reset all flips and rotation")
        print("  Q/q or ESC - Quit")
        print("="*60 + "\n")
        
        # Create window and make sure it's visible
        print(f"OpenCV version: {cv2.__version__}")
        print(f"OpenCV build info: {cv2.getBuildInformation()[:200]}...")
        
        cv2.namedWindow(WINDOW_NAME, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(WINDOW_NAME, 640, 480)  # Set initial size
        
        # Try to use a backend that works better on Linux
        try:
            # Force window to be visible
            cv2.moveWindow(WINDOW_NAME, 100, 100)
            print("Window created successfully")
        except Exception as e:
            print(f"Warning: Could not move window: {e}")
        
        while self.running:
            response = self.connect_stream()
            if response is None:
                if self.running:
                    print(f"Reconnecting in {RECONNECT_DELAY} seconds...")
                    time.sleep(RECONNECT_DELAY)
                continue
            
            try:
                frames_received = 0
                for frame in self.parse_multipart_stream(response):
                    if not self.running:
                        break
                    
                    frames_received += 1
                    if frames_received == 1:
                        print(f"First frame received! Size: {frame.shape}")
                    
                    # Apply all transformations (flip then rotation)
                    display_frame = self.apply_transforms(frame)
                    
                    # Update window title
                    cv2.setWindowTitle(WINDOW_NAME, self.get_window_title())
                    
                    # Display frame
                    cv2.imshow(WINDOW_NAME, display_frame)
                    
                    # Handle keyboard input (non-blocking)
                    key = cv2.waitKey(1)
                    if key != -1:  # -1 means no key pressed
                        if self.handle_keyboard_input(key):
                            break
                    
                    # Small delay to prevent excessive CPU usage
                    time.sleep(0.01)
                    
            except KeyboardInterrupt:
                print("\nInterrupted by user")
                self.running = False
            except Exception as e:
                print(f"Stream error: {e}")
                if self.running:
                    print(f"Reconnecting in {RECONNECT_DELAY} seconds...")
                    time.sleep(RECONNECT_DELAY)
            finally:
                if response:
                    response.close()
        
        cv2.destroyAllWindows()
        print("\nViewer closed.")


def main():
    """Main entry point."""
    # Allow IP to be specified as command line argument
    ip_address = DEFAULT_IP
    if len(sys.argv) > 1:
        ip_address = sys.argv[1]
        print(f"Using custom IP: {ip_address}")
    
    viewer = MJPEGViewer(ip_address)
    try:
        viewer.run()
    except KeyboardInterrupt:
        print("\nExiting...")
        viewer.running = False
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
