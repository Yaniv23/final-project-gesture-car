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
    # Mode control commands
    "MODE_MANUAL": 0x20,
    "MODE_AUTONOMOUS": 0x21,
    "MODE_TOGGLE": 0x22,
}

def find_available_ports():
    """Find all available COM ports"""
    ports = serial.tools.list_ports.comports()
    available = []
    for port in ports:
        available.append(port.device)
    return available

def connect_serial(port=None, baud=115200, retries=3):
    """Connect to serial port with retry logic and error reporting"""
    target_port = COM_PORT
    available_ports = find_available_ports()
    
    if not available_ports:
        print("[ERROR] No serial ports found!")
        return None
    
    if target_port not in available_ports:
        target_port = available_ports[0]
    
    for attempt in range(retries):
        try:
            connection = serial.Serial(target_port, baud, timeout=0.1)
            time.sleep(2)
            if connection.is_open:
                return connection
        except serial.SerialException as e:
            if attempt < retries - 1:
                time.sleep(1)
        except Exception as e:
            if attempt < retries - 1:
                time.sleep(1)
    
    return None

ser = connect_serial(COM_PORT, BAUD_RATE)

last_detected = ""
current_display = "STOP"
stable_counter = 0
stable_threshold = 3
stable_threshold_stop = 2

# Mode de conduite (autonome par défaut)
driving_mode = "AUTONOMOUS"  # Mode par défaut
mode_toggle_cooldown = 0.0
mode_toggle_cooldown_duration = 1.0  # 1 seconde entre les changements de mode
last_mode_sent = None
mode_toggle_stable_counter = 0
mode_toggle_stable_threshold = 15  # Nombre de frames consécutives avec 3 doigts levés requis (environ 0.5 seconde à 30 FPS)

tracked_hand_pos = None
tracking_active = False
no_hand_timeout = 0.0
hand_tracking_timeout = 1.0
hand_distance_threshold = 150

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
        return "STOP"

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

def count_raised_fingers(landmarks):
    """Count how many fingers (excluding thumb) are raised"""
    fingers = {
        "index": (8, 6),
        "middle": (12, 10),
        "ring": (16, 14),
        "pinky": (20, 18),
    }
    raised_count = 0
    for tip_idx, pip_idx in fingers.values():
        if landmarks[tip_idx].y < landmarks[pip_idx].y:
            raised_count += 1
    return raised_count

def main():
    """Main function for hand tracking - can be called from other modules"""
    global cap, ser, last_detected, current_display, stable_counter, hands
    global tracked_hand_pos, tracking_active, no_hand_timeout
    global driving_mode, mode_toggle_cooldown, last_mode_sent, mode_toggle_stable_counter
    
    if ser is None or not ser.is_open:
        ser = connect_serial(COM_PORT, BAUD_RATE)
    
    if hands is None:
        hands = mp_hands.Hands()
    
    if cap is None or not cap.isOpened():
        cap = cv2.VideoCapture(0)
        if not cap.isOpened():
            print("[ERROR] Could not open webcam!")
            return
    
    window_name = "HandTracking + Serial"
    
    try:
        cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(window_name, 640, 480)
    except Exception:
        return
    
    frame_count = 0
    iteration_count = 0
    last_send_time = 0
    last_sent_command = None
    send_interval = 0.1
    while True:
        iteration_count += 1
        current_time = time.time()
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

        cv2.line(frame, (cx, 0), (cx, h), (200, 200, 200), 2)
        cv2.line(frame, (0, cy), (w, cy), (200, 200, 200), 2)
        cv2.line(frame, (0, 0), (w, h), (200, 200, 200), 2)
        cv2.line(frame, (w, 0), (0, h), (200, 200, 200), 2)
        cv2.circle(frame, (cx, cy), center_threshold, (100, 100, 255), 2)

        detected_label = ""

        if results.multi_hand_landmarks:
            selected_hand_landmarks = None
            
            if tracking_active and tracked_hand_pos is not None:
                min_distance = float('inf')
                for hand_landmarks in results.multi_hand_landmarks:
                    wrist = hand_landmarks.landmark[0]
                    wrist_x = int(wrist.x * w)
                    wrist_y = int(wrist.y * h)
                    distance = math.hypot(wrist_x - tracked_hand_pos[0], wrist_y - tracked_hand_pos[1])
                    
                    if distance < min_distance:
                        min_distance = distance
                        selected_hand_landmarks = hand_landmarks
                
                if min_distance > hand_distance_threshold:
                    tracking_active = False
                    tracked_hand_pos = None
                    selected_hand_landmarks = results.multi_hand_landmarks[0]
            else:
                selected_hand_landmarks = results.multi_hand_landmarks[0]
                tracking_active = True
            
            if selected_hand_landmarks:
                wrist = selected_hand_landmarks.landmark[0]
                tracked_hand_pos = (int(wrist.x * w), int(wrist.y * h))
                no_hand_timeout = current_time
            
            hand_landmarks = selected_hand_landmarks
            landmarks = hand_landmarks.landmark

            x_avg = sum(lm.x for lm in landmarks) / 21
            y_avg = sum(lm.y for lm in landmarks) / 21
            hx, hy = int(x_avg * w), int(y_avg * h)

            dx = hx - cx
            dy = cy - hy
            angle_deg = math.degrees(math.atan2(dy, dx))

            if is_hand_closed(landmarks):
                detected_label = "STOP"
            else:
                raised_fingers = count_raised_fingers(landmarks)
                # Détection de 3 doigts levés pour changer de mode
                if raised_fingers == 3:
                    # Incrémenter le compteur de stabilité pour le changement de mode
                    mode_toggle_stable_counter += 1
                    
                    # Vérifier si on a maintenu 3 doigts levés assez longtemps
                    if mode_toggle_stable_counter >= mode_toggle_stable_threshold:
                        # Vérifier le cooldown pour éviter les changements trop rapides
                        if current_time - mode_toggle_cooldown >= mode_toggle_cooldown_duration:
                            # Basculer entre AUTONOMOUS et MANUAL
                            if driving_mode == "AUTONOMOUS":
                                driving_mode = "MANUAL"
                                mode_cmd = COMMAND_MAP["MODE_MANUAL"]
                            else:
                                driving_mode = "AUTONOMOUS"
                                mode_cmd = COMMAND_MAP["MODE_AUTONOMOUS"]
                            
                            # Envoyer la commande de mode
                            if ser and ser.is_open:
                                try:
                                    cmd_str = f"{mode_cmd}\n"
                                    ser.write(cmd_str.encode('utf-8'))
                                    ser.flush()
                                    last_mode_sent = driving_mode
                                    print(f"[MODE] Changement de mode: {driving_mode}")
                                except serial.SerialException:
                                    ser = connect_serial(COM_PORT, BAUD_RATE)
                                except Exception:
                                    pass
                            
                            mode_toggle_cooldown = current_time
                            mode_toggle_stable_counter = 0  # Réinitialiser après changement
                            detected_label = "STOP"  # Ne pas bouger pendant le changement de mode
                        else:
                            # Pendant le cooldown, réinitialiser le compteur et traiter comme mouvement normal
                            mode_toggle_stable_counter = 0
                            detected_label = get_direction_label(angle_deg)
                    else:
                        # Pas encore assez de frames, continuer à compter mais ne pas changer de mode
                        detected_label = "STOP"  # Arrêter le mouvement pendant la détection
                else:
                    # Réinitialiser le compteur si on ne détecte plus 3 doigts
                    mode_toggle_stable_counter = 0
                    if raised_fingers == 1:
                        detected_label = "ROTATE_CW"
                    elif raised_fingers == 2:
                        detected_label = "ROTATE_CCW"
                    else:
                        detected_label = get_direction_label(angle_deg)

            mp_draw.draw_landmarks(frame, hand_landmarks, mp_hands.HAND_CONNECTIONS)
            cv2.circle(frame, (hx, hy), 10, (0, 255, 0), -1)
            cv2.line(frame, (cx, cy), (hx, hy), (255, 0, 0), 2)
        else:
            detected_label = "STOP"
            if tracking_active:
                if no_hand_timeout == 0:
                    no_hand_timeout = current_time
                elif current_time - no_hand_timeout > hand_tracking_timeout:
                    tracking_active = False
                    tracked_hand_pos = None
                    no_hand_timeout = 0

        if detected_label == last_detected:
            stable_counter += 1
        else:
            stable_counter = 0
            last_detected = detected_label

        threshold = stable_threshold_stop if detected_label == "STOP" else stable_threshold
        
        if stable_counter >= threshold:
            if current_display != detected_label:
                current_display = detected_label

        # Envoyer le mode initial au démarrage ou si le mode a changé
        if last_mode_sent != driving_mode:
            if ser and ser.is_open:
                try:
                    mode_cmd = COMMAND_MAP["MODE_AUTONOMOUS"] if driving_mode == "AUTONOMOUS" else COMMAND_MAP["MODE_MANUAL"]
                    cmd_str = f"{mode_cmd}\n"
                    ser.write(cmd_str.encode('utf-8'))
                    ser.flush()
                    last_mode_sent = driving_mode
                    print(f"[MODE] Mode envoyé: {driving_mode}")
                except serial.SerialException:
                    ser = connect_serial(COM_PORT, BAUD_RATE)
                except Exception:
                    pass
        
        if current_display and (current_time - last_send_time) >= send_interval:
            if last_sent_command != current_display:
                if ser and ser.is_open:
                    try:
                        cmd_byte = COMMAND_MAP.get(current_display, COMMAND_MAP["STOP"])
                        cmd_str = f"{cmd_byte}\n"
                        ser.write(cmd_str.encode('utf-8'))
                        ser.flush()
                        last_sent_command = current_display
                    except serial.SerialException:
                        ser = connect_serial(COM_PORT, BAUD_RATE)
                    except Exception:
                        pass
                else:
                    last_sent_command = current_display
            last_send_time = current_time                
        if ser and ser.is_open:
            try:
                if ser.in_waiting:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        print(f"[ESP32] {line}")
            except serial.SerialException:
                ser = connect_serial(COM_PORT, BAUD_RATE)
            except Exception:
                pass
        color = {
            "STOP": (0, 0, 255),
            "FORWARD": (0, 255, 0),
            "BACKWARD": (255, 255, 0),
            "ROTATE_CW": (255, 0, 255),
            "ROTATE_CCW": (0, 255, 255),
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
        
        # Afficher le mode de conduite en haut à droite
        mode_text = f"Mode: {driving_mode}"
        mode_text_size = cv2.getTextSize(mode_text, cv2.FONT_HERSHEY_SIMPLEX, 0.7, 2)[0]
        mode_x = w - mode_text_size[0] - 10
        mode_y = 30
        mode_color = (0, 255, 0) if driving_mode == "AUTONOMOUS" else (0, 165, 255)
        cv2.putText(frame, mode_text, (mode_x, mode_y), cv2.FONT_HERSHEY_SIMPLEX, 0.7, mode_color, 2)
        
        try:
            cv2.imshow(window_name, frame)
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                break
        except cv2.error:
            break
        except Exception:
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
        cv2.destroyWindow(window_name)
    except:
        pass


if __name__ == "__main__":
    main()