import cv2
import mediapipe as mp
import math
import serial
import serial.tools.list_ports
import time
from constant import COM_PORT, BAUD_RATE

# === SETUP ===
mp_hands = mp.solutions.hands
hands = None  # Will be initialized in main()
mp_draw = mp.solutions.drawing_utils

cap = None  # Will be initialized in main()

# Serial to ESP32 - Try to connect, but make it optional
ser = None

def find_available_ports():
    """Find all available COM ports"""
    ports = serial.tools.list_ports.comports()
    available = []
    for port in ports:
        available.append(port.device)
    return available

# Try to connect to serial port
try:
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0.1)
    time.sleep(2)
    print(f"[OK] Connected to {COM_PORT}")
except serial.SerialException as e:
    print(f"[WARNING] Could not connect to {COM_PORT}: {e}")
    print("Available COM ports:")
    available_ports = find_available_ports()
    if available_ports:
        for port in available_ports:
            print(f"  - {port}")
        print(f"\nTo use a different port, change COM_PORT in the script or connect your ESP32.")
    else:
        print("  No COM ports found.")
    print("\n[INFO] Running in camera-only mode (no serial communication)")
    print("       Hand tracking will still work, but commands won't be sent to ESP32.\n")

# Stability filtering
last_detected = ""
current_display = ""
stable_counter = 0
stable_threshold = 10  # Frames needed to confirm a new label

def get_direction_label(angle_deg):
    if -22.5 < angle_deg <= 22.5:
        return "Sideway_Right"
    elif 22.5 < angle_deg <= 67.5:
        return "diagonal_forward_right"
    elif 67.5 < angle_deg <= 112.5:
        return "Forward"
    elif 112.5 < angle_deg <= 157.5:
        return "diagonal_forward_left"
    elif 157.5 < angle_deg or angle_deg <= -157.5:
        return "Sideway_Left"
    elif -157.5 < angle_deg <= -112.5:
        return "diagonal_backward_left"
    elif -112.5 < angle_deg <= -67.5:
        return "Backward"
    elif -67.5 < angle_deg <= -22.5:
        return "diagonal_backward_right"
    else:
        return "Center"

def is_hand_closed(landmarks):
    fingers = {
        "index": (8, 6),
        "middle": (12, 10),
        "ring": (16, 14),
        "pinky": (20, 18),
    }
    bent_count = 0
    for tip_idx, pip_idx in fingers.values():
        if landmarks[tip_idx].y > landmarks[pip_idx].y:
            bent_count += 1
    return bent_count == 4

def is_index_finger_up(landmarks):
    return (
        landmarks[8].y < landmarks[6].y and
        landmarks[12].y > landmarks[10].y and
        landmarks[16].y > landmarks[14].y and
        landmarks[20].y > landmarks[18].y
    )

def is_index_finger_down(landmarks):
    return landmarks[8].y > landmarks[6].y

def is_hand_open(landmarks):
    return (
        landmarks[8].y < landmarks[6].y and
        landmarks[12].y < landmarks[10].y and
        landmarks[16].y < landmarks[14].y and
        landmarks[20].y < landmarks[18].y
    )

# === Circle Gestures ===
def is_circle_ccw(landmarks, w, h):
    x1, y1 = int(landmarks[4].x * w), int(landmarks[4].y * h)  # Thumb tip
    x2, y2 = int(landmarks[8].x * w), int(landmarks[8].y * h)  # Index tip
    distance = math.hypot(x2 - x1, y2 - y1)
    return distance < 40 and x2 > x1  # Index to the right → CW

def is_circle_cw(landmarks, w, h):
    x1, y1 = int(landmarks[4].x * w), int(landmarks[4].y * h)  # Thumb tip
    x2, y2 = int(landmarks[8].x * w), int(landmarks[8].y * h)  # Index tipq
    distance = math.hypot(x2 - x1, y2 - y1)
    return distance < 40 and x2 < x1  # Index to the left → CCW

def main():
    """Main function for hand tracking - can be called from other modules"""
    global cap, ser, last_detected, current_display, stable_counter, hands
    
    # Initialize MediaPipe if not already done
    if hands is None:
        hands = mp_hands.Hands()
    
    # Initialize webcam if not already done
    if cap is None or not cap.isOpened():
        cap = cv2.VideoCapture(0)
        if not cap.isOpened():
            print("[ERROR] Could not open webcam!")
            return
        print("[Hand_Tracker] Webcam opened successfully")
    
    window_name = "Hand + Serial"
    print(f"[Hand_Tracker] Starting main loop, window name: {window_name}")
    
    # Create window explicitly before main loop
    try:
        cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(window_name, 640, 480)
        print(f"[Hand_Tracker] Window '{window_name}' created and ready")
    except Exception as e:
        print(f"[Hand_Tracker] Error creating window: {e}")
        import traceback
        traceback.print_exc()
        return
    
    # === MAIN LOOP ===
    frame_count = 0
    iteration_count = 0
    while True:
        iteration_count += 1
        ret, frame = cap.read()
        if not ret:
            frame_count += 1
            if frame_count > 10:
                print("[Hand_Tracker] Too many failed frame reads, exiting")
                break
            continue
        
        frame_count = 0  # Reset counter on success
            
        frame = cv2.flip(frame, 1)
        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results = hands.process(frame_rgb)

        h, w, _ = frame.shape
        cx, cy = w // 2, h // 2
        center_threshold = 60

       # Draw axes and center neutral zone
        cv2.line(frame, (cx, 0), (cx, h), (200, 200, 200), 1)
        cv2.line(frame, (0, cy), (w, cy), (200, 200, 200), 1)
        cv2.line(frame, (0, 0), (w, h), (180, 180, 180), 1)        # Diagonal ↘️
        cv2.line(frame, (w, 0), (0, h), (180, 180, 180), 1)        # Diagonal ↙️
        cv2.circle(frame, (cx, cy), center_threshold, (100, 100, 255), 1)

        detected_label = ""

        if results.multi_hand_landmarks:
            hand_landmarks = results.multi_hand_landmarks[0]
            landmarks = hand_landmarks.landmark

            x_avg = sum(lm.x for lm in landmarks) / 21
            y_avg = sum(lm.y for lm in landmarks) / 21
            hx, hy = int(x_avg * w), int(y_avg * h)

            dx = hx - cx
            dy = cy - hy
            angle_deg = math.degrees(math.atan2(dy, dx))
            distance_to_center = math.hypot(dx, dy)

            # === GESTURE DECISION TREE ===
            
            
            if is_index_finger_up(landmarks):
                detected_label = "Forward"
            elif is_hand_closed(landmarks):
                detected_label = "Stop"
            elif is_circle_ccw(landmarks, w, h):
                detected_label = "rotate_ccw"
            elif is_circle_cw(landmarks, w, h):
                detected_label = "rotate_cw"    
            elif is_index_finger_down(landmarks):
                detected_label = "Backward"
            elif is_hand_open(landmarks) and distance_to_center < center_threshold:
                detected_label = "Center"
            else:
                detected_label = get_direction_label(angle_deg)

            # Draw hand
            mp_draw.draw_landmarks(frame, hand_landmarks, mp_hands.HAND_CONNECTIONS)
            cv2.circle(frame, (hx, hy), 10, (0, 255, 0), -1)
            cv2.line(frame, (cx, cy), (hx, hy), (255, 0, 0), 2)
        else:
            # No hand detected - send Stop command
            detected_label = "Stop"

        # Stability filter (applies to both hand detected and no hand detected)
        if detected_label == last_detected:
            stable_counter += 1
        else:
            stable_counter = 0
            last_detected = detected_label

        if stable_counter >= stable_threshold:
            if current_display != detected_label:
                current_display = detected_label
                if ser and ser.is_open:
                    try:
                        ser.write((current_display + '\n').encode('utf-8'))
                        print("Sent to ESP32:", current_display)
                    except Exception as e:
                        print("Serial write error:", e)
                else:
                    print("Detected (no serial):", current_display)

        # Read serial
        if ser and ser.is_open:
            try:
                if ser.in_waiting:
                    line = ser.readline().decode('utf-8').strip()
                    if line:
                        print("ESP32:", line)
            except Exception as e:
                print("Serial read error:", e)

        # Display label
        color = {
            "Stop": (0, 0, 255),
            "Forward": (0, 255, 0),
            "Downward": (255, 255, 0),
            "Turn_CW": (255, 0, 255),
            "Turn_CCW": (0, 255, 255),
            "Center": (100, 100, 255)
        }.get(current_display, (0, 255, 255))

        cv2.putText(frame, current_display, (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 1, color, 2)
        
        # Display frame
        try:
            cv2.imshow(window_name, frame)
            # Important: waitKey must be called for window to update
            # Use waitKey(1) - non-blocking, returns immediately
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                print("[Hand_Tracker] 'q' key pressed, exiting")
                break
        except cv2.error as e:
            print(f"[Hand_Tracker] OpenCV error: {e}")
            break
        except Exception as e:
            print(f"[Hand_Tracker] Error displaying frame: {e}")
            import traceback
            traceback.print_exc()
            break
        
        # Check if window was closed by clicking X button (check less frequently to avoid slowdown)
        # Only check every 30 iterations to avoid performance issues
        if iteration_count % 30 == 0:
            try:
                if cv2.getWindowProperty(window_name, cv2.WND_PROP_VISIBLE) < 1:
                    print("[Hand_Tracker] Window closed by user")
                    break
            except:
                # Window might not exist, continue
                pass

    # Cleanup
    cap.release()
    if ser and ser.is_open:
        ser.close()
        print("Serial port closed")
    # Only destroy this specific window, not all windows
    try:
        cv2.destroyWindow("Hand + Serial")
    except:
        pass
    # Don't call destroyAllWindows() as it closes other windows too


if __name__ == "__main__":
    main()