#include <Arduino.h>
#include <esp_camera.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_heap_caps.h>
#include <esp32/spiram.h>

// ===== WiFi Configuration =====
const char *WIFI_SSID = "iPhone de Yaniv";
const char *WIFI_PASSWORD = "12345678";

// ===== UDP Configuration =====
const char *TARGET_IP = "172.20.10.7";  // Replace with your PC IP
const int TARGET_PORT = 5000;
const int DISCOVERY_PORT = 5001;
WiFiUDP udp;
IPAddress targetIP;

// ===== Camera Configuration for OV3660 =====
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1   // Use internal pull-up, no external reset on AI Thinker ESP32-CAM
#define XCLK_GPIO_NUM 0
#define SIMD_GPIO_NUM 26    // SIOD / SDA
#define SIMC_GPIO_NUM 27    // SIOC / SCL

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// ===== Frame Buffer Configuration =====
#define FRAME_BUFFER_COUNT 2
#define JPEG_QUALITY 15  // Higher compression = smaller frames = less fragmentation

// ===== UDP Packet Configuration =====
constexpr size_t PACKET_HEADER_SIZE = 8;            // frame_id (4) + fragment_id (2) + total_fragments (2)
constexpr size_t MAX_PAYLOAD_PER_PACKET = 1024;     // bytes of JPEG data per UDP packet
constexpr uint16_t START_OF_FRAME_MARKER = 0xFFFF;   // fragment_id used as frame start marker

volatile uint32_t frameID = 0;

// ===== Function Prototypes =====
void initWiFi();
void initCamera();
void handleDiscovery();
void sendFrameOverUDP(camera_fb_t *fb);
void printCameraInfo();
void writeHeader(uint8_t *buf, uint32_t frameId, uint16_t fragmentId, uint16_t totalFragments);

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

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("================================");
  Serial.println("ESP32-CAM OV3660 UDP Streamer");
  Serial.println("================================");

  // Initialize camera
  Serial.println("[SETUP] Initializing camera...");
  initCamera();
  printCameraInfo();

  // Stabilize camera - discard first frames and wait for sensor to settle
  Serial.println("[SETUP] Stabilizing camera sensor...");
  for (int i = 0; i < 10; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
    }
    delay(100);
  }
  Serial.println("[SETUP] Camera stabilization complete");

  // Initialize WiFi
  Serial.println("[SETUP] Initializing WiFi...");
  initWiFi();

  // Initialize UDP
  Serial.println("[SETUP] Initializing UDP...");
  udp.begin(DISCOVERY_PORT);

  Serial.println("[SETUP] Setup complete. Ready to stream!");
  Serial.println("================================\n");
}

void loop() {
  // Handle discovery requests
  handleDiscovery();

  // Capture and send frame
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[ERROR] Camera capture failed - fb_get returned NULL");
    // Reset camera if capture fails repeatedly
    static int failCount = 0;
    failCount++;
    if (failCount > 50) {
      Serial.println("[ERROR] Too many capture failures, reinitializing camera...");
      esp_camera_deinit();
      delay(100);
      initCamera();
      failCount = 0;
      // Stabilize again
      for (int i = 0; i < 5; i++) {
        camera_fb_t *temp_fb = esp_camera_fb_get();
        if (temp_fb) esp_camera_fb_return(temp_fb);
        delay(50);
      }
    }
    delay(50);
    return;
  } else {
    static int failCount = 0;
    failCount = 0; // Reset fail counter on success
  }

  // Send frame over UDP
  sendFrameOverUDP(fb);

  // Return frame buffer to driver
  esp_camera_fb_return(fb);

  // Delay to ensure all fragments are sent before next frame
  // 20ms = ~50 FPS max
  delay(20);
}

// ===== WiFi Initialization =====
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // Disable WiFi sleep for better performance
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] RSSI: ");
    Serial.println(WiFi.RSSI());
    targetIP.fromString(TARGET_IP);
  } else {
    Serial.println("\n[WiFi] Failed to connect");
  }
}

// ===== Camera Initialization =====
void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  // Data lines d0-d7 must map to Y2-Y9 in ascending order
  config.pin_d7 = Y9_GPIO_NUM; // D7 -> Y9
  config.pin_d6 = Y8_GPIO_NUM; // D6 -> Y8
  config.pin_d5 = Y7_GPIO_NUM; // D5 -> Y7
  config.pin_d4 = Y6_GPIO_NUM; // D4 -> Y6
  config.pin_d3 = Y5_GPIO_NUM; // D3 -> Y5
  config.pin_d2 = Y4_GPIO_NUM; // D2 -> Y4
  config.pin_d1 = Y3_GPIO_NUM; // D1 -> Y3
  config.pin_d0 = Y2_GPIO_NUM; // D0 -> Y2
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_sccb_sda = SIMD_GPIO_NUM;
  config.pin_sccb_scl = SIMC_GPIO_NUM;

  config.xclk_freq_hz = 20000000;  // 20MHz clock for OV3660
  config.pixel_format = PIXFORMAT_JPEG;

  // Image resolution
  config.frame_size = FRAMESIZE_QVGA;  // 320x240 - minimal fragmentation
  config.jpeg_quality = JPEG_QUALITY;
  config.fb_count = FRAME_BUFFER_COUNT;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  // Camera module
  config.sccb_i2c_port = 0;  // Default I2C port for ESP32-CAM

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] Camera init failed with error 0x%x\n", err);
    return;
  }

  // Get sensor and configure for OV3660
  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    Serial.println("[Camera] OV3660 sensor detected");
    // OV3660 specific settings
    s->set_brightness(s, 0);     // brightness
    s->set_contrast(s, 0);       // contrast
    s->set_saturation(s, 0);     // saturation
    s->set_special_effect(s, 0); // no special effect
    s->set_wb_mode(s, 1);        // auto white balance
    s->set_exposure_ctrl(s, 1);  // auto exposure
    s->set_aec_value(s, 300);    // exposure value
  } else {
    Serial.printf("[Camera] Sensor ID: 0x%04X (OV3660 PID: 0x%04X)\n", s->id.PID, OV3660_PID);
  }

  Serial.println("[Camera] Camera initialized successfully");
}

// ===== Discovery Handler =====
void handleDiscovery() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char incomingPacket[255];
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = 0;
    }
    Serial.printf("[Discovery] Received: %s from %s:%d\n", incomingPacket, udp.remoteIP().toString().c_str(), udp.remotePort());

    // Respond with our IP and ready status
    String response = "ESP32-CAM:" + WiFi.localIP().toString() + ":READY";
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write((uint8_t *)response.c_str(), response.length());
    udp.endPacket();
  }
}

// ===== UDP Frame Transmission =====
void sendFrameOverUDP(camera_fb_t *fb) {
  if (!fb || fb->len == 0) {
    Serial.println("[ERROR] Invalid frame buffer");
    return;
  }

  const size_t maxChunk = MAX_PAYLOAD_PER_PACKET;
  const uint16_t totalFragments = (fb->len + maxChunk - 1) / maxChunk;
  const uint32_t currentFrameId = frameID++;

  uint8_t header[PACKET_HEADER_SIZE];

  // Send frame start marker
  writeHeader(header, currentFrameId, START_OF_FRAME_MARKER, totalFragments);
  udp.beginPacket(targetIP, TARGET_PORT);
  udp.write(header, PACKET_HEADER_SIZE);
  udp.endPacket();

  // Send frame data in fragments
  size_t offset = 0;
  for (uint16_t frag = 0; frag < totalFragments; ++frag) {
    const size_t chunkSize = (offset + maxChunk > fb->len) ? (fb->len - offset) : maxChunk;
    writeHeader(header, currentFrameId, frag, totalFragments);
    udp.beginPacket(targetIP, TARGET_PORT);
    udp.write(header, PACKET_HEADER_SIZE);
    udp.write(fb->buf + offset, chunkSize);
    udp.endPacket();
    offset += chunkSize;
  }

  Serial.printf("[UDP] Sent frame %u in %u fragments (total: %u bytes)\n", currentFrameId, totalFragments, fb->len);
}

// ===== Debug Info =====
void printCameraInfo() {
  sensor_t *s = esp_camera_sensor_get();
  Serial.println("\n[Camera Info]");
  Serial.printf("  Sensor ID: 0x%04X\n", s->id.PID);
  Serial.printf("  PSRAM Size: %u bytes\n", esp_spiram_get_size());
  Serial.printf("  Free PSRAM: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  Serial.println();
}