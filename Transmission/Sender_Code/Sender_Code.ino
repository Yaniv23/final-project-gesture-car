#include <esp_now.h>
#include <WiFi.h>

uint8_t receiverMAC[] = {0x00, 0x4B, 0x12, 0x34, 0xF7, 0xF4}; 

typedef struct struct_message {
  char command[32];
} struct_message;

struct_message outgoingMsg;

// Temperature message structure (must match vehicle ESP32)
struct temp_message {
  float temperature;  // Temperature in Celsius
  uint8_t state;      // 0=normal, 1=warning, 2=critical
};

// Receive callback for temperature messages
void onReceiveTemp(const uint8_t *mac, const uint8_t *data, int len) {
  // Check if this is a temperature message (5 bytes: 4 bytes float + 1 byte state)
  if (len == sizeof(temp_message)) {
    temp_message temp_msg;
    memcpy(&temp_msg, data, sizeof(temp_msg));
    
    // Forward temperature via Serial
    const char* state_str = (temp_msg.state == 1) ? "WARNING" : "CRITICAL";
    Serial.print("TEMP:");
    Serial.print(temp_msg.temperature, 1);
    Serial.print(":");
    Serial.print(state_str);
    Serial.println();
  }
  // Ignore other message types (commands are one-way from sender to receiver)
}

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

  // Register receive callback for temperature messages
  // Using function pointer for compatibility
  esp_now_register_recv_cb(onReceiveTemp);

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
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toCharArray(outgoingMsg.command, sizeof(outgoingMsg.command));
    esp_now_send(receiverMAC, (uint8_t *)&outgoingMsg, sizeof(outgoingMsg));
    Serial.print("📤 Sent: ");
    Serial.println(outgoingMsg.command);
  }
}
