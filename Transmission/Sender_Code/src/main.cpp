#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

uint8_t receiverMAC[] = {0x00, 0x4B, 0x12, 0x34, 0xF7, 0xF4}; 

typedef struct struct_message {
  uint8_t command;   // command byte
  uint8_t param;     // optional parameter/speed byte
} struct_message;

struct_message outgoingMsg;

// Connection status flag (set when first successful send occurs)
volatile bool peer_connected = false;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }

  esp_now_register_send_cb([](const uint8_t *mac, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
      peer_connected = true;
      Serial.println("✅ Sent");
    } else {
      Serial.println("❌ Failed");
    }
  });

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Failed to add peer");
    return;
  }

  // Reset connection flag
  peer_connected = false;
  
  const uint32_t timeout_ms = 60000;  // 60 seconds
  const uint32_t heartbeat_interval_ms = 200;  // Send every 200ms
  uint32_t start_time = millis();
  uint32_t last_status_time = start_time;
  const uint32_t status_interval_ms = 2000;  // Print status every 2 seconds
  
  outgoingMsg.command = 0x00;  // CMD_STOP - safe heartbeat command
  outgoingMsg.param = 0x00;
  
  while (!peer_connected && (millis() - start_time < timeout_ms)) {
    // Send heartbeat message
    esp_now_send(receiverMAC, reinterpret_cast<uint8_t*>(&outgoingMsg), sizeof(outgoingMsg));
    
    // Print status updates periodically
    uint32_t current_time = millis();
    if (current_time - last_status_time >= status_interval_ms) {
      uint32_t elapsed = current_time - start_time;
      uint32_t remaining = (timeout_ms > elapsed) ? (timeout_ms - elapsed) : 0;
      Serial.print("📡 Waiting for connection... (");
      Serial.print(elapsed / 1000);
      Serial.print("s elapsed, ");
      Serial.print(remaining / 1000);
      Serial.println("s remaining)");
      last_status_time = current_time;
    }
    
    delay(heartbeat_interval_ms);
  }
  
  if (peer_connected) {
    Serial.println("🟢 Sender ready - connection established!");
  } else {
    Serial.println("❌ Connection timeout - failed to establish connection");
    Serial.println("   Please check receiver is powered on and MAC address is correct");
  }
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

