#include <Arduino.h>

// === Pin Definitions ===
const int pot_pin = 32;     // Gas potentiometer (center = stop)
const int joy_pinX = 34;    // Joystick X-axis for steering
const int joy_swBtn = 25;   // Joystick button (active LOW)

// === Neutral Reference Values ===
const int GAS_TOLERANCE = 10; // Dead zone around 0 for stopping
const int ANGLE_ZERO_RAW = 2700; // Calibrated analog value at center (adjusted)
const int ANGLE_TOLERANCE = 50;  // Raw tolerance

// === Struct for message (simulated) ===
typedef struct struct_message {
  char type[16];   // "forward", "backward", "stop"
  int power;       // -100 to 100 (signed)
  int angle;       // -100 to 100 (signed)
  bool button;     // true if joystick button is pressed
} struct_message;

struct_message outgoingMsg;

void setup() {
  Serial.begin(115200);
  pinMode(pot_pin, INPUT);
  pinMode(joy_pinX, INPUT);
  pinMode(joy_swBtn, INPUT_PULLUP);
}

void loop() {
  int steerRaw = analogRead(joy_pinX);          // 0–4095
  int gasRaw = analogRead(pot_pin);             // 0–4095
  bool buttonPressed = digitalRead(joy_swBtn) == LOW;

  // === Gas Mapping ===
  int gas = map(gasRaw, 0, 4095, -100, 100);    // -100 = full back, 0 = stop, 100 = full forward
  String type = "stop";
  int power = 0;

  if (gas > GAS_TOLERANCE) {
    type = "forward";
    power = gas;
  } else if (gas < -GAS_TOLERANCE) {
    type = "backward";
    power = gas;
  } else {
    power = 0;
  }

  // === Steering Mapping centered at ANGLE_ZERO_RAW ===
  int steer = 0;
  int delta = steerRaw - ANGLE_ZERO_RAW;

  if (abs(delta) > ANGLE_TOLERANCE) {
    steer = map(delta, -ANGLE_ZERO_RAW, 4095 - ANGLE_ZERO_RAW, -100, 100);
    steer = constrain(steer, -100, 100);
  }

  // === Fill and Print Struct ===
  type.toCharArray(outgoingMsg.type, sizeof(outgoingMsg.type));
  outgoingMsg.power = power;     // Signed power (-100 to 100)
  outgoingMsg.angle = steer;     // Signed angle (-100 to 100)
  outgoingMsg.button = buttonPressed;

  Serial.print("Type: ");
  Serial.print(outgoingMsg.type);
  Serial.print(" | Power: ");
  Serial.print(outgoingMsg.power);
  Serial.print(" | Angle: ");
  Serial.print(outgoingMsg.angle);
  Serial.print(" | Btn: ");
  Serial.println(outgoingMsg.button ? "PRESSED" : "RELEASED");

  delay(200);
}
