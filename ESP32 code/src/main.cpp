#define BLYNK_TEMPLATE_ID "TMPL65vofxwdb"
#define BLYNK_TEMPLATE_NAME "Smart Plant Pot"
#define BLYNK_AUTH_TOKEN "xG82BnT4AY72sQmtiZFL3UNVtzmiyHww"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WebSocketsServer.h>
#include <DHT.h>
#include <cstdlib>
#include <ctime>
#include <ThingSpeak.h>
#include <BlynkSimpleEsp32.h>
#include "FS.h"

// WiFi credentials
const char* ssid = "Helpmeplease";
const char* password = "lataelol";

// ThingSpeak credentials
const char* thingSpeakAPI = "C3XN7PMXD1W8LOC8"; // Replace with your Write API Key
unsigned long channelID = 2761438;        // Replace with your Channel ID

// LINE Notify access token
const String LINE_NOTIFY_TOKEN = "hIq1jdwjHasxdAjDeUbbROqEaMrBk11qJoz4stlM3gR";

// WebSocket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// Pin definitions
#define RELAY_PIN 23
#define SOIL_SENSOR_PIN 34
#define WATER_LEVEL_PIN 32
#define LDR_DO_PIN 33
#define DHTPIN 4
#define DHTTYPE DHT11


// Threshold value for soil moisture to turn on motor
#define MOTOR_THRESHOLD 2000

// Sensor and motor setup
DHT dht(DHTPIN, DHTTYPE);
unsigned long lastDHTTime = 0;

unsigned long lastHumidity = 52.00;

int lastlightStatus = digitalRead(LDR_DO_PIN);
const unsigned long DHTInterval = 30000; // 30 seconds
String data = "{";
// WiFi and HTTPClient objects
WiFiClient client;
HTTPClient http;

// Function to send a LINE notification
void sendLineNotify(String message) {
  if (WiFi.status() == WL_CONNECTED) {  // Check WiFi connection
    http.begin("https://notify-api.line.me/api/notify");  // LINE Notify API URL
    http.addHeader("Authorization", "Bearer " + LINE_NOTIFY_TOKEN);  // Authorization header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");  // Content-Type header

    String payload = "message=" + message;  // Set message
    int httpResponseCode = http.POST(payload);  // Send POST request

    if (httpResponseCode == 200) {
      Serial.println("Notification sent to LINE");
    } else {
      Serial.printf("Error sending notification. HTTP Response Code: %d\n", httpResponseCode);
    }

    http.end();  // Close HTTP connection
  } else {
    Serial.println("Error: WiFi not connected.");
  }
}

void sendRealTimeData() {
  int soilValue = analogRead(SOIL_SENSOR_PIN);
  int waterLevelRaw = analogRead(WATER_LEVEL_PIN);
  int waterLevelPercentage = map(waterLevelRaw, 600, 1800, 0, 100);
  Serial.println(waterLevelRaw);
  waterLevelPercentage = constrain(waterLevelPercentage, 0, 100);
  int lightStatus = digitalRead(LDR_DO_PIN);

  data = "{";
  data += "\"soil_moisture\":" + String(soilValue) + ",";
  data += "\"water_level\":" + String(waterLevelPercentage) + ",";
  data += "\"light\":" + String(lightStatus == HIGH ? 0 : 1) + "}"; // 0 for bright, 1 for dark
  // data += "\"humidity\":" + String(humidity) + "}";
  Serial.println(data);
  webSocket.broadcastTXT(data);

  if (lastlightStatus != lightStatus) {
    if (lightStatus == HIGH) {
     sendLineNotify("Not enough light!!");
     lastlightStatus = lightStatus;
    } else {
     lastlightStatus = lightStatus;
    }

  } 
  
  //Control motor based on soil moisture level
  if (soilValue > MOTOR_THRESHOLD) {
    digitalWrite(RELAY_PIN, HIGH);  // Turn on motor
    Serial.println("Motor ON");
  } else {
    digitalWrite(RELAY_PIN, LOW);   // Turn off motor
    Serial.println("Motor OFF");
  }
}

void sendHumidity(float humidity) {
  // delay(3000);
  // float humidity = dht.readHumidity();
  if (!isnan(humidity)) {
    String message = "{\"humidity\":" + String(humidity, 1) + "}";
    webSocket.broadcastTXT(message);
  } else {
    humidity = lastHumidity;
    String message = "{\"humidity\":" + String(humidity, 1) + "}";
    webSocket.broadcastTXT(message);
    Serial.println("Failed to read from DHT sensor!");
  }
    lastHumidity = humidity;

}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());
      break;
    }
    case WStype_TEXT: {
      String text = String((char*)payload);
      Serial.printf("Client[%u] message: %s\n", num, text.c_str());

      if (text == "MOTOR_ON") {
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("Motor turned ON");
      }
      break;
    }
    default:
      break;
  }
}

void cloudStorage() {
  int soilMoisture = analogRead(SOIL_SENSOR_PIN);
  int waterLevelRaw = analogRead(WATER_LEVEL_PIN);
  int waterLevelPercentage = map(waterLevelRaw, 0, 1780, 0, 100);
  waterLevelPercentage = constrain(waterLevelPercentage, 0, 100);
  int lightStatus = digitalRead(LDR_DO_PIN);
  lightStatus = !lightStatus;
  int pumpStatus = digitalRead(RELAY_PIN);
  float humidity = dht.readHumidity();


  if (!isnan(humidity)) {
    sendHumidity(humidity);
    Serial.println("Read on cloud storage");
    // Serial.println(humidity);

  } else {
    sendHumidity(humidity);
    humidity = lastHumidity;
  }

  // Send data to ThingSpeak
  ThingSpeak.setField(1, soilMoisture); 
  ThingSpeak.setField(2, lightStatus);
  ThingSpeak.setField(3, humidity);
  ThingSpeak.setField(4, waterLevelPercentage); 
  ThingSpeak.setField(5, pumpStatus);
  int responseCode = ThingSpeak.writeFields(channelID, thingSpeakAPI);

  // Send data to Blynk
  char auth[] = BLYNK_AUTH_TOKEN;
  Blynk.virtualWrite(V0, soilMoisture);
  Blynk.virtualWrite(V1, lightStatus);
  Blynk.virtualWrite(V2, humidity);
  Blynk.virtualWrite(V3, waterLevelPercentage);
  Blynk.virtualWrite(V4, pumpStatus);

  // Debug response
  if (responseCode == 200) {
    Serial.println("Data sent to ThingSpeak successfully!");
  } else {
    Serial.printf("Failed to send data. Response code: %d\n", responseCode);
  }
    lastHumidity = humidity;
}

void setup() {
  Serial.begin(9600);

  // WiFi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected!");

  // Pin setup
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(SOIL_SENSOR_PIN, INPUT);
  pinMode(WATER_LEVEL_PIN, INPUT);
  pinMode(LDR_DO_PIN, INPUT);

  // Initialize DHT
  dht.begin();

  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started.");

  // Send a notification when the device starts up
  // sendLineNotify("ESP32 is connected to WiFi and running!");

  // Initialize cloud storage
  ThingSpeak.begin(client);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);

}

void loop() {
  Blynk.run();
  webSocket.loop();

  static unsigned long lastRealTimeData = 0;
  unsigned long currentTime = millis();
  static unsigned long lastThingSpeakTime = 0;
  static unsigned long lastLineNoti = 0;

  // Send real-time data every 2 seconds
  if (currentTime - lastRealTimeData >= 2000) {
    lastRealTimeData = currentTime;
    sendRealTimeData();
  }

  // Send data to ThingSpeak every 15 seconds
  if (currentTime - lastThingSpeakTime >= 15000) {
    lastThingSpeakTime = currentTime;
    cloudStorage();
  }

}