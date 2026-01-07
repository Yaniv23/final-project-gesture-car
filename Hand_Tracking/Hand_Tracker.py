import cv2
import mediapipe as mp
import math
import serial
import serial.tools.list_ports
import time
from constant import COM_PORT, BAUD_RATE

mp_hands = mp.solutions.hands
hands = None
mp_draw = mp.solutions.drawing_utils

cap = None
ser = None

COMMAND_MAP = {
    "STOP": 0x00,
    "FORWARD": 0x01,
    "BACKWARD": 0x02,
    "SIDEWAY_LEFT": 0x03,
    "SIDEWAY_RIGHT": 0x04,
    "ROTATE_CW": 0x05,
    "ROTATE_CCW": 0x06,
    "DIAGONAL_315": 0x07,
    "DIAGONAL_45": 0x08,
    "DIAGONAL_225": 0x09,
    "DIAGONAL_135": 0x0A,
    "PIVOT_LEFT": 0x0B,
    "PIVOT_RIGHT": 0x0C,
    "CENTER": 0x00,
}

def find_available_ports():
    """Find all available COM ports"""
    ports = serial.tools.list_ports.comports()
    available = []
    for port in ports:
        available.append(port.device)
    return available

try:
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0.1)
    time.sleep(2)
except serial.SerialException:
    pass

last_detected = ""
current_display = "STOP"
stable_counter = 0
stable_threshold = 10

def get_direction_label(angle_deg):
    if -22.5 < angle_deg <= 22.5:
        return "SIDEWAY_RIGHT"
    elif 22.5 < angle_deg <= 67.5:
        return "DIAGONAL_45"
    elif 67.5 < angle_deg <= 112.5:
        return "FORWARD"
    elif 112.5 < angle_deg <= 157.5:
        return "DIAGONAL_135"
    elif 157.5 < angle_deg or angle_deg <= -157.5:
        return "SIDEWAY_LEFT"
    elif -157.5 < angle_deg <= -112.5:
        return "DIAGONAL_225"
    elif -112.5 < angle_deg <= -67.5:
        return "BACKWARD"
    elif -67.5 < angle_deg <= -22.5:
        return "DIAGONAL_315"
    else:
        return "CENTER"

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

def is_hand_open(landmarks):
    return (
        landmarks[8].y < landmarks[6].y and
        landmarks[12].y < landmarks[10].y and
        landmarks[16].y < landmarks[14].y and
        landmarks[20].y < landmarks[18].y
    )

def is_circle_ccw(landmarks, w, h):
    x1, y1 = int(landmarks[4].x * w), int(landmarks[4].y * h)
    x2, y2 = int(landmarks[8].x * w), int(landmarks[8].y * h)
    distance = math.hypot(x2 - x1, y2 - y1)
    return distance < 40 and x2 > x1

def is_circle_cw(landmarks, w, h):
    x1, y1 = int(landmarks[4].x * w), int(landmarks[4].y * h)
    x2, y2 = int(landmarks[8].x * w), int(landmarks[8].y * h)
    distance = math.hypot(x2 - x1, y2 - y1)
    return distance < 40 and x2 < x1

def main():
    """Main function for hand tracking - can be called from other modules"""
    global cap, ser, last_detected, current_display, stable_counter, hands
    
    if hands is None:
        hands = mp_hands.Hands()
    
    if cap is None or not cap.isOpened():
        cap = cv2.VideoCapture(0)
        if not cap.isOpened():
            print("[ERROR] Could not open webcam!")
            return
    
    window_name = "Hand + Serial"
    
    try:
        cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(window_name, 640, 480)
    except Exception as e:
        print(f"[Hand_Tracker] Error creating window: {e}")
        import traceback
        traceback.print_exc()
        return
    
    frame_count = 0
    iteration_count = 0
    last_send_time = 0
    send_interval = 0.2
    while True:
        iteration_count += 1
        ret, frame = cap.read()
        if not ret:
            frame_count += 1
            if frame_count > 10:
                print("[Hand_Tracker] Too many failed frame reads, exiting")
                break
            continue
        
        frame_count = 0
            
        frame = cv2.flip(frame, 1)
        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results = hands.process(frame_rgb)

        h, w, _ = frame.shape
        cx, cy = w // 2, h // 2
        center_threshold = 60

        cv2.line(frame, (cx, 0), (cx, h), (200, 200, 200), 1)
        cv2.line(frame, (0, cy), (w, cy), (180, 180, 180), 2)
        cv2.line(frame, (0, 0), (w, h), (180, 180, 180), 2)
        cv2.line(frame, (w, 0), (0, h), (180, 180, 180), 2)
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

            if is_hand_closed(landmarks):
                detected_label = "STOP"
            elif is_circle_ccw(landmarks, w, h):
                detected_label = "ROTATE_CCW"
            elif is_circle_cw(landmarks, w, h):
                detected_label = "ROTATE_CW"    
            elif is_hand_open(landmarks) and distance_to_center < center_threshold:
                detected_label = "CENTER"
            else:
                detected_label = get_direction_label(angle_deg)

            mp_draw.draw_landmarks(frame, hand_landmarks, mp_hands.HAND_CONNECTIONS)
            cv2.circle(frame, (hx, hy), 10, (0, 255, 0), -1)
            cv2.line(frame, (cx, cy), (hx, hy), (255, 0, 0), 2)
        else:
            detected_label = "STOP"

        if detected_label == last_detected:
            stable_counter += 1
        else:
            stable_counter = 0
            last_detected = detected_label

        if stable_counter >= stable_threshold:
            if current_display != detected_label:
                current_display = detected_label

        current_time = time.time()
        if current_display and (current_time - last_send_time) >= send_interval:
            if ser and ser.is_open:
                try:
                    cmd_byte = COMMAND_MAP.get(current_display, COMMAND_MAP["STOP"])
                    cmd_str = f"{cmd_byte}\n"
                    ser.write(cmd_str.encode('utf-8'))
                    last_send_time = current_time
                except Exception as e:
                    print("Serial write error:", e)
            else:
                last_send_time = current_time

        if ser and ser.is_open:
            try:
                if ser.in_waiting:
                    line = ser.readline().decode('utf-8').strip()
                    if line:
                        print("ESP32:", line)
            except Exception as e:
                print("Serial read error:", e)
        color = {
            "STOP": (0, 0, 255),
            "FORWARD": (0, 255, 0),
            "BACKWARD": (255, 255, 0),
            "ROTATE_CW": (255, 0, 255),
            "ROTATE_CCW": (0, 255, 255),
            "CENTER": (100, 100, 255),
            "SIDEWAY_LEFT": (255, 165, 0),
            "SIDEWAY_RIGHT": (255, 165, 0),
            "DIAGONAL_45": (0, 255, 255),
            "DIAGONAL_135": (0, 255, 255),
            "DIAGONAL_225": (0, 255, 255),
            "DIAGONAL_315": (0, 255, 255),
            "PIVOT_LEFT": (255, 0, 128),
            "PIVOT_RIGHT": (255, 0, 128)
        }.get(current_display, (0, 255, 255))

        cv2.putText(frame, current_display, (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 1, color, 2)
        
        try:
            cv2.imshow(window_name, frame)
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                break
        except cv2.error as e:
            print(f"[Hand_Tracker] OpenCV error: {e}")
            break
        except Exception as e:
            print(f"[Hand_Tracker] Error displaying frame: {e}")
            import traceback
            traceback.print_exc()
            break
        
        if iteration_count % 30 == 0:
            try:
                if cv2.getWindowProperty(window_name, cv2.WND_PROP_VISIBLE) < 1:
                    break
            except:
                pass

    cap.release()
    if ser and ser.is_open:
        ser.close()
    try:
        cv2.destroyWindow("Hand + Serial")
    except:
        pass


if __name__ == "__main__":
    main()