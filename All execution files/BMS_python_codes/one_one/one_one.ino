#include <WiFi.h>

const char* ssid = "";         // ESP32 hotspot SSID or your WiFi
const char* password = "";     // Match this with your setup

WiFiServer server(8080);               // Same port as Python

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);  // If using station mode (connect to router)

  // WiFi.softAP(ssid, password);  // Uncomment if using ESP32 as hotspot

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ ESP32 Connected!");
  Serial.println(WiFi.localIP());

  server.begin();
  Serial.println("📡 Listening for TCP data...");
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    Serial.println("🔌 Client Connected!");

    String buffer = "";

    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          Serial.println("📥 Received: " + buffer);
          buffer = "";
        } else {
          buffer += c;
        }
      }
    }

    client.stop();
    Serial.println("❌ Client Disconnected");
  }
}
