#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

uint8_t receiverMAC[] = {0x00, 0x4B, 0x12, 0x35, 0x44, 0xCC}; 

typedef struct struct_message {
  uint8_t command;   // command byte
  uint8_t param;     // optional parameter/speed byte
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
    // Read line from serial and convert to two bytes: command + param
    String input = Serial.readStringUntil('\n');
    input.trim();

    // Split on whitespace/comma to allow "cmd param"
    int sep = input.indexOf(' ');
    if (sep == -1) sep = input.indexOf(',');

    String cmdStr = (sep == -1) ? input : input.substring(0, sep);
    String paramStr = (sep == -1) ? "" : input.substring(sep + 1);
    cmdStr.trim();
    paramStr.trim();

    auto parseByte = [](const String& s, long fallback) -> long {
      if (s.length() == 0) return fallback;
      return (s.startsWith("0x") || s.startsWith("0X"))
               ? strtol(s.c_str(), nullptr, 16)
               : s.toInt();
    };

    long cmdVal = parseByte(cmdStr, -1);
    long paramVal = parseByte(paramStr, 0);

    if (cmdVal < 0 || cmdVal > 255 || paramVal < 0 || paramVal > 255) {
      Serial.println("⚠️  Invalid bytes (0-255 or 0x00-0xFF). Usage: <cmd> [param]");
      return;
    }

    outgoingMsg.command = static_cast<uint8_t>(cmdVal);
    outgoingMsg.param   = static_cast<uint8_t>(paramVal);

    esp_now_send(receiverMAC, reinterpret_cast<uint8_t*>(&outgoingMsg), sizeof(outgoingMsg));

    Serial.print("📤 Sent bytes: cmd=0x");
    Serial.print(outgoingMsg.command, HEX);
    Serial.print(" (");
    Serial.print(outgoingMsg.command);
    Serial.print("), param=0x");
    Serial.print(outgoingMsg.param, HEX);
    Serial.print(" (");
    Serial.print(outgoingMsg.param);
    Serial.println(")");
  }
}

