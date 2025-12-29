import cv2
import mediapipe as mp
import math
import serial
import serial.tools.list_ports
import time
from constant import COM_PORT, BAUD_RATE

# === SETUP ===
mp_hands = mp.solutions.hands
hands = mp_hands.Hands(
    static_image_mode=False,
    max_num_hands=2,
    model_complexity=1,
    min_detection_confidence=0.6,
    min_tracking_confidence=0.6
)
mp_draw = mp.solutions.drawing_utils
cap = cv2.VideoCapture(0)

# Serial (optional)
ser = None

def find_available_ports():
    ports = serial.tools.list_ports.comports()
    return [p.device for p in ports]

def send_stop_safe():
    """Best-effort STOP (for shutdown/crash cases)."""
    global ser
    if ser and ser.is_open:
        try:
            # Send a few times to improve chance of arrival
            for _ in range(3):
                ser.write(b"Stop\n")
                time.sleep(0.03)
            ser.flush()
        except Exception:
            pass

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

# =========================
# Stability filtering (FASTER)
# =========================
last_detected = ""
current_display = ""
stable_counter = 0

stable_threshold = 5   # was 10 -> faster response
stop_threshold = 1     # Stop almost immediate (safety)

# =========================
# Hand lock by proximity (STRONG LOCK)
# =========================
locked_hand = False
locked_center = None
missing_locked_frames = 0
release_after_missing = 8   # faster unlock if the locked hand disappears

max_jump_distance = 200     # if closest hand is farther than this -> treat as missing
far_frames = 0
release_after_far = 6

# Optional smoothing (helps avoid jitter near thresholds)
use_smoothing = True
alpha = 0.45          # higher -> more responsive (was 0.35)

smooth_center = None

# =========================
# Gesture helpers
# =========================
def is_hand_closed(landmarks):
    fingers = {"index": (8, 6), "middle": (12, 10), "ring": (16, 14), "pinky": (20, 18)}
    bent = 0
    for tip_idx, pip_idx in fingers.values():
        if landmarks[tip_idx].y > landmarks[pip_idx].y:
            bent += 1
    return bent == 4

def is_one_finger_up(landmarks):
    return (
        landmarks[8].y < landmarks[6].y and
        landmarks[12].y > landmarks[10].y and
        landmarks[16].y > landmarks[14].y and
        landmarks[20].y > landmarks[18].y
    )

def is_two_fingers_up(landmarks):
    return (
        landmarks[8].y < landmarks[6].y and
        landmarks[12].y < landmarks[10].y and
        landmarks[16].y > landmarks[14].y and
        landmarks[20].y > landmarks[18].y
    )

def get_direction_label_from_position(dx, dy, center_threshold):
    dist = math.hypot(dx, dy)
    if dist < center_threshold:
        return "Center"

    angle = math.degrees(math.atan2(dy, dx))
    if -22.5 < angle <= 22.5:
        return "Sideway_Right"
    elif 22.5 < angle <= 67.5:
        return "diagonal_forward_right"
    elif 67.5 < angle <= 112.5:
        return "Forward"
    elif 112.5 < angle <= 157.5:
        return "diagonal_forward_left"
    elif angle > 157.5 or angle <= -157.5:
        return "Sideway_Left"
    elif -157.5 < angle <= -112.5:
        return "diagonal_backward_left"
    elif -112.5 < angle <= -67.5:
        return "Backward"
    elif -67.5 < angle <= -22.5:
        return "diagonal_backward_right"
    return "Center"

def hand_center_from_landmarks(hand_landmarks, w, h):
    lms = hand_landmarks.landmark
    x_avg = sum(lm.x for lm in lms) / 21.0
    y_avg = sum(lm.y for lm in lms) / 21.0
    return int(x_avg * w), int(y_avg * h)

def pick_locked_hand_index(results, w, h):
    global locked_hand, locked_center, missing_locked_frames, far_frames, smooth_center

    if not results.multi_hand_landmarks:
        missing_locked_frames += 1
        if missing_locked_frames >= release_after_missing:
            locked_hand = False
            locked_center = None
            smooth_center = None
            far_frames = 0
        return None

    missing_locked_frames = 0

    centers = [hand_center_from_landmarks(hlm, w, h) for hlm in results.multi_hand_landmarks]

    # Lock immediately when first hand appears
    if not locked_hand or locked_center is None:
        locked_hand = True
        locked_center = centers[0]
        smooth_center = centers[0]
        far_frames = 0
        return 0

    # Choose the hand closest to the locked center
    best_i = 0
    best_d = float("inf")
    for i, (x, y) in enumerate(centers):
        d = math.hypot(x - locked_center[0], y - locked_center[1])
        if d < best_d:
            best_d = d
            best_i = i

    # If jump is too large, treat as missing (don’t “switch hands”)
    if best_d > max_jump_distance:
        far_frames += 1
        if far_frames >= release_after_far:
            locked_hand = False
            locked_center = None
            smooth_center = None
            far_frames = 0
        return None

    far_frames = 0

    chosen_center = centers[best_i]
    if use_smoothing and smooth_center is not None:
        sx, sy = smooth_center
        nx = int(alpha * chosen_center[0] + (1 - alpha) * sx)
        ny = int(alpha * chosen_center[1] + (1 - alpha) * sy)
        smooth_center = (nx, ny)
        locked_center = (nx, ny)
    else:
        locked_center = chosen_center
        smooth_center = chosen_center

    return best_i

def send_if_needed(cmd):
    """Do not send Center. Send only on change (handled outside)."""
    if cmd == "Center":
        return
    if ser and ser.is_open:
        try:
            ser.write((cmd + "\n").encode("utf-8"))
            # print("Sent:", cmd)
        except Exception as e:
            print("Serial write error:", e)
    else:
        print("Detected (no serial):", cmd)

# =========================
# MAIN LOOP (with crash-safe STOP)
# =========================
try:
    while True:
        ret, frame = cap.read()
        if not ret:
            break

        frame = cv2.flip(frame, 1)
        # Ensure square image for MediaPipe ROI conversion (avoids M_RECT error)
        h0, w0, _ = frame.shape
        if w0 != h0:
            size = max(w0, h0)
            pad_left = (size - w0) // 2
            pad_right = size - w0 - pad_left
            pad_top = (size - h0) // 2
            pad_bottom = size - h0 - pad_top
            frame = cv2.copyMakeBorder(frame, pad_top, pad_bottom, pad_left, pad_right,
                                       cv2.BORDER_CONSTANT, value=(0, 0, 0))
        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results = hands.process(frame_rgb)

        h, w, _ = frame.shape
        cx, cy = w // 2, h // 2
        center_threshold = 60

        # Draw axes + diagonals + center zone
        cv2.line(frame, (cx, 0), (cx, h), (200, 200, 200), 1)
        cv2.line(frame, (0, cy), (w, cy), (200, 200, 200), 1)
        cv2.line(frame, (0, 0), (w, h), (180, 180, 180), 1)
        cv2.line(frame, (w, 0), (0, h), (180, 180, 180), 1)
        cv2.circle(frame, (cx, cy), center_threshold, (100, 100, 255), 1)

        detected_label = ""

        hand_idx = pick_locked_hand_index(results, w, h)

        if hand_idx is not None and results.multi_hand_landmarks:
            hand_landmarks = results.multi_hand_landmarks[hand_idx]
            landmarks = hand_landmarks.landmark

            hx, hy = locked_center if locked_center is not None else hand_center_from_landmarks(hand_landmarks, w, h)
            dx = hx - cx
            dy = cy - hy

            # === PRIORITY TREE ===
            if is_hand_closed(landmarks):
                detected_label = "Stop"
            elif is_two_fingers_up(landmarks):
                detected_label = "rotate_ccw"
            elif is_one_finger_up(landmarks):
                detected_label = "rotate_cw"
            else:
                detected_label = get_direction_label_from_position(dx, dy, center_threshold)

            mp_draw.draw_landmarks(frame, hand_landmarks, mp_hands.HAND_CONNECTIONS)
            cv2.circle(frame, (hx, hy), 10, (0, 255, 0), -1)
            cv2.line(frame, (cx, cy), (hx, hy), (255, 0, 0), 2)
        else:
            detected_label = "Stop"

        # Stability filter (Stop faster)
        threshold = stop_threshold if detected_label == "Stop" else stable_threshold

        if detected_label == last_detected:
            stable_counter += 1
        else:
            stable_counter = 0
            last_detected = detected_label

        if stable_counter >= threshold:
            if current_display != detected_label:
                current_display = detected_label
                send_if_needed(current_display)

        # Display label
        color = {
            "Stop": (0, 0, 255),
            "Forward": (0, 255, 0),
            "Backward": (0, 165, 255),
            "rotate_cw": (255, 0, 255),
            "rotate_ccw": (0, 255, 255),
            "Center": (100, 100, 255)
        }.get(current_display, (0, 255, 255))

        cv2.putText(frame, current_display, (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 1, color, 2)
        cv2.imshow("Hand + Serial", frame)

        key = cv2.waitKey(1) & 0xFF
        if key == ord("q"):
            break
        if cv2.getWindowProperty("Hand + Serial", cv2.WND_PROP_VISIBLE) < 1:
            break

except KeyboardInterrupt:
    print("\n[INFO] KeyboardInterrupt - stopping...")
except Exception as e:
    print("\n[ERROR] Unexpected crash:", e)
finally:
    # IMPORTANT: try to stop the car on any exit path
    send_stop_safe()

    cap.release()
    if ser and ser.is_open:
        try:
            ser.close()
            print("Serial port closed")
        except Exception:
            pass
    cv2.destroyAllWindows()