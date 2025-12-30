#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME "Auto lock online"
#define BLYNK_AUTH_TOKEN ""

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp32.h>

const char* ssid = "";
const char* password = "";
const char* scriptURL = "";

char auth[] = BLYNK_AUTH_TOKEN;
WiFiServer server(8080);

// Blynk virtual pins
#define VPIN_VOLTAGE   V0
#define VPIN_CURRENT   V1
#define VPIN_SOC       V2
#define VPIN_TEMP      V3
#define VPIN_SOC_BLINK V4

#define LED_BUILTIN 2

// Parameters
float voltage = 0.0;
float current = 0.0;
float soc = 0.0;
float temperature = 0.0;
float latitude = 0.0;
float longitude = 0.0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected: " + WiFi.localIP().toString());

  server.begin();
  Blynk.begin(auth, ssid, password);
}

void loop() {
  Blynk.run();
  handleTCPClient();

  // Update Blynk dashboard
  Blynk.virtualWrite(VPIN_VOLTAGE, voltage);
  Blynk.virtualWrite(VPIN_CURRENT, current);
  Blynk.virtualWrite(VPIN_SOC, soc);
  Blynk.virtualWrite(VPIN_TEMP, temperature);

  // LED alert for low SOC
  if (soc < 21) {
    for (int i = 0; i < 30; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      Blynk.virtualWrite(VPIN_SOC_BLINK, 255);
      delay(200);
      digitalWrite(LED_BUILTIN, LOW);
      Blynk.virtualWrite(VPIN_SOC_BLINK, 0);
      delay(200);
    }
  } else {
    digitalWrite(LED_BUILTIN, LOW);
    Blynk.virtualWrite(VPIN_SOC_BLINK, 0);
  }

  sendToGoogleSheets();  // 📤 Send to Google Sheets
  delay(10000);
}

void handleTCPClient() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("🔌 TCP Client Connected");
    String buffer = "";

    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          parseJSONData(buffer);
          client.stop();
          return;
        }
        buffer += c;
      }
    }
  }
}

void parseJSONData(String jsonStr) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) {
    Serial.println("⚠️ JSON parse error");
    return;
  }

  voltage = doc["voltage"];
  current = doc["current"];
  soc     = doc["soc"];
  if (doc.containsKey("temperature")) {
    temperature = doc["temperature"];
  }
  if (doc.containsKey("latitude")) {
    latitude = doc["latitude"];
  }
  if (doc.containsKey("longitude")) {
    longitude = doc["longitude"];
  }

  Serial.printf("✔️ Parsed: %.2f V, %.2f A, %.1f %% %.1f°C\n", voltage, current, soc, temperature);
  Serial.printf("📍 Location: %.6f, %.6f\n", latitude, longitude);
}

void sendToGoogleSheets() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(scriptURL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<384> doc;
    doc["voltage"] = voltage;
    doc["current"] = current;
    doc["soc"] = soc;
    doc["temperature"] = temperature;
    doc["latitude"] = latitude;
    doc["longitude"] = longitude;

    String jsonStr;
    serializeJson(doc, jsonStr);

    int httpCode = http.POST(jsonStr);
    Serial.println("📤 Sent to Google Sheets. Code: " + String(httpCode));
    http.end();
  }
}
