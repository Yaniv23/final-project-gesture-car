#include <Arduino.h>

int motorpin3 = 2;
int motorpin4 = 4;


void setup() {
  // put your setup code here, to run once:
  pinMode(motorpin3, OUTPUT);
  pinMode(motorpin4,OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(motorpin3, HIGH);
  digitalWrite(motorpin4, LOW);
  Serial.print("FORWARD");
  delay(5000);
   digitalWrite(motorpin3, LOW);
  digitalWrite(motorpin4, LOW);
  Serial.print("STOP");
  delay(1000);
  digitalWrite(motorpin3, LOW);
  digitalWrite(motorpin4, HIGH);
  Serial.print("BACKWARD");
  delay(5000);
   digitalWrite(motorpin3, LOW);
  digitalWrite(motorpin4, LOW);
  Serial.print("STOP");
  delay(1000);
}
