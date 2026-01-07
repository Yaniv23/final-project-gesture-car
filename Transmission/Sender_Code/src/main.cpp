#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// Control bytes (must match Vehicule/command_protocol.h)
static const uint8_t CMD_HANDSHAKE_INIT  = 0xF0;
static const uint8_t CMD_HANDSHAKE_ACK   = 0xF1;
static const uint8_t CMD_HEARTBEAT       = 0xF2;  // reserved for future use

static const uint8_t PROTOCOL_VERSION    = 0x01;

uint8_t receiverMAC[] = {0x00, 0x4B, 0x12, 0x34, 0xF7, 0xF4};

typedef struct struct_message {
  uint8_t command;   // command byte
  uint8_t param;     // optional parameter/speed byte
} struct_message;

struct_message outgoingMsg;

// Handshake state
volatile bool handshakeAcked = false;

// Receive callback: watch for handshake ACK from vehicle
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len >= 1 && data[0] == CMD_HANDSHAKE_ACK) {
    handshakeAcked = true;
    Serial.println("🤝 Handshake ACK received from vehicle");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }

  // Receive callback is used to detect handshake ACK from vehicle.
  esp_now_register_recv_cb(onDataRecv);

  // Send callback kept for future diagnostics; currently does not print
  // to keep Serial output clean during normal operation.
  esp_now_register_send_cb([](const uint8_t *mac, esp_now_send_status_t status) {
    (void)mac;
    (void)status;
  });

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Failed to add peer");
    return;
  }

  // Handshake phase: actively wait for ACK from vehicle
  const uint32_t handshake_timeout_ms  = 60000;  // 60 seconds
  const uint32_t handshake_interval_ms = 200;    // Send every 200ms
  const uint32_t status_interval_ms    = 2000;   // Print status every 2 seconds

  uint32_t start_time       = millis();
  uint32_t last_status_time = start_time;
  uint32_t last_send_time   = start_time;

  Serial.println("📡 Starting ESP-NOW handshake with vehicle...");

  while (!handshakeAcked && (millis() - start_time < handshake_timeout_ms)) {
    uint32_t now = millis();

    // Periodically send handshake-init frame: [INIT, version]
    if (now - last_send_time >= handshake_interval_ms) {
      uint8_t buf[2];
      buf[0] = CMD_HANDSHAKE_INIT;
      buf[1] = PROTOCOL_VERSION;
      esp_now_send(receiverMAC, buf, sizeof(buf));
      last_send_time = now;
    }

    // Print status updates periodically
    if (now - last_status_time >= status_interval_ms) {
      uint32_t elapsed   = now - start_time;
      uint32_t remaining = (handshake_timeout_ms > elapsed)
                             ? (handshake_timeout_ms - elapsed)
                             : 0;
      Serial.print("📡 Waiting for handshake ACK... (");
      Serial.print(elapsed / 1000);
      Serial.print("s elapsed, ");
      Serial.print(remaining / 1000);
      Serial.println("s remaining)");
      last_status_time = now;
    }

    delay(10);
  }

  if (handshakeAcked) {
    Serial.println("🟢 Handshake complete - entering command mode");
  } else {
    Serial.println("⚠️ Handshake timeout - proceeding in best-effort mode");
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


  }
}

