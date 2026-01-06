#include <esp_now.h>
#include <WiFi.h>

uint8_t receiverMAC[] = {0x00, 0x4B, 0x12, 0x34, 0xF7, 0xF4}; 

typedef struct struct_message {
  uint8_t command;          // single-byte command expected by vehicle
} struct_message;

struct_message outgoingMsg;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }

  esp_now_register_send_cb([](const uint8_t *mac, esp_now_send_status_t status) {
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "✅ Sent" : "❌ Failed");
  });

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Failed to add peer");
    return;
  }

  Serial.println("🟢 Sender ready");
}

void loop() {
  if (Serial.available()) {
    // Read line from serial and convert to uint8_t command
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    // Accept decimal (default) or hex with 0x prefix
    long value = input.startsWith("0x") || input.startsWith("0X")
                   ? strtol(input.c_str(), nullptr, 16)
                   : input.toInt();

    if (value < 0 || value > 255) {
      Serial.println("⚠️  Invalid command (must be 0-255 or 0x00-0xFF)");
      return;
    }

    outgoingMsg.command = static_cast<uint8_t>(value);
    esp_now_send(receiverMAC, reinterpret_cast<uint8_t*>(&outgoingMsg), sizeof(outgoingMsg));

    Serial.print("📤 Sent byte: 0x");
    Serial.print(outgoingMsg.command, HEX);
    Serial.print(" (");
    Serial.print(outgoingMsg.command);
    Serial.println(")");
  }
}
