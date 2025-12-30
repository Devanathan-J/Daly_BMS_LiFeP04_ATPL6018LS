#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"
#include <ArduinoJson.h>

// WiFi Credentials
const char* ssid = "";
const char* password = "";

// DHT Sensor
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Apps Script Web App URL (POST)
const char* scriptURL = "";

// ThingSpeak settings
const char* thingspeakURL = "";
String thingspeakApiKey = "";

void setup() {
  Serial.begin(115200);
  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
}

void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.printf("Temp: %.2f C, Humidity: %.2f %%\n", temperature, humidity);

  // -------- Send to Google Sheets via POST --------
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(scriptURL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> jsonDoc;
    jsonDoc["temperature"] = temperature;
    jsonDoc["humidity"] = humidity;

    String jsonData;
    serializeJson(jsonDoc, jsonData);

    int httpCode = http.POST(jsonData);

    if (httpCode > 0) {
      Serial.print("Google Sheets response code: ");
      Serial.println(httpCode);
      Serial.println("Response: " + http.getString());
    } else {
      Serial.println("Error sending to Google Sheets");
    }

    http.end();
  }

  // -------- Send to ThingSpeak via GET --------
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String tsURL = String(thingspeakURL) + "?api_key=" + thingspeakApiKey +
                   "&field3=" + String(temperature) +
                   "&field4=" + String(humidity);

    http.begin(tsURL);
    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.print("ThingSpeak response code: ");
      Serial.println(httpCode);
      Serial.println("Response: " + http.getString());
    } else {
      Serial.println("Error sending to ThingSpeak");
    }

    http.end();
  }

  delay(60000); // Wait 60 seconds before next reading
}
