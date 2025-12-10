#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "Orbit IoT";
const char* password = "OrbitRyanRD";

const String serverName = "https://73bj12ipia.execute-api.us-east-1.amazonaws.com/default/SmarParBackEnd";

#define RS485_RX_PIN 18
#define RS485_TX_PIN 17
#define RS485_DE_PIN 4 

#define RS485_BAUD 9600
#define POLL_TIMEOUT 200     
#define POLL_INTERVAL 1000   

String spotIDs[] = {"A10 "}; 
int spotCount = 1;

#define RS485_TRANSMIT HIGH
#define RS485_RECEIVE  LOW

HardwareSerial RS485Serial(1);

void pollSensor(String id);
void sendToAWS(String spotId, String status);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10); 
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");

  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, RS485_RECEIVE); 
  RS485Serial.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

  Serial.println("\n=== PARKING MASTER SYSTEM STARTED ===");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Lost... Reconnecting");
    WiFi.reconnect();
  }

  for (int i = 0; i < spotCount; i++) {
    String currentSpot = spotIDs[i];
    currentSpot.trim(); 
    pollSensor(currentSpot);
    delay(200); 
  }
  delay(POLL_INTERVAL);
}

void pollSensor(String id) {
  Serial.print("Polling Spot [" + id + "]... ");

  digitalWrite(RS485_DE_PIN, RS485_TRANSMIT);
  delayMicroseconds(50); 
  RS485Serial.println(id);
  RS485Serial.flush();   
  digitalWrite(RS485_DE_PIN, RS485_RECEIVE);
  
  unsigned long startTime = millis();
  bool responseReceived = false;
  String response = "";

  while (millis() - startTime < POLL_TIMEOUT) {
    if (RS485Serial.available()) {
      char c = RS485Serial.read();
      if (c == '\n') {
        responseReceived = true;
        break;
      }
      if (c != '\r') response += c;
    }
  }

  if (responseReceived) {
    response.trim();
    if (response.startsWith(id)) {
      String status = "";
      if (response.indexOf("Taken") != -1) {
        Serial.println("Response: 🔴 OCCUPIED");
        status = "Taken";
      } 
      else if (response.indexOf("Available") != -1) {
        Serial.println("Response: 🟢 VACANT");
        status = "Available";
      }

      if (status != "") {
        sendToAWS(id, status);
      }

    } else {
      Serial.println("Error: ID Mismatch");
    }
  } else {
    Serial.println("NO REPLY (Timeout)");
  }
}

void sendToAWS(String spotId, String status) {
  HTTPClient http;
  
  bool connected = http.begin(serverName); 
  http.addHeader("Content-Type", "application/json");
  if (!connected) {
    Serial.println("Connection failed!");
    return; 
  }
  StaticJsonDocument<5000> doc;
  doc["spotId"] = spotId;
  doc["status"] = status;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0) {
    Serial.print("AWS Response: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("AWS Error: ");
    Serial.println(httpResponseCode);
  }
}
