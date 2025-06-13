#include <Servo.h>

Servo myServo;

// Define HC-SR04 pins
#define TRIG_PIN A0
#define ECHO_PIN A1

void setup() {
  Serial.begin(9600);
  myServo.attach(A2); // Servo signal pin
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void loop() {
  for (int angle = 0; angle <= 60; angle += 5) {
    myServo.write(angle);
    delay(300); // Wait for servo to move

    long distance = readDistanceCM();

    Serial.print("Angle: ");
    Serial.print(angle);
    Serial.print("° → Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    if (distance < 20) {
      Serial.println("⚠️ Object detected close!");
      // Optional: stop motors or trigger alert here
    }
  }

  Serial.println("Finished sweep");
  delay(3000);
}

// Function to measure distance using HC-SR04
long readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  long distance = duration * 0.034 / 2;
  return distance; // in cm
}
