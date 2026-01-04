#include <Wire.h>
#include <MPU9250_asukiaaa.h>

MPU9250_asukiaaa mpu;

void setup() {
  Serial.begin(115200);
  Wire.begin();           // A4 = SDA, A5 = SCL
  delay(2000);

  Serial.println("Init MPU9250...");

  mpu.setWire(&Wire);
  mpu.beginAccel();
  mpu.beginGyro();

  Serial.println("MPU9250 OK");
}

void loop() {
  mpu.accelUpdate();
  mpu.gyroUpdate();

  Serial.print("ACC g  X:");
  Serial.print(mpu.accelX(), 2);
  Serial.print(" Y:");
  Serial.print(mpu.accelY(), 2);
  Serial.print(" Z:");
  Serial.println(mpu.accelZ(), 2);

  Serial.print("GYRO dps X:");
  Serial.print(mpu.gyroX(), 2);
  Serial.print(" Y:");
  Serial.print(mpu.gyroY(), 2);
  Serial.print(" Z:");
  Serial.println(mpu.gyroZ(), 2);

  Serial.println("------");
  delay(500);
}

