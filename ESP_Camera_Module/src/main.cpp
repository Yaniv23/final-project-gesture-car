#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/message_buffer.h"
#include "esp_log.h"

const char *WIFI_SSID     = "iPhone de Yaniv";
const char *WIFI_PASSWORD = "12345678";

const char* TARGET_IP = "172.20.10.7";
const int UDP_PORT = 5000;

const size_t UDP_PACKET_MAX_SIZE = 1400;
const size_t UDP_PACKET_DATA_SIZE = UDP_PACKET_MAX_SIZE - 8;

struct PacketHeader {
  uint32_t frame_id;
  uint16_t fragment_id;
  uint16_t total_fragments;
};
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

MessageBufferHandle_t frame_buffer;

// Statistics
static uint32_t frames_dropped = 0;
static uint32_t packets_sent = 0;
static uint32_t packets_failed = 0;

void cam_task(void *pvParameters) {
  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    if (fb->len > 35000) {
      esp_camera_fb_return(fb);
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }
    
    // Flow control: Check if buffer has space, drop frame if full (non-blocking)
    size_t space_available = xMessageBufferSpacesAvailable(frame_buffer);
    if (space_available < fb->len) {
      // Buffer is full, drop this frame to prevent blocking
      frames_dropped++;
      esp_camera_fb_return(fb);
      vTaskDelay(5 / portTICK_PERIOD_MS);
      continue;
    }
    
    BaseType_t result = xMessageBufferSend(frame_buffer, (void *)fb->buf, fb->len, 0);
    if (result != pdTRUE) {
      frames_dropped++;
    }
    esp_camera_fb_return(fb);
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

static uint8_t frame_buffer_data[35000];
static uint32_t frame_counter = 0;
static uint32_t last_wifi_check = 0;
static const uint32_t WIFI_CHECK_INTERVAL_MS = 5000; // Check WiFi every 5 seconds

// Function to check and reconnect WiFi if needed
bool ensureWiFiConnected() {
  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    return true;
  }
  
  // Try to reconnect
  if (status == WL_DISCONNECTED || status == WL_CONNECTION_LOST) {
    Serial.printf("[WiFi] Connection lost, attempting reconnect...\n");
    WiFi.disconnect();
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(250);
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WiFi] Reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
      return true;
    } else {
      Serial.printf("[WiFi] Reconnection failed\n");
      return false;
    }
  }
  
  return false;
}

// Function to send UDP packet with retry logic
bool sendUdpPacket(WiFiUDP& udp, const uint8_t* data, size_t len, uint32_t frame_id, uint16_t frag_id) {
  const int MAX_RETRIES = 3;
  int retry_count = 0;
  
  while (retry_count < MAX_RETRIES) {
    // Check WiFi before each attempt
    if (!ensureWiFiConnected()) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      retry_count++;
      continue;
    }
    
    // Try to begin packet
    if (!udp.beginPacket(TARGET_IP, UDP_PORT)) {
      retry_count++;
      vTaskDelay((retry_count * 5) / portTICK_PERIOD_MS); // Exponential backoff
      continue;
    }
    
    // Write data
    size_t written = udp.write(data, len);
    if (written != len) {
      udp.stop();
      retry_count++;
      vTaskDelay((retry_count * 5) / portTICK_PERIOD_MS);
      continue;
    }
    
    // End packet and check result
    if (!udp.endPacket()) {
      packets_failed++;
      retry_count++;
      vTaskDelay((retry_count * 5) / portTICK_PERIOD_MS);
      continue;
    }
    
    // Success
    packets_sent++;
    return true;
  }
  
  // All retries failed
  packets_failed++;
  return false;
}

void udp_client_task(void *pvParameters) {
  WiFiUDP udp;
  
  while (true) {
    // Periodic WiFi status check
    uint32_t now = millis();
    if (now - last_wifi_check > WIFI_CHECK_INTERVAL_MS) {
      ensureWiFiConnected();
      last_wifi_check = now;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    size_t frame_size = xMessageBufferReceive(frame_buffer, (void *)frame_buffer_data, sizeof(frame_buffer_data), portMAX_DELAY);
    
    if (frame_size > 0) {
      uint16_t total_fragments = (frame_size + UDP_PACKET_DATA_SIZE - 1) / UDP_PACKET_DATA_SIZE;
      frame_counter++;
      
      // Calculate dynamic delay based on fragment size and total fragments
      // Larger packets and more fragments need more time between sends
      uint32_t base_delay_ms = 2; // Base delay of 2ms
      uint32_t fragment_delay = base_delay_ms + (total_fragments / 10); // Add delay for many fragments
      
      for (uint16_t frag = 0; frag < total_fragments; frag++) {
        size_t offset = frag * UDP_PACKET_DATA_SIZE;
        size_t fragment_size = (offset + UDP_PACKET_DATA_SIZE <= frame_size) 
                               ? UDP_PACKET_DATA_SIZE 
                               : (frame_size - offset);
        
        PacketHeader header;
        header.frame_id = frame_counter;
        header.fragment_id = frag;
        header.total_fragments = total_fragments;
        
        // Prepare packet data
        uint8_t packet_data[sizeof(header) + fragment_size];
        memcpy(packet_data, &header, sizeof(header));
        memcpy(packet_data + sizeof(header), frame_buffer_data + offset, fragment_size);
        
        // Send with retry logic
        bool success = sendUdpPacket(udp, packet_data, sizeof(packet_data), frame_counter, frag);
        
        if (!success && frag == 0) {
          // If first fragment fails, skip entire frame to avoid partial frames
          break;
        }
        
        // Dynamic delay between fragments (except last one)
        if (frag < total_fragments - 1) {
          vTaskDelay(fragment_delay / portTICK_PERIOD_MS);
        }
      }
      
      // Periodic statistics (every 100 frames)
      if (frame_counter % 100 == 0) {
        Serial.printf("[Stats] Frames: %lu, Dropped: %lu, Packets Sent: %lu, Failed: %lu\n",
                      frame_counter, frames_dropped, packets_sent, packets_failed);
      }
    }
    
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

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
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 15;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s != nullptr) {
    s->set_hmirror(s, true);
    s->set_vflip(s, true);
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  esp_log_level_set("wifi", ESP_LOG_ERROR);
  esp_log_level_set("WiFiUdp", ESP_LOG_ERROR);

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

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    frame_buffer = xMessageBufferCreate(35000);
    if (frame_buffer == NULL) {
      Serial.println("Failed to create frame buffer!");
      while (true) {
        delay(1000);
      }
    }
    
    xTaskCreate(
      cam_task,
      "cam_task",
      8192,
      NULL,
      configMAX_PRIORITIES,
      NULL
    );
    
    xTaskCreate(
      udp_client_task,
      "udp_client",
      8192,
      NULL,
      configMAX_PRIORITIES,
      NULL
    );
  } else {
    Serial.println("WiFi connection failed. Check SSID/PASSWORD.");
  }
}

void loop() {
  delay(1000);
}

