// ESP32 + 4 Motors + Mecanum Wheels + 2 L298N Drivers
// Mecanum drive control via ESP-NOW

#include <esp_now.h>
#include <WiFi.h>
#include "driver/ledc.h"
#include <Arduino.h>  // Required for ESP32 PWM API (ledc functions)



// Define motor control pins
// L298N #1 (Left side)
#define IN1 27
#define IN2 26
#define ENA 14  // PWM front left

#define IN3 25
#define IN4 33
#define ENB 13  // PWM rear left

// L298N #2 (Right side)
#define IN5 19
#define IN6 18
#define ENA2 32  // PWM front right

#define IN7 17
#define IN8 16
#define ENB2 15  // PWM rear right

// PWM speed (0-255)
int speedVal = 200;

// Command structure
typedef struct struct_message {
  char command[32];
} struct_message;

struct_message incomingMsg;

// Setup motor pins
void setupMotors() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(IN5, OUTPUT); pinMode(IN6, OUTPUT); pinMode(ENA2, OUTPUT);
  pinMode(IN7, OUTPUT); pinMode(IN8, OUTPUT); pinMode(ENB2, OUTPUT);
  stopAll();
}

// Movement functions
void stopAll() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  digitalWrite(IN5, LOW); digitalWrite(IN6, LOW);
  digitalWrite(IN7, LOW); digitalWrite(IN8, LOW);
  ledcWrite(0, 0); ledcWrite(1, 0);
  ledcWrite(2, 0); ledcWrite(3, 0);
}

void moveForward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  // FL
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  // RL
  digitalWrite(IN5, HIGH); digitalWrite(IN6, LOW);  // FR
  digitalWrite(IN7, HIGH); digitalWrite(IN8, LOW);  // RR
  setAllSpeed(speedVal);
}

void moveBackward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  digitalWrite(IN5, LOW); digitalWrite(IN6, HIGH);
  digitalWrite(IN7, LOW); digitalWrite(IN8, HIGH);
  setAllSpeed(speedVal);
}

void slideLeft() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);  // FL ←
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  // RL →
  digitalWrite(IN5, HIGH); digitalWrite(IN6, LOW);  // FR →
  digitalWrite(IN7, LOW); digitalWrite(IN8, HIGH);  // RR ←
  setAllSpeed(speedVal);
}

void slideRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  digitalWrite(IN5, LOW); digitalWrite(IN6, HIGH);
  digitalWrite(IN7, HIGH); digitalWrite(IN8, LOW);
  setAllSpeed(speedVal);
}

void rotateLeft() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  digitalWrite(IN5, HIGH); digitalWrite(IN6, LOW);
  digitalWrite(IN7, HIGH); digitalWrite(IN8, LOW);
  setAllSpeed(speedVal);
}

void rotateRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  digitalWrite(IN5, LOW); digitalWrite(IN6, HIGH);
  digitalWrite(IN7, LOW); digitalWrite(IN8, HIGH);
  setAllSpeed(speedVal);
}

void setAllSpeed(int val) {
  ledcWrite(0, val); // FL
  ledcWrite(1, val); // RL
  ledcWrite(2, val); // FR
  ledcWrite(3, val); // RR
}

// ESP-NOW receive callback
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  memcpy(&incomingMsg, data, sizeof(incomingMsg));
  String cmd = String(incomingMsg.command);
  cmd.trim();

  if (cmd == "FORWARD") moveForward();
  else if (cmd == "BACKWARD") moveBackward();
  else if (cmd == "LEFT") slideLeft();
  else if (cmd == "RIGHT") slideRight();
  else if (cmd == "ROTATE_LEFT") rotateLeft();
  else if (cmd == "ROTATE_RIGHT") rotateRight();
  else stopAll();
}

void setup() {
  Serial.begin(115200);
  setupMotors();

  WiFi.mode(WIFI_STA);
  esp_now_init();
  esp_now_register_recv_cb(onReceive);

  // PWM setup
  ledcSetup(0, 1000, 8); ledcAttachPin(ENA, 0);   // Front Left
  ledcSetup(1, 1000, 8); ledcAttachPin(ENB, 1);   // Rear Left
  ledcSetup(2, 1000, 8); ledcAttachPin(ENA2, 2);  // Front Right
  ledcSetup(3, 1000, 8); ledcAttachPin(ENB2, 3);  // Rear Right

  Serial.println("🟢 Mecanum bot ready");
}

void loop() {
  // Nothing here
}
