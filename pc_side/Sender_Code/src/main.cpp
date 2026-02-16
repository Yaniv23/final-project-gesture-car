#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// ESP-NOW WiFi channel - MUST match on both sender and vehicle
#define ESPNOW_WIFI_CHANNEL 1

// Control bytes (must match Car/command_protocol.h)
static const uint8_t CMD_HANDSHAKE_INIT  = 0xF0;
static const uint8_t CMD_HANDSHAKE_ACK   = 0xF1;
static const uint8_t CMD_HEARTBEAT       = 0xF2;  // reserved for future use
static const uint8_t CMD_MODE_STATUS     = 0xF4;  // Vehicle → sender: current mode

static const uint8_t PROTOCOL_VERSION    = 0x01;

// Mode constants (must match Car/mode_manager.h)
static const uint8_t MODE_MANUAL         = 0;
static const uint8_t MODE_AUTONOMOUS     = 1;

uint8_t receiverMAC[] = {0x00, 0x4B, 0x12, 0x34, 0xF7, 0xF4};

typedef struct struct_message {
  uint8_t command;   // command byte
  uint8_t param;     // optional parameter/speed byte
} struct_message;

struct_message outgoingMsg;

// Handshake state
volatile bool handshakeAcked = false;

// Connection state tracking
volatile bool isConnected = false;
unsigned long lastHandshakeAttempt = 0;
unsigned long lastSuccessfulSend = 0;
const unsigned long HANDSHAKE_RETRY_INTERVAL_MS = 2000;  // 2 seconds
const unsigned long CONNECTION_CHECK_INTERVAL_MS = 5000;  // 5 seconds

// Vehicle mode tracking (for blocking motion commands in autonomous mode)
volatile uint8_t vehicleMode = MODE_MANUAL;  // Default to manual mode

// REMOVED: getNavStateName() - No longer needed, telemetry reception disabled

// Helper function to send handshake init frame
void sendHandshakeInit() {
  uint8_t buf[2];
  buf[0] = CMD_HANDSHAKE_INIT;
  buf[1] = PROTOCOL_VERSION;
  esp_now_send(receiverMAC, buf, sizeof(buf));
}

// Receive callback: watch for handshake ACK and mode status from vehicle
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < 1) return;
  
  uint8_t cmd = data[0];
  
  if (cmd == CMD_HANDSHAKE_ACK) {
    if (!handshakeAcked || !isConnected) {
      Serial.println("🤝 Handshake ACK received from vehicle");
    }
    handshakeAcked = true;
    isConnected = true;
    lastSuccessfulSend = millis();
  } else if (cmd == CMD_MODE_STATUS && len >= 2) {
    // Vehicle sent its current mode
    uint8_t mode = data[1];
    vehicleMode = mode;  // Update stored mode
    
    if (mode == MODE_MANUAL) {
      Serial.println("📱 Vehicle mode: MANUAL");
    } else if (mode == MODE_AUTONOMOUS) {
      Serial.println("🤖 Vehicle mode: AUTONOMOUS (motion commands blocked)");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);  // Small delay for serial stability
  
  Serial.println("\n========================================");
  Serial.println("ESP-NOW Sender - Starting...");
  Serial.println("========================================");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  WiFi.setSleep(false);
  Serial.println("📡 WiFi sleep mode disabled");
  
  delay(300);  
  
  // Force WiFi channel - CRITICAL for ESP-NOW reliability
  // Both sender and vehicle MUST be on the same channel
  esp_err_t channel_result = esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (channel_result != ESP_OK) {
    Serial.print("❌ Failed to set WiFi channel: ");
    Serial.println(channel_result);
    return;
  }
  Serial.print("📡 WiFi channel set to: ");
  Serial.println(ESPNOW_WIFI_CHANNEL);
  
  esp_wifi_set_max_tx_power(78);  

  // Print MAC address for debugging
  Serial.print("📍 Sender MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("🎯 Target MAC: ");
  char targetMac[18];
  snprintf(targetMac, sizeof(targetMac), "%02X:%02X:%02X:%02X:%02X:%02X",
           receiverMAC[0], receiverMAC[1], receiverMAC[2],
           receiverMAC[3], receiverMAC[4], receiverMAC[5]);
  Serial.println(targetMac);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }
  Serial.println("✅ ESP-NOW initialized");

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
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;  // Use fixed channel
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Failed to add peer");
    return;
  }

  // Initial handshake attempt (non-blocking, max 10 seconds)
  const uint32_t initial_timeout_ms = 10000;  // 10 seconds
  const uint32_t handshake_interval_ms = 500;  // Send every 500ms
  uint32_t start_time = millis();
  lastHandshakeAttempt = start_time;

  Serial.println("📡 Attempting initial handshake with vehicle...");

  while (!handshakeAcked && (millis() - start_time < initial_timeout_ms)) {
    uint32_t now = millis();
    if (now - lastHandshakeAttempt >= handshake_interval_ms) {
      sendHandshakeInit();
      lastHandshakeAttempt = now;
    }
    delay(10);
  }

  if (handshakeAcked) {
    isConnected = true;
    Serial.println("🟢 Initial handshake complete");
  } else {
    Serial.println("⚠️ Initial handshake timeout - will retry in background");
  }
}

void loop() {
  unsigned long now = millis();
  
  // Check connection status periodically
  if (!isConnected) {
    // Not connected - attempt handshake every 2 seconds
    if (now - lastHandshakeAttempt >= HANDSHAKE_RETRY_INTERVAL_MS) {
      static uint32_t retry_count = 0;
      retry_count++;
      if (retry_count % 5 == 0) {  // Print every 10 seconds (5 retries * 2s)
        Serial.println("📡 Attempting reconnection...");
      }
      sendHandshakeInit();
      lastHandshakeAttempt = now;
    }
  } else {
    // Connected - check if connection is still alive
    // Note: We don't immediately mark as disconnected, wait for actual send failure
    if (now - lastSuccessfulSend > CONNECTION_CHECK_INTERVAL_MS && lastSuccessfulSend > 0) {
      // No successful sends recently - connection might be lost
      // Will verify on next send attempt
    }
  }
  
  // Handle serial commands
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

    uint8_t cmd = static_cast<uint8_t>(cmdVal);
    
    // Check if this is a motion command (0x00-0x0C)
    bool isMotionCommand = (cmd >= 0x00 && cmd <= 0x0C);
    
    // Check if this is a mode change command (always allowed)
    bool isModeCommand = (cmd == 0x20 || cmd == 0x21 || cmd == 0x22);
    
    // Block motion commands if vehicle is in autonomous mode
    if (isMotionCommand && vehicleMode == MODE_AUTONOMOUS) {
      Serial.println("🚫 Blocked: Motion commands disabled in AUTONOMOUS mode");
      Serial.println("💡 Send 0x20 to switch to MANUAL mode first");
      return;
    }

    outgoingMsg.command = cmd;
    outgoingMsg.param   = static_cast<uint8_t>(paramVal);

    // Send command and check result
    esp_err_t result = esp_now_send(receiverMAC, 
                                    reinterpret_cast<uint8_t*>(&outgoingMsg), 
                                    sizeof(outgoingMsg));
    
    if (result == ESP_OK) {
      lastSuccessfulSend = now;
      if (!isConnected) {
        Serial.println("✅ Send successful - connection restored");
        isConnected = true;
      }
    } else {
      Serial.print("❌ Send failed (error: ");
      Serial.print(result);
      Serial.println(") - connection lost");
      isConnected = false;
      // Will trigger reconnection attempts in next loop iteration
    }
  }
  
  // Small delay to prevent tight loop
  delay(10);
}

