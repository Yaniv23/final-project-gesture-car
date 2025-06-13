#include <esp_now.h>
#include <WiFi.h>

// Structure to match the sender's message
typedef struct struct_message {
  char command[32]; // Holds command like "FORWARD"
} struct_message;

struct_message incomingMsg;

// New signature for ESP-IDF v5+
void onReceive(const esp_now_recv_info_t *recvInfo, const uint8_t *data, int len) {
  memcpy(&incomingMsg, data, sizeof(incomingMsg));
  
  // Optional: print sender MAC address
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           recvInfo->src_addr[0], recvInfo->src_addr[1], recvInfo->src_addr[2],
           recvInfo->src_addr[3], recvInfo->src_addr[4], recvInfo->src_addr[5]);
  Serial.print("📡 From MAC: ");
  Serial.println(macStr);

  // Print the command received
  Serial.print("📥 Received command: ");
  Serial.println(incomingMsg.command);

  // Handle commands
  if (strcmp(incomingMsg.command, "FORWARD") == 0) {
    Serial.println("➡️ Moving forward");
    // moveForward();
  } else if (strcmp(incomingMsg.command, "BACKWARD") == 0) {
    Serial.println("⬅️ Moving backward");
    // moveBackward();
  } else if (strcmp(incomingMsg.command, "LEFT") == 0) {
    Serial.println("↪️ Turning left");
    // turnLeft();
  } else if (strcmp(incomingMsg.command, "RIGHT") == 0) {
    Serial.println("↩️ Turning right");
    // turnRight();
  } else if (strcmp(incomingMsg.command, "STOP") == 0) {
    Serial.println("⏹️ Stopping");
    // stopMotors();
  } else {
    Serial.println("❓ Unknown command");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  Serial.println("🔧 ESP32 set to STA mode");

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onReceive);
  Serial.println("🟢 Ready to receive ESP-NOW messages");
}

void loop() {
  // Nothing to do here; everything happens in the onReceive() callback
}
