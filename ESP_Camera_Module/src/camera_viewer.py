import argparse
import cv2


# === CONFIGURATION ===
# Change this to the IP address printed by the ESP32 in the Serial Monitor.
# Example: if Serial shows "IP address: 192.168.1.42", set ESP32_IP = "192.168.1.42"
ESP32_IP = "192.168.0.129"
ESP32_STREAM_URL_TEMPLATE = "http://{ip}/stream"


def parse_args():
    """
    Optional command line argument to override the ESP32 IP:
      python camera_viewer.py --ip 192.168.1.42
    """
    parser = argparse.ArgumentParser(
        description="Simple viewer for ESP32-S3 OV2640 camera stream (OpenCV VideoCapture)"
    )
    parser.add_argument(
        "--ip",
        type=str,
        help="ESP32-S3 IP address (overrides ESP32_IP constant)",
    )
    return parser.parse_args()


def get_stream_url(ip_override: str | None) -> str:
    """
    Build the final stream URL using either:
    - the IP from --ip argument, or
    - the ESP32_IP constant above.
    """
    ip = ip_override if ip_override else ESP32_IP
    return ESP32_STREAM_URL_TEMPLATE.format(ip=ip)


def main():
    args = parse_args()
    stream_url = get_stream_url(args.ip)

    print(f"[INFO] Opening stream with OpenCV VideoCapture: {stream_url}")
    cap = cv2.VideoCapture(stream_url)

    if not cap.isOpened():
        print("[ERROR] Could not open video stream. Test this URL in your browser first:")
        print(f"        {stream_url}")
        return

    window_name = "ESP32 Car Camera"
    print("[INFO] Press 'q' to quit, or close the window.")

    while True:
        ret, frame = cap.read()
        if not ret or frame is None:
            # If we don't get a frame, wait a bit and try again.
            key = cv2.waitKey(30) & 0xFF
            if key == ord("q"):
                break
            continue

        # Flip the image horizontally (mirror effect)
        frame = cv2.flip(frame, 0)

        cv2.imshow(window_name, frame)

        key = cv2.waitKey(1) & 0xFF
        if key == ord("q"):
            break

        # If window is closed with the X button, stop the loop
        if cv2.getWindowProperty(window_name, cv2.WND_PROP_VISIBLE) < 1:
            break

    print("\n[INFO] Closing viewer...")
    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()


