import cv2
import mediapipe as mp
import math
import serial
import serial.tools.list_ports
import time
from constant import COM_PORT, BAUD_RATE

# === SETUP ===
mp_hands = mp.solutions.hands

# ✅ allow up to 2 hands, but we will LOCK on one (so it won't jump)
hands = mp_hands.Hands(
    static_image_mode=False,
    max_num_hands=2,
    min_detection_confidence=0.6,
    min_tracking_confidence=0.6
)

mp_draw = mp.solutions.drawing_utils
cap = cv2.VideoCapture(0)

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

# ==========================
# ✅ Hand lock (no jumping)
# ==========================
locked_center = None
missing_frames = 0
MISSING_RELEASE_FRAMES = 12   # frames without hand before releasing lock
SWITCH_DISTANCE_PX = 220      # if the "best" hand is too far, don't switch

def hand_center_from_landmarks(hand_landmarks, w, h):
    lms = hand_landmarks.landmark
    x_avg = sum(lm.x for lm in lms) / 21
    y_avg = sum(lm.y for lm in lms) / 21
    return int(x_avg * w), int(y_avg * h)

def choose_locked_hand(multi_hand_landmarks, w, h, locked_center):
    centers = [hand_center_from_landmarks(hm, w, h) for hm in multi_hand_landmarks]

    if locked_center is None:
        return 0, multi_hand_landmarks[0], centers[0], 0.0

    dists = [math.hypot(c[0] - locked_center[0], c[1] - locked_center[1]) for c in centers]
    best_idx = min(range(len(dists)), key=lambda i: dists[i])
    return best_idx, multi_hand_landmarks[best_idx], centers[best_idx], dists[best_idx]

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

def count_fingers_up(landmarks):
    """Return the number of fingers (index, middle, ring, pinky) that are 'up'."""
    fingers = [
        (8, 6),   # index
        (12, 10), # middle
        (16, 14), # ring
        (20, 18), # pinky
    ]
    count = 0
    for tip_idx, pip_idx in fingers:
        if landmarks[tip_idx].y < landmarks[pip_idx].y:
            count += 1
    return count

def is_hand_open(landmarks):
    return (
        landmarks[8].y < landmarks[6].y and
        landmarks[12].y < landmarks[10].y and
        landmarks[16].y < landmarks[14].y and
        landmarks[20].y < landmarks[18].y and
        landmarks[20].y < landmarks[18].y
    )

# Circle gestures (disabled)
# The thumb-index circle gestures were previously used to emit rotate_cw / rotate_ccw.
# They are intentionally left out (disabled) to avoid accidental rotation triggers.

# === MAIN LOOP ===
while True:
    ret, frame = cap.read()
    if not ret:
        break

    frame = cv2.flip(frame, 1)
    frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    results = hands.process(frame_rgb)

    h, w, _ = frame.shape
    cx, cy = w // 2, h // 2
    center_threshold = 60

    # Draw axes (✅ no center circle)
    cv2.line(frame, (cx, 0), (cx, h), (200, 200, 200), 1)
    cv2.line(frame, (0, cy), (w, cy), (200, 200, 200), 1)
    cv2.line(frame, (0, 0), (w, h), (180, 180, 180), 1)        # Diagonal ↘️
    cv2.line(frame, (w, 0), (0, h), (180, 180, 180), 1)        # Diagonal ↙️
    # cv2.circle(frame, (cx, cy), center_threshold, (100, 100, 255), 1)  # ❌ removed

    detected_label = ""

    if results.multi_hand_landmarks:
        # ✅ choose ONE hand (locked) instead of always [0]
        best_idx, hand_landmarks, chosen_center, best_dist = choose_locked_hand(
            results.multi_hand_landmarks, w, h, locked_center
        )

        # update lock only if not a big jump
        if locked_center is None or best_dist <= SWITCH_DISTANCE_PX:
            locked_center = chosen_center

        missing_frames = 0

        landmarks = hand_landmarks.landmark

        x_avg = sum(lm.x for lm in landmarks) / 21
        y_avg = sum(lm.y for lm in landmarks) / 21

        # ✅ keep your original hx/hy calculation, but if you prefer stability use chosen_center:
        # hx, hy = chosen_center
        hx, hy = int(x_avg * w), int(y_avg * h)

        dx = hx - cx
        dy = cy - hy
        angle_deg = math.degrees(math.atan2(dy, dx))
        distance_to_center = math.hypot(dx, dy)

        # === GESTURE DECISION TREE ===
        # Priority:
        # 1. Stop (hand closed)
        # 2. Forward/Backward determined ONLY by hand position (angle)
        # 3. Rotation gestures (finger counts or circle gestures)
        # 4. Center (open hand near center)
        # 5. Fallback to directional label
        if is_hand_closed(landmarks):
            detected_label = "Stop"
        else:
            pos_label = get_direction_label(angle_deg)

            # Forward/Backward are controlled only by hand position
            if pos_label in ("Forward", "Backward"):
                detected_label = pos_label
            else:
                # Finger-based rotation gestures (do not affect Forward/Backward)
                fingers_up = count_fingers_up(landmarks)
                if fingers_up == 1:
                    detected_label = "Rotate_Right"
                elif fingers_up == 2:
                    detected_label = "Rotate_Left"
                # Keep circle gestures as alternative rotation inputs
                # Circle gestures disabled (no rotate_ccw / rotate_cw)
                elif is_hand_open(landmarks) and distance_to_center < center_threshold:
                    detected_label = "Center"
                else:
                    detected_label = pos_label

        # Draw hand
        mp_draw.draw_landmarks(frame, hand_landmarks, mp_hands.HAND_CONNECTIONS)
        cv2.circle(frame, (hx, hy), 10, (0, 255, 0), -1)
        cv2.line(frame, (cx, cy), (hx, hy), (255, 0, 0), 2)

    else:
        # No hand detected - send Stop command
        missing_frames += 1
        if missing_frames >= MISSING_RELEASE_FRAMES:
            locked_center = None
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
            # Do not send any labels that contain 'Center'
            if 'Center' in current_display:
                # Suppress sending center labels to the ESP32
                print("[SUPPRESS] Center label suppressed - not sent")
            else:
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

    # Display label (✅ only the command already, no extra debug text)
    color = {
        "Stop": (0, 0, 255),
        "Forward": (0, 255, 0),
        "Backward": (255, 255, 0),
        "Rotate_Right": (255, 0, 255),
        "Rotate_Left": (0, 255, 255),
        "Center": (100, 100, 255)
    }.get(current_display, (0, 255, 255))

    cv2.putText(frame, current_display, (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 1, color, 2)
    cv2.imshow("Hand + Serial", frame)

    # Check if window was closed with X button or 'q' key pressed
    key = cv2.waitKey(1) & 0xFF
    if key == ord('q'):
        break

    # Check if window was closed by clicking X button
    if cv2.getWindowProperty("Hand + Serial", cv2.WND_PROP_VISIBLE) < 1:
        break

# Cleanup
cap.release()
if ser and ser.is_open:
    ser.close()
    print("Serial port closed")
cv2.destroyAllWindows()