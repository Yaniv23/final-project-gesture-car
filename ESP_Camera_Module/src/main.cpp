#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/message_buffer.h"

// =================== USER SETTINGS ===================
// Change these to match your WiFi network
const char *WIFI_SSID     = "iPhone de Yaniv";
const char *WIFI_PASSWORD = "12345678";

// =================== UDP CONFIGURATION ===================
// Configuration UDP - Changez l'IP pour correspondre à votre PC
// Pour trouver votre IP: hostname -I  ou  ip addr show wlo1
const char* TARGET_IP = "172.20.10.7";  // IP de votre PC (trouvée via ip addr)
const int UDP_PORT = 5000;

// UDP fragmentation settings
const size_t UDP_PACKET_MAX_SIZE = 1400;  // Safe size for WiFi MTU (1500 - headers)
const size_t UDP_PACKET_DATA_SIZE = UDP_PACKET_MAX_SIZE - 8;  // 8 bytes for header

// Packet header structure (8 bytes)
struct PacketHeader {
  uint32_t frame_id;        // Frame sequence number
  uint16_t fragment_id;     // Fragment number (0-based)
  uint16_t total_fragments;  // Total number of fragments
};

// =================== CAMERA PINS (OV2640 on ESP32-S3) ===================
// These pins are taken from your existing S3 camera config (camerapins.h)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    15
#define SIOD_GPIO_NUM    4
#define SIOC_GPIO_NUM    5

#define Y9_GPIO_NUM      16
#define Y8_GPIO_NUM      17
#define Y7_GPIO_NUM      18
#define Y6_GPIO_NUM      12
#define Y5_GPIO_NUM      10
#define Y4_GPIO_NUM      8
#define Y3_GPIO_NUM      9
#define Y2_GPIO_NUM      11

#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM    13

// =================== UDP STREAMING ===================

// Message buffer for inter-task communication
MessageBufferHandle_t frame_buffer;

// Camera task: captures frames and sends them to the message buffer
void cam_task(void *pvParameters) {
  Serial.println("Camera task started");
  
  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    // Send frame to message buffer
    // Check if frame fits in buffer (max 35000 bytes)
    if (fb->len > 35000) {
      Serial.printf("Frame too large (%d bytes) - frame dropped\n", fb->len);
      esp_camera_fb_return(fb);
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }
    
    size_t sent = xMessageBufferSend(frame_buffer, (void *)fb->buf, fb->len, 0);
    if (sent != fb->len) {
      Serial.println("Frame buffer full - frame dropped");
    }

    // Return frame buffer to camera
    esp_camera_fb_return(fb);
    
    // Small delay to prevent WDT reset
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

// UDP client task: reads frames from message buffer and sends via UDP
// Static buffer to avoid stack overflow (35000 bytes is too large for stack)
static uint8_t frame_buffer_data[35000];
static uint32_t frame_counter = 0;

void udp_client_task(void *pvParameters) {
  Serial.println("UDP client task started");
  
  WiFiUDP udp;
  
  while (true) {
    // Wait for WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("UDP task: Waiting for WiFi...");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    // Read frame from message buffer
    // Increased buffer size to handle QVGA JPEG frames (can be up to ~30KB)
    // Buffer is static to avoid stack overflow
    size_t frame_size = xMessageBufferReceive(frame_buffer, (void *)frame_buffer_data, sizeof(frame_buffer_data), portMAX_DELAY);
    
    if (frame_size > 0) {
      // Calculate number of fragments needed
      uint16_t total_fragments = (frame_size + UDP_PACKET_DATA_SIZE - 1) / UDP_PACKET_DATA_SIZE;
      frame_counter++;
      
      // Send frame in fragments
      bool send_success = true;
      for (uint16_t frag = 0; frag < total_fragments; frag++) {
        size_t offset = frag * UDP_PACKET_DATA_SIZE;
        size_t fragment_size = (offset + UDP_PACKET_DATA_SIZE <= frame_size) 
                               ? UDP_PACKET_DATA_SIZE 
                               : (frame_size - offset);
        
        // Create packet header
        PacketHeader header;
        header.frame_id = frame_counter;
        header.fragment_id = frag;
        header.total_fragments = total_fragments;
        
        // Send packet
        udp.beginPacket(TARGET_IP, UDP_PORT);
        udp.write((uint8_t*)&header, sizeof(header));
        size_t written = udp.write(frame_buffer_data + offset, fragment_size);
        
        if (!udp.endPacket()) {
          Serial.printf("UDP send failed for fragment %d/%d\n", frag + 1, total_fragments);
          send_success = false;
          break;
        }
        
        // Small delay between fragments to avoid overwhelming the network
        if (frag < total_fragments - 1) {
          vTaskDelay(1 / portTICK_PERIOD_MS);
        }
      }
      
      if (send_success && total_fragments > 1) {
        // Only log for fragmented frames to avoid spam
        static uint32_t last_log_frame = 0;
        if (frame_counter - last_log_frame >= 30) {
          Serial.printf("Sent frame %lu (%d bytes, %d fragments)\n", frame_counter, frame_size, total_fragments);
          last_log_frame = frame_counter;
        }
      }
    }
    
    // Small delay to prevent WDT reset
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

// =================== CAMERA INITIALIZATION ===================

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Smaller frame and medium quality = smoother streaming
  config.frame_size = FRAMESIZE_QVGA;  // 320x240
  config.jpeg_quality = 15;            // 0 = best, 63 = worst
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }

  // Configure camera orientation
  // Adjust these based on your camera mounting:
  // - set_hmirror(true) = flip left-right (mirror effect)
  // - set_vflip(true) = flip upside-down
  sensor_t *s = esp_camera_sensor_get();
  if (s != nullptr) {
    s->set_hmirror(s, true);   // Horizontal mirror (left-right flip)
    s->set_vflip(s, true);     // Vertical flip (upside-down flip)
    Serial.println("Camera orientation: horizontal mirror + vertical flip enabled");
  }

  Serial.println("Camera init OK");
  return true;
}

// =================== SETUP & LOOP ===================

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("=== Simple ESP32-S3 OV2640 Camera ===");

  if (!psramFound()) {
    Serial.println("No PSRAM found! Camera needs PSRAM on ESP32-S3.");
    while (true) {
      delay(1000);
    }
  }

  if (!initCamera()) {
    Serial.println("Camera init failed, stopping.");
    while (true) {
      delay(1000);
    }
  }

  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("UDP target: ");
    Serial.print(TARGET_IP);
    Serial.print(":");
    Serial.println(UDP_PORT);
    
    // Create message buffer for inter-task communication
    // Increased size to handle QVGA JPEG frames (can be up to ~30KB)
    frame_buffer = xMessageBufferCreate(35000);
    if (frame_buffer == NULL) {
      Serial.println("Failed to create frame buffer!");
      while (true) {
        delay(1000);
      }
    }
    
    // Create camera capture task
    xTaskCreate(
      cam_task,
      "cam_task",
      8192,
      NULL,
      configMAX_PRIORITIES,
      NULL
    );
    
    // Create UDP client task
    // Stack size: buffer is now static (outside stack), so 8KB is sufficient
    xTaskCreate(
      udp_client_task,
      "udp_client",
      8192,  // Sufficient now that 35KB buffer is static (not on stack)
      NULL,
      configMAX_PRIORITIES,
      NULL
    );
    
    Serial.println("UDP streaming tasks started");
  } else {
    Serial.println("WiFi connection failed. Check SSID/PASSWORD.");
  }
}

void loop() {
  // Nothing to do here.
  // The camera server runs in the background (in its own task).
  delay(1000);
}

