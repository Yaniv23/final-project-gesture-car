// ESP32: Serial from PC (MediaPipe), Serial1 to Arduino

void setup() {
  Serial.begin(115200);  // USB Serial to PC
  Serial1.begin(9600, SERIAL_8N1, 16, 17); // UART to Arduino (TX = 17, RX = 16)
  Serial.println("ESP32 Bridge Ready");
}

void loop() {
  if (Serial.available()) {
    String gesture = Serial.readStringUntil('\n');
    gesture.trim(); // Remove newline or spaces

    int cmd = decodeGestureToCommand(gesture);
    Serial1.println(cmd); // Send as integer string to Arduino
    Serial.print("Received: ");
    Serial.print(gesture);
    Serial.print(" → Sent to Arduino: ");
    Serial.println(cmd);
  }

  // Optional: Forward Arduino response back to PC
  if (Serial1.available()) {
    String response = Serial1.readStringUntil('\n');
    Serial.println("Arduino: " + response);
  }
}

int decodeGestureToCommand(String gesture) {
  if (gesture == "Stop") return 0;
  if (gesture == "Forward") return 1;
  if (gesture == "Backward") return 2;
  if (gesture == "Sideway_Left") return 3;
  if (gesture == "Sideway_Right") return 4;
  if (gesture == "Up") return 5;                    // pivot_left
  if (gesture == "Down") return 6;                  // pivot_right
  if (gesture == "rotate_cw") return 7;
  if (gesture == "rotate_ccw") return 8;
  if (gesture == "diagonal_forward_left") return 9;
  if (gesture == "diagonal_forward_right") return 10;
  if (gesture == "diagonal_backward_left") return 11;
  if (gesture == "diagonal_backward_right") return 12;
  
  return -1; // Unknown
}
