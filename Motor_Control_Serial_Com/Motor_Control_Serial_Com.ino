#include <Arduino.h>

//==Front Wheel==
#define FrontR_IN1 2
#define FrontR_IN2 4
#define FrontR_ENA 3 //PWM

#define FrontL_IN3 5
#define FrontL_IN4 7    
#define FrontL_ENB 6 //PWM

//==Back Wheel==
#define BackR_IN1 8
#define BackR_IN2 10
#define BackR_ENA 9 //PWM

#define BackL_IN3 13
#define BackL_IN4 12
#define BackL_ENB 11 //PWM

//Define Speed
const int speed_slow = 150; //Slow speed
const int speed_fast = 255; //Fast speed

// Serial command buffer
String incoming = "";

void setup() {
  // Set motor pins as outputs
  pinMode(FrontR_IN1, OUTPUT);
  pinMode(FrontR_IN2, OUTPUT);
  pinMode(FrontR_ENA, OUTPUT);

  pinMode(FrontL_IN3, OUTPUT);
  pinMode(FrontL_IN4, OUTPUT);
  pinMode(FrontL_ENB, OUTPUT);

  pinMode(BackR_IN1, OUTPUT);
  pinMode(BackR_IN2, OUTPUT);
  pinMode(BackR_ENA, OUTPUT);

  pinMode(BackL_IN3, OUTPUT);
  pinMode(BackL_IN4, OUTPUT);
  pinMode(BackL_ENB, OUTPUT);

  Serial.begin(9600);  // Serial to ESP32 (via RX/TX pins)
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      incoming.trim();
      int cmd = incoming.toInt();  // Convert string to integer
      Serial.print("ACK: ");
      Serial.println(cmd);

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
          Serial.println("Unknown command");
      }

      incoming = "";
    } else {
      incoming += c;
    }
  }
}

// === Motion Functions ===
void stop_motors() {
  Serial.println("Stopping all motors");
  digitalWrite(FrontR_IN1, LOW);
  digitalWrite(FrontR_IN2, LOW);
  analogWrite(FrontR_ENA, 0);

  digitalWrite(FrontL_IN3, LOW);
  digitalWrite(FrontL_IN4, LOW);
  analogWrite(FrontL_ENB, 0);

  digitalWrite(BackR_IN1, LOW);
  digitalWrite(BackR_IN2, LOW);
  analogWrite(BackR_ENA, 0);

  digitalWrite(BackL_IN3, LOW);
  digitalWrite(BackL_IN4, LOW);
  analogWrite(BackL_ENB, 0);
}

void Forward() {
  Forward_FR();
  Forward_FL();
  Forward_BR();
  Forward_BL();
}

void Backward() {
  Backward_FR();
  Backward_FL();
  Backward_BR();
  Backward_BL();
}

void Sideway_Right() {
  Backward_FR();
  Forward_FL();
  Forward_BR();
  Backward_BL();
}

void Sideway_Left() {
  Forward_FR();
  Backward_FL();
  Backward_BR();
  Forward_BL();
}

void pivot_left() {
  stop_motors();
  Forward_FR();
  Backward_FL();
}

void pivot_right() {
  stop_motors();
  Backward_FR();
  Forward_FL();
}

void rotate_cw() {
  Backward_FR();
  Forward_FL();
  Backward_BR();
  Forward_BL();
}

void rotate_ccw() {
  Forward_FR();
  Backward_FL();
  Forward_BR();
  Backward_BL();
}

void diagonal_forward_left() {
  stop_motors();
  Forward_FR();
  Forward_BL();
}

void diagonal_forward_right() {
  stop_motors();
  Forward_FL();
  Forward_BR();
}

void diagonal_backward_left() {
  stop_motors();
  Backward_FR();
  Backward_BL();
}

void diagonal_backward_right() {
  stop_motors();
  Backward_FL();
  Backward_BR();
}

// === Individual Wheel Control ===
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
  analogWrite(FrontL_ENB, speed_slow);
}

void Backward_FL() {
  digitalWrite(FrontL_IN3, LOW);
  digitalWrite(FrontL_IN4, HIGH);
  analogWrite(FrontL_ENB, speed_slow);
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
  analogWrite(BackL_ENB, speed_slow);
}

void Backward_BL() {
  digitalWrite(BackL_IN3, LOW);
  digitalWrite(BackL_IN4, HIGH);
  analogWrite(BackL_ENB, speed_slow);
}
