#include <WiFi.h>
#include <WiFiUdp.h>
#include <algorithm>
#include <cstring>
#include "esp_camera.h"
#include "esp32-hal-psram.h"

const char *WIFI_SSID = "iPhone de Yaniv";
const char *WIFI_PASSWORD = "12345678";

// UDP configuration
constexpr uint16_t STREAM_PORT = 5000;
constexpr uint16_t DISCOVERY_PORT = 5001;
constexpr size_t PACKET_HEADER_SIZE = 8;            // frame_id (4) + fragment_id (2) + total_fragments (2)
constexpr size_t MAX_PAYLOAD_PER_PACKET = 1024;     // bytes of JPEG data per UDP packet
constexpr uint16_t START_OF_FRAME_MARKER = 0xFFFF;   // fragment_id used as frame start marker

// AI Thinker ESP32-CAM (OV2640/OV3660 capable) pin map
#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

WiFiUDP udpStream;
WiFiUDP udpDiscovery;
IPAddress targetIp(255, 255, 255, 255);  // Broadcast until a viewer responds
bool hasTargetIp = false;
uint32_t frameCounter = 0;
unsigned long lastAnnounceMs = 0;

void writeHeader(uint8_t *buf, uint32_t frameId, uint16_t fragmentId, uint16_t totalFragments) {
  buf[0] = frameId & 0xFF;
  buf[1] = (frameId >> 8) & 0xFF;
  buf[2] = (frameId >> 16) & 0xFF;
  buf[3] = (frameId >> 24) & 0xFF;
  buf[4] = fragmentId & 0xFF;
  buf[5] = (fragmentId >> 8) & 0xFF;
  buf[6] = totalFragments & 0xFF;
  buf[7] = (totalFragments >> 8) & 0xFF;
}

bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WiFi] Connecting to %s\n", WIFI_SSID);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    if (millis() - start > 15000) {
      Serial.println("[WiFi] Connection timeout");
      return false;
    }
  }

  Serial.print("[WiFi] Connected. IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = CAM_PIN_D0;
  config.pin_d1 = CAM_PIN_D1;
  config.pin_d2 = CAM_PIN_D2;
  config.pin_d3 = CAM_PIN_D3;
  config.pin_d4 = CAM_PIN_D4;
  config.pin_d5 = CAM_PIN_D5;
  config.pin_d6 = CAM_PIN_D6;
  config.pin_d7 = CAM_PIN_D7;
  config.pin_xclk = CAM_PIN_XCLK;
  config.pin_pclk = CAM_PIN_PCLK;
  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href = CAM_PIN_HREF;
  config.pin_sscb_sda = CAM_PIN_SIOD;
  config.pin_sscb_scl = CAM_PIN_SIOC;
  config.pin_pwdn = CAM_PIN_PWDN;
  config.pin_reset = CAM_PIN_RESET;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;   // 640x480 default for stable UDP
  config.jpeg_quality = 12;            // Lower = better quality
  config.fb_count = 2;                 // Use continuous mode when PSRAM is present
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  if (psramFound()) {
    Serial.println("[PSRAM] Found PSRAM, enabling double buffers");
  } else {
    Serial.println("[PSRAM] PSRAM not detected, using single buffer");
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[Camera] Init failed: 0x%X\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    if (s->id.PID == OV3660_PID) {
      s->set_brightness(s, 1);
      s->set_saturation(s, -1);
      s->set_vflip(s, 1);
      s->set_hmirror(s, 0);
    }
    s->set_framesize(s, FRAMESIZE_VGA);
    s->set_quality(s, 12);
  }

  Serial.println("[Camera] Initialized");
  return true;
}

void handleDiscovery() {
  int packetSize = udpDiscovery.parsePacket();
  while (packetSize > 0) {
    char incoming[64] = {0};
    int len = udpDiscovery.read(incoming, sizeof(incoming) - 1);
    incoming[len] = '\0';

    if (strncmp(incoming, "DISCOVER_CAMERA_VIEWER", 22) == 0) {
      IPAddress remote = udpDiscovery.remoteIP();
      Serial.printf("[Discovery] Viewer ping from %s\n", remote.toString().c_str());
      udpDiscovery.beginPacket(remote, DISCOVERY_PORT);
      udpDiscovery.printf("ESP32-CAM:%s", WiFi.localIP().toString().c_str());
      udpDiscovery.endPacket();
      targetIp = remote;
      hasTargetIp = true;
    } else if (strncmp(incoming, "VIEWER_IP:", 10) == 0) {
      IPAddress remote;
      if (remote.fromString(incoming + 10)) {
        targetIp = remote;
        hasTargetIp = true;
        Serial.printf("[Discovery] Viewer IP learned: %s\n", targetIp.toString().c_str());
      }
    }

    packetSize = udpDiscovery.parsePacket();
  }

  const unsigned long now = millis();
  if (now - lastAnnounceMs > 5000) {
    udpDiscovery.beginPacket(IPAddress(255, 255, 255, 255), DISCOVERY_PORT);
    udpDiscovery.printf("ESP32-CAM:%s", WiFi.localIP().toString().c_str());
    udpDiscovery.endPacket();
    lastAnnounceMs = now;
  }
}

bool sendFrame(camera_fb_t *fb) {
  if (!fb || fb->len == 0) {
    return false;
  }

  const size_t maxChunk = MAX_PAYLOAD_PER_PACKET;
  const uint16_t totalFragments = (fb->len + maxChunk - 1) / maxChunk;
  const uint32_t frameId = frameCounter++;

  uint8_t header[PACKET_HEADER_SIZE];

  // Optional frame start marker
  writeHeader(header, frameId, START_OF_FRAME_MARKER, totalFragments);
  udpStream.beginPacket(targetIp, STREAM_PORT);
  udpStream.write(header, PACKET_HEADER_SIZE);
  udpStream.endPacket();

  size_t offset = 0;
  for (uint16_t frag = 0; frag < totalFragments; ++frag) {
    const size_t chunkSize = std::min(maxChunk, fb->len - offset);
    writeHeader(header, frameId, frag, totalFragments);
    udpStream.beginPacket(targetIp, STREAM_PORT);
    udpStream.write(header, PACKET_HEADER_SIZE);
    udpStream.write(fb->buf + offset, chunkSize);
    udpStream.endPacket();
    offset += chunkSize;
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!connectWifi()) {
    Serial.println("[WiFi] Restarting due to connection failure");
    delay(2000);
    ESP.restart();
  }

  if (!initCamera()) {
    Serial.println("[Camera] Restarting due to init failure");
    delay(2000);
    ESP.restart();
  }

  udpDiscovery.begin(DISCOVERY_PORT);
  udpStream.begin(STREAM_PORT);
  Serial.printf("[UDP] Discovery on %u, stream on %u\n", DISCOVERY_PORT, STREAM_PORT);
  Serial.println("[UDP] Waiting for viewer discovery... (defaulting to broadcast)");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Lost connection, reconnecting");
    connectWifi();
  }

  handleDiscovery();

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[Camera] Capture failed");
    delay(50);
    return;
  }

  IPAddress dest = hasTargetIp ? targetIp : IPAddress(255, 255, 255, 255);
  targetIp = dest;

  const bool sent = sendFrame(fb);
  esp_camera_fb_return(fb);

  if (!sent) {
    Serial.println("[UDP] Send failed");
  }

  delay(10);
}
