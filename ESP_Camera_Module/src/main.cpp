#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"

// =================== USER SETTINGS ===================
// Change these to match your WiFi network
const char *WIFI_SSID     = "TP-Link_IoT_4720";
const char *WIFI_PASSWORD = "88628257";

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

// =================== SIMPLE MJPEG STREAM HANDLER ===================

static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
static const char *STREAM_BOUNDARY     = "\r\n--frame\r\n";
static const char *STREAM_PART         = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// Called by the HTTP server when a client requests /stream
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = nullptr;
  esp_err_t res;

  // Tell the browser this is an MJPEG stream
  res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  // Endless loop: capture a frame, send it, repeat
  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return ESP_FAIL;
    }

    // Send boundary between frames
    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res != ESP_OK) {
      esp_camera_fb_return(fb);
      break;
    }

    // Send JPEG headers (size of the image)
    char header[64];
    int hlen = snprintf(header, sizeof(header), STREAM_PART, fb->len);
    res = httpd_resp_send_chunk(req, header, hlen);
    if (res != ESP_OK) {
      esp_camera_fb_return(fb);
      break;
    }

    // Send the image bytes
    res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);

    if (res != ESP_OK) {
      break;
    }
  }

  return res;
}

// Simple index page: shows the video using an <img> tag
static esp_err_t index_handler(httpd_req_t *req) {
  const char html[] =
      "<!DOCTYPE html>"
      "<html>"
      "<head><meta charset='UTF-8'><title>ESP32-S3 Camera</title></head>"
      "<body style='margin:0; background:#000; display:flex; justify-content:center; align-items:center; height:100vh;'>"
      "<img src='/stream' style='width:90vw; height:auto; max-height:90vh; object-fit:contain;' />"
      "</body>"
      "</html>";

  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html, strlen(html));
}

// Start a very small HTTP server with 2 endpoints:
//   /      -> simple HTML page with <img>
//   /stream -> MJPEG video stream
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_handle_t server = nullptr;
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = nullptr};

    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = nullptr};

    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &stream_uri);

    Serial.println("Camera server started");
    Serial.println("Open this URL in your browser:");
    Serial.print("  http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Error starting server!");
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
    startCameraServer();
  } else {
    Serial.println("WiFi connection failed. Check SSID/PASSWORD.");
  }
}

void loop() {
  // Nothing to do here.
  // The camera server runs in the background (in its own task).
  delay(1000);
}

