// GestureCar_ESP32.ino
// TEST MODE: Hardware disabled - only ESP-NOW communication and command parsing
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <esp_wifi.h>

//==Front Wheel==
#define FrontR_IN1 35
#define FrontR_IN2 32
#define FrontR_ENA 34 //PWM  //All the EN pin will be on 1 pin since they are all the same speed

#define FrontL_IN3 33
#define FrontL_IN4 25    
#define FrontL_ENA 34 //PWM  //All the EN pin will be on 1 pin since they are all the same speed

//==Back Wheel==
#define BackR_IN1 27
#define BackR_IN2 14
#define BackR_ENA 26 //PWM

#define BackL_IN3 12
#define BackL_IN4 13
#define BackL_ENA 26 //PWM  //All the EN pin will be on 1 pin since they are all the same speed

// Servo + sensor pins (update to match your wiring)
const int SERVO_PIN = 4;   // PWM-capable pin
const int TRIG_PIN = 18;    // HC-SR04 trigger
const int ECHO_PIN = 16;    // HC-SR04 echo (input only)

//Define Speed
const int speed_slow = 150; //Slow speed
const int speed_fast = 255; //Fast speed

// Servo sweep configuration
const int SERVO_MIN_ANGLE = 0;
const int SERVO_MAX_ANGLE = 60;
const int SERVO_STEP_DEG = 5;
const unsigned long SERVO_STEP_INTERVAL_MS = 300;
const unsigned long SERVO_REST_INTERVAL_MS = 1500;
const int OBSTACLE_DISTANCE_CM = 20;

Servo scanServo;
int currentServoAngle = SERVO_MIN_ANGLE;
unsigned long lastServoStepMs = 0;
unsigned long sweepFinishedMs = 0;
bool sweepResting = false;
long lastDistanceCm = -1;

// ESP-NOW message structure
typedef struct struct_message {
  char command[32]; // Holds command like "FORWARD"
} struct_message;

volatile bool newCommand = false;
volatile int cmd = -1;  // -1 indicates no pending command
struct_message incomingMsg;

// Case-insensitive string comparison helper
bool strEquals(const char* str1, const char* str2) {
  int i = 0;
  while (str1[i] != '\0' && str2[i] != '\0') {
    char c1 = str1[i];
    char c2 = str2[i];
    // Convert to lowercase for comparison
    if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
    if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
    if (c1 != c2) return false;
    i++;
  }
  return str1[i] == '\0' && str2[i] == '\0';
}

int commandToInt(const char* command) {
  // Basic movement commands (case-insensitive)
  if (strEquals(command, "Stop")) return 0;
  if (strEquals(command, "Forward")) return 1;
  if (strEquals(command, "Backward")) return 2;
  
  // Sideway movement
  if (strEquals(command, "Sideway_Left")) return 3;
  if (strEquals(command, "Sideway_Right")) return 4;
  
  // Rotation commands (case-insensitive)
  if (strEquals(command, "rotate_cw")) return 7;
  if (strEquals(command, "rotate_ccw")) return 8;
  
  // Diagonal movement commands
  if (strEquals(command, "diagonal_forward_left")) return 9;
  if (strEquals(command, "diagonal_forward_right")) return 10;
  if (strEquals(command, "diagonal_backward_left")) return 11;
  if (strEquals(command, "diagonal_backward_right")) return 12;
  
  // Additional mappings for direction labels

  if (strEquals(command, "Center")) return 0;  // Map "Center" to Stop
  
  return -1;  // Unknown command
}

void onReceive(const esp_now_recv_info_t *recvInfo, const uint8_t *data, int len) {
  if (len != sizeof(incomingMsg)) {
    Serial.print("Invalid message size. Expected: ");
    Serial.print(sizeof(incomingMsg));
    Serial.print(", got: ");
    Serial.println(len);
    return;
  }

  memcpy((void*)&incomingMsg, data, sizeof(incomingMsg));

  cmd = commandToInt(incomingMsg.command);
  newCommand = true;

  if (cmd == -1) {
    Serial.println("Unknown command - not recognized");
  } else {
    Serial.print("Command mapped to code: ");
    Serial.println(cmd);
  }
}

long readDistanceCM();
void updateServoSensor();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Gesture Car ESP32 - TEST MODE");
  Serial.println("Hardware disabled - testing ESP-NOW only");

  // MOTOR PINS - Initialize motor pins as outputs
  pinMode(FrontR_IN1, OUTPUT);
  pinMode(FrontR_IN2, OUTPUT);
  pinMode(FrontR_ENA, OUTPUT);
  pinMode(FrontL_IN3, OUTPUT);
  pinMode(FrontL_IN4, OUTPUT);
  pinMode(FrontL_ENA, OUTPUT);
  pinMode(BackR_IN1, OUTPUT);
  pinMode(BackR_IN2, OUTPUT);
  pinMode(BackR_ENA, OUTPUT);
  pinMode(BackL_IN3, OUTPUT);
  pinMode(BackL_IN4, OUTPUT);
  pinMode(BackL_ENA, OUTPUT);

  // SERVO + SENSOR DISABLED FOR TESTING
  // Servo + sensor setup
  scanServo.attach(SERVO_PIN);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  lastServoStepMs = millis();
  sweepFinishedMs = millis();
  sweepResting = false;

  // Initialize all motors to stopped state (simulated)
  stop_motors();

  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // Disconnect from any previous connection
  delay(200); // Give WiFi time to initialize
  Serial.println("ESP32 set to STA mode");
  
  // Print MAC address using esp_wifi_get_mac
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  Serial.print("MAC Address: ");
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println(macStr);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onReceive);
  Serial.println("Ready to receive ESP-NOW messages");
  Serial.println("Command parser initialized");
  Serial.println("Waiting for ESP-NOW commands...");
}

void loop() {
  // Check for new command from ESP-NOW
  if (newCommand) {
    newCommand = false; // Reset flag
    
    // Execute motor command based on integer code (simulated)
    switch (cmd) {
      case 0: stop_motors(); break;
      case 1: Forward(); break;
      case 2: Backward(); break;
      case 3: Sideway_Left(); break;
      case 4: Sideway_Right(); break;
      case 5: pivot_left(); break;
      case 6: pivot_right(); break;
      case 7: rotate_cw(); break;
      case 8: rotate_ccw(); break;
      case 9: diagonal_forward_left(); break;
      case 10: diagonal_forward_right(); break;
      case 11: diagonal_backward_left(); break;
      case 12: diagonal_backward_right(); break;
      default:
        Serial.println("Unknown command code");
    }
    cmd = -1; // Reset command
  }
  
  // SERVO/SENSOR DISABLED FOR TESTING
  updateServoSensor();
}

// === Motion Functions (TEST MODE - Print only) ===
void stop_motors() {
  Serial.println("[MOTOR] ⏹️ STOP - All motors stopped");
  Serial.println("        [SIMULATED] FrontR: OFF, FrontL: OFF, BackR: OFF, BackL: OFF");
  digitalWrite(FrontR_IN1, LOW);
  digitalWrite(FrontR_IN2, LOW);
  analogWrite(FrontR_ENA, 0);
  digitalWrite(FrontL_IN3, LOW);
  digitalWrite(FrontL_IN4, LOW);
  // analogWrite(FrontL_ENB, 0);
  digitalWrite(BackR_IN1, LOW);
  digitalWrite(BackR_IN2, LOW);
  analogWrite(BackR_ENA, 0);
  digitalWrite(BackL_IN3, LOW);
  digitalWrite(BackL_IN4, LOW);
  // analogWrite(BackL_ENB, 0);
}

void Forward() {
  Serial.println("[MOTOR] ➡️ FORWARD");
  Serial.println("        [SIMULATED] All wheels: Forward (FR, FL, BR, BL)");
  Forward_FR();
  Forward_FL();
  Forward_BR();
  Forward_BL();
}

void Backward() {
  Serial.println("[MOTOR] ⬅️ BACKWARD");
  Serial.println("        [SIMULATED] All wheels: Backward (FR, FL, BR, BL)");
  Backward_FR();
  Backward_FL();
  Backward_BR();
  Backward_BL();
}

void Sideway_Right() {
  Serial.println("[MOTOR] ➡️ STRAFE RIGHT");
  Serial.println("        [SIMULATED] FR: Backward, FL: Forward, BR: Forward, BL: Backward");
  Backward_FR();
  Forward_FL();
  Forward_BR();
  Backward_BL();
}

void Sideway_Left() {
  Serial.println("[MOTOR] ⬅️ STRAFE LEFT");
  Serial.println("        [SIMULATED] FR: Forward, FL: Backward, BR: Backward, BL: Forward");
  Forward_FR();
  Backward_FL();
  Backward_BR();
  Forward_BL();
}

void pivot_left() {
  Serial.println("[MOTOR] ↪️ PIVOT LEFT");
  Serial.println("        [SIMULATED] FR: Forward, FL: Backward");
  stop_motors();
  Forward_FR();
  Backward_FL();
}

void pivot_right() {
  Serial.println("[MOTOR] ↩️ PIVOT RIGHT");
  Serial.println("        [SIMULATED] FR: Backward, FL: Forward");
  stop_motors();
  Backward_FR();
  Forward_FL();
}

void rotate_cw() {
  Serial.println("[MOTOR] 🔄 ROTATE CLOCKWISE");
  Serial.println("        [SIMULATED] FR: Backward, FL: Forward, BR: Backward, BL: Forward");
  Backward_FR();
  Forward_FL();
  Backward_BR();
  Forward_BL();
}

void rotate_ccw() {
  Serial.println("[MOTOR] 🔄 ROTATE COUNTER-CLOCKWISE");
  Serial.println("        [SIMULATED] FR: Forward, FL: Backward, BR: Forward, BL: Backward");
  Forward_FR();
  Backward_FL();
  Forward_BR();
  Backward_BL();
}

void diagonal_forward_left() {
  Serial.println("[MOTOR] ↗️ DIAGONAL FORWARD LEFT");
  Serial.println("        [SIMULATED] FR: Forward, BL: Forward");
  stop_motors();
  Forward_FR();
  Forward_BL();
}

void diagonal_forward_right() {
  Serial.println("[MOTOR] ↖️ DIAGONAL FORWARD RIGHT");
  Serial.println("        [SIMULATED] FL: Forward, BR: Forward");
  stop_motors();
  Forward_FL();
  Forward_BR();
}

void diagonal_backward_left() {
  Serial.println("[MOTOR] ↘️ DIAGONAL BACKWARD LEFT");
  Serial.println("        [SIMULATED] FR: Backward, BL: Backward");
  stop_motors();
  Backward_FR();
  Backward_BL();
}

void diagonal_backward_right() {
  Serial.println("[MOTOR] ↙️ DIAGONAL BACKWARD RIGHT");
  Serial.println("        [SIMULATED] FL: Backward, BR: Backward");
  stop_motors();
  Backward_FL();
  Backward_BR();
}

// === Individual Wheel Control (DISABLED IN TEST MODE) ===
void Forward_FR() {
    digitalWrite(FrontR_IN1, HIGH);
    digitalWrite(FrontR_IN2, LOW);
    analogWrite(FrontR_ENA, speed_slow);
}

void Backward_FR() {
  digitalWrite(FrontR_IN1, LOW);
  digitalWrite(FrontR_IN2, HIGH);
  analogWrite(FrontR_ENA, speed_slow);
}

void Forward_FL() {
  digitalWrite(FrontL_IN3, HIGH);
  digitalWrite(FrontL_IN4, LOW);
  analogWrite(FrontL_ENA, speed_slow);
}

void Backward_FL() {
  digitalWrite(FrontL_IN3, LOW);
  digitalWrite(FrontL_IN4, HIGH);
  analogWrite(FrontL_ENA, speed_slow);
}

void Forward_BR() {
  digitalWrite(BackR_IN1, HIGH);
  digitalWrite(BackR_IN2, LOW);
  analogWrite(BackR_ENA, speed_slow);
}

void Backward_BR() {
  digitalWrite(BackR_IN1, LOW);
  digitalWrite(BackR_IN2, HIGH);
  analogWrite(BackR_ENA, speed_slow);
}

void Forward_BL() {
  digitalWrite(BackL_IN3, HIGH);
  digitalWrite(BackL_IN4, LOW);
  analogWrite(BackL_ENA, speed_slow);
}

void Backward_BL() {
  digitalWrite(BackL_IN3, LOW);
  digitalWrite(BackL_IN4, HIGH);
  analogWrite(BackL_ENA, speed_slow);
}

// SERVO/SENSOR FUNCTIONS DISABLED FOR TESTING
void updateServoSensor() {
  unsigned long now = millis();

  if (sweepResting) {
    if (now - sweepFinishedMs >= SERVO_REST_INTERVAL_MS) {
      sweepResting = false;
      currentServoAngle = SERVO_MIN_ANGLE;
      lastServoStepMs = 0; // Force immediate update
    }
    return;
  }

  if (now - lastServoStepMs < SERVO_STEP_INTERVAL_MS) {
    return;
  }

  lastServoStepMs = now;
  scanServo.write(currentServoAngle);

  lastDistanceCm = readDistanceCM();
  // Serial.print("[SERVO] Angle: ");
  // Serial.print(currentServoAngle);
  // Serial.print("° | Distance: ");
  // Serial.print(lastDistanceCm);
  // Serial.println(" cm");

  if (lastDistanceCm > 0 && lastDistanceCm < OBSTACLE_DISTANCE_CM) {
    Serial.println("⚠️ Object detected close!");
    // stop_motors();
  }

  if (currentServoAngle >= SERVO_MAX_ANGLE) {
    sweepResting = true;
    sweepFinishedMs = now;
  } else {
    currentServoAngle += SERVO_STEP_DEG;
  }
}

long readDistanceCM() {
  // DISABLED - Returns simulated value for testing
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  long distance = duration * 0.034 / 2;
  return distance; // in cm
}
