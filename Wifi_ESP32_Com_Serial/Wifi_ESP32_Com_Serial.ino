#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "cohenzag";
const char* password = "0587002303";

WebServer server(80); // HTTP server

void setup() {
  Serial.begin(115200);                        // Serial Monitor
  Serial1.begin(9600, SERIAL_8N1, 16, 17);     // UART (RX=16, TX=17)

  // Wi-Fi connect
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  // HTTP handler to receive command
  server.on("/send", []() {
    if (server.hasArg("cmd")) {
      String cmd = server.arg("cmd");
      Serial1.println(cmd); // Send to Arduino
      server.send(200, "text/plain", "Command sent: " + cmd);
    } else {
      server.send(400, "text/plain", "Missing 'cmd' parameter");
    }
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  // From Serial Monitor to Arduino
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    Serial1.println(msg);
  }

  // From Arduino to Serial Monitor
  if (Serial1.available()) {
    String received = Serial1.readStringUntil('\n');
    Serial.print("From Arduino: ");
    Serial.println(received);
  }
}
