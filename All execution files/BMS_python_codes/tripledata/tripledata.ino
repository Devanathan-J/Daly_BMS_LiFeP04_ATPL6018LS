#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME "Auto lock online"
#define BLYNK_AUTH_TOKEN ""

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp32.h>
#include "DHT.h"

// --- WiFi Credentials ---
const char* ssid = "";
const char* password = "";

// --- Google Sheets Web App URL ---
const char* scriptURL = "";

// --- Blynk Auth ---
char auth[] = BLYNK_AUTH_TOKEN;

// --- Virtual Pins ---
#define VPIN_VOLTAGE   V0
#define VPIN_CURRENT   V1
#define VPIN_SOC       V2
#define VPIN_TEMP      V3
#define VPIN_SOC_BLINK V4

// --- DHT Sensor ---
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- TCP Server Port ---
WiFiServer server(8080);

// --- Built-in LED ---
#define LED_BUILTIN 2

// --- Global Variables ---
float voltage = 0.0;
float current = 0.0;
float soc = 0.0;
float temperature = 0.0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
  Serial.println(WiFi.localIP());

  server.begin();
  Serial.println("📡 Listening for TCP data...");

  Blynk.begin(auth, ssid, password);
}

void loop() {
  Blynk.run();
  handleTCPClient(); // Updates voltage/current/soc if data arrives

  // --- Read temperature from DHT ---
  temperature = dht.readTemperature();
  int retries = 0;
  while (isnan(temperature) && retries < 5) {
    delay(200);
    temperature = dht.readTemperature();
    retries++;
  }

  if (isnan(temperature)) {
    Serial.println("❌ DHT failed after retries, using -999.");
    temperature = -999;
  }

  // --- Send data to Blynk ---
  Blynk.virtualWrite(VPIN_VOLTAGE, voltage);
  Blynk.virtualWrite(VPIN_CURRENT, current);
  Blynk.virtualWrite(VPIN_SOC, soc);
  Blynk.virtualWrite(VPIN_TEMP, temperature);

  // --- Blinking LED if SOC < 21 ---
  if (soc < 21) {
    Serial.println("⚠️ SOC < 20%! Blinking LED");
    for (int i = 0; i < 30; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      Blynk.virtualWrite(VPIN_SOC_BLINK, 255);
      delay(200);
      digitalWrite(LED_BUILTIN, LOW);
      Blynk.virtualWrite(VPIN_SOC_BLINK, 0);
      delay(200);
    }
  } else {
    Blynk.virtualWrite(VPIN_SOC_BLINK, 0);
    digitalWrite(LED_BUILTIN, LOW);
  }

  // --- Send data to Google Sheets ---
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(scriptURL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<384> jsonDoc;
    jsonDoc["voltage"] = voltage;
    jsonDoc["current"] = current;
    jsonDoc["soc"] = soc;
    jsonDoc["temperature"] = temperature;

    String jsonStr;
    serializeJson(jsonDoc, jsonStr);
    Serial.println("📦 Sending to Google Sheets: " + jsonStr);

    int httpCode = http.POST(jsonStr);
    if (httpCode > 0) {
      Serial.print("📤 Sent to Sheets. Code: ");
      Serial.println(httpCode);
    } else {
      Serial.println("❌ Failed to send to Sheets.");
    }
    http.end();
  }

  delay(10000); // Send every 10 seconds
}

// --- TCP JSON Receiver ---
void handleTCPClient() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("🔌 Client Connected!");
    String buffer = "";

    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          Serial.println("📥 Received JSON: " + buffer);
          parseJSONData(buffer);
          client.stop();
          Serial.println("✅ Client session complete");
          return;
        } else {
          buffer += c;
        }
      }
    }
  }
}

// --- JSON Parser ---
void parseJSONData(String jsonStr) {
  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) {
    Serial.print("⚠️ JSON parse error: ");
    Serial.println(err.c_str());
    return;
  }

  if (doc.containsKey("voltage")) voltage = doc["voltage"];
  if (doc.containsKey("current")) current = doc["current"];
  if (doc.containsKey("soc")) soc = doc["soc"];

  Serial.printf("✔️ Parsed: Voltage=%.2f V, Current=%.2f A, SOC=%.2f %%\n", voltage, current, soc);
}
