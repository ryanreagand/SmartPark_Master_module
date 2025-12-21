#include <Arduino.h>
#include <TFT_eSPI.h> 
#include <SPI.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "logo.h" 
const char* ssid = "Orbit IoT";
const char* password = "OrbitRyanRD";
const String serverName = "https://92da14ipia.execute-api.us-east-1.amazonaws.com/default/SmarParBackend";
#define RS485_RX_PIN 18
#define RS485_TX_PIN 17
#define RS485_DE_PIN 4  
#define RS485_BAUD 9600
#define POLL_TIMEOUT 1000     
#define TFT_BG          TFT_BLACK
#define HEADER_HEIGHT   50
#define COLOR_HEADER    TFT_NAVY
#define COLOR_BG        0xFFFF 
#define COLOR_BUTTON    0xE71C 
#define COLOR_BORDER    0xFFFF 
#define COLOR_TEXT      0x0000 
#define BTN_SENSOR_X 20
#define BTN_SENSOR_Y 100
#define BTN_W        200
#define BTN_H        60
#define BTN_SETTING_X 270
#define BTN_SETTING_Y 100
#define BTN_BACK_X    10
#define BTN_BACK_Y    10
#define BTN_BACK_W    80
#define BTN_BACK_H    30
#define BTN_ADD_X     380
#define BTN_ADD_Y     10
#define BTN_ADD_W     80
#define BTN_ADD_H     30
TFT_eSPI tft = TFT_eSPI(); 
HardwareSerial RS485Serial(1);
SemaphoreHandle_t dataMutex; 

enum ScreenState {
  SCREEN_DASHBOARD,
  SCREEN_SENSORLIST,
  SCREEN_SETTINGS,
  SCREEN_ADD_SENSOR
};

ScreenState currentScreen = SCREEN_DASHBOARD;
bool screenNeedsUpdate = true; 

#define MAX_SENSORS 10
String spotIDs[MAX_SENSORS] = {"A5", "", "", "", "", "", "", "", "", ""};

String spotStatus[MAX_SENSORS] = {"WAITING", "WAITING", "WAITING", "", "", "", "", "", "", ""};
int spotCount = 1; 

String tempInput = ""; 
uint16_t calData[5] = { 444, 3335, 436, 3118, 7 };

void sensorTask(void * parameter); 
String pollSensor(String id);
void sendToAWS(String spotId, String status);
void drawDashboard();
void drawSensorList();
void drawSettings();
void drawAddSensorScreen(); 
void drawHeader();

void drawButton(int x, int y, int w, int h, String label, uint16_t color = COLOR_BUTTON, int textSize = 3) {
  tft.fillRoundRect(x, y, w, h, 8, color);
  for(int i = 0; i < 4; i++) {
    tft.drawRoundRect(x + i, y + i, w - i*2, h - i*2, 8, COLOR_BORDER);
  }
  tft.setTextColor(COLOR_TEXT, color);
  tft.setTextSize(textSize);  
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, x + w/2, y + h/2);
}

void drawBackButton() {
  drawButton(BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H, "BACK", TFT_RED, 2);
}

void drawProgressBar(int x, int y, int width, int height, uint8_t progress, String msg) {
  tft.drawRect(x, y, width, height, TFT_WHITE);
  tft.fillRect(x + 2, y + 2, width - 4, height - 4, TFT_DARKGREY);
  int fillWidth = map(progress, 0, 100, 0, width - 4);
  tft.fillRect(x + 2, y + 2, fillWidth, height - 4, TFT_BLUE);
  tft.fillRect(x, y + height + 5, width, 20, TFT_BLACK); 
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.drawString(msg, x + width / 2, y + height + 15);
}

void drawHeader() {
  tft.fillRect(0, 0, 480, HEADER_HEIGHT, COLOR_HEADER);
  tft.drawRect(0, 0, 480, HEADER_HEIGHT, COLOR_BORDER);
  tft.setTextColor(COLOR_BG, COLOR_HEADER);
  tft.setTextSize(3);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("SMARPAR MASTER", 20, HEADER_HEIGHT / 2);
}

void drawDashboard(){
  tft.fillScreen(TFT_BG);
  drawHeader();
  drawButton(BTN_SENSOR_X, BTN_SENSOR_Y, BTN_W, BTN_H, "SENSOR LIST");
  drawButton(BTN_SETTING_X, BTN_SETTING_Y, BTN_W, BTN_H, "SETTINGS");
}

void drawSensorList() {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
   
  tft.fillScreen(TFT_BG);
  drawHeader();
  drawBackButton(); 
  if (spotCount < MAX_SENSORS) {
    drawButton(BTN_ADD_X, BTN_ADD_Y, BTN_ADD_W, BTN_ADD_H, "ADD", TFT_DARKGREEN, 2);
  }

  tft.setTextColor(TFT_WHITE, TFT_BG);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Live Sensor Status", 480/2, 20);

  int startY = 70;
  int cardHeight = 45; 
  int gap = 8;

  for(int i = 0; i < spotCount; i++) {
    int currentY = startY + (i * (cardHeight + gap));
    if (currentY + cardHeight > 320) break; 

    uint16_t statusColor = TFT_DARKGREY;

    if(spotStatus[i] == "OCCUPIED") statusColor = TFT_GREEN;
    else if(spotStatus[i] == "VACANT") statusColor = TFT_RED;
    else if(spotStatus[i] == "NO_REPLY") statusColor = TFT_ORANGE;

    tft.fillRoundRect(20, currentY, 440, cardHeight, 5, statusColor);
    tft.setTextColor(TFT_BLACK, statusColor);
    tft.setTextSize(2); 
    tft.setTextDatum(ML_DATUM);
    tft.drawString("Spot " + spotIDs[i], 40, currentY + (cardHeight/2));
    tft.setTextDatum(MR_DATUM);
    tft.drawString(spotStatus[i], 440, currentY + (cardHeight/2));
  }
  xSemaphoreGive(dataMutex);
}

void drawAddSensorScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Enter New Sensor ID:", 240, 10);

  tft.drawRect(140, 40, 200, 40, TFT_WHITE);
  tft.setTextSize(3);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(tempInput, 240, 60);

  int startY = 100;
  int btnW = 60; int btnH = 50; int gap = 10;
   
  auto drawKey = [&](int col, int row, String label, uint16_t color) {
    int x = 20 + 20 + (col * (btnW + gap));
    int y = startY + (row * (btnH + gap));
    drawButton(x, y, btnW, btnH, label, color, 2);
  };

  drawKey(0, 0, "A", TFT_DARKGREY); drawKey(1, 0, "B", TFT_DARKGREY);
  drawKey(2, 0, "C", TFT_DARKGREY); drawKey(3, 0, "D", TFT_DARKGREY); drawKey(4, 0, "E", TFT_DARKGREY);

  for(int i=0; i<5; i++) drawKey(i, 1, String(i+1), TFT_BLUE);
  for(int i=0; i<4; i++) drawKey(i, 2, String(i+6), TFT_BLUE); drawKey(4, 2, "0", TFT_BLUE);

  drawButton(40, startY + 3*(btnH+gap), 100, 50, "DEL", TFT_RED, 2);
  drawButton(160, startY + 3*(btnH+gap), 100, 50, "CANCEL", TFT_ORANGE, 2);
  drawButton(280, startY + 3*(btnH+gap), 100, 50, "OK", TFT_GREEN, 2);
}

void drawSettings() {
  tft.fillScreen(TFT_BG);
  drawHeader();
  drawBackButton();
  tft.setTextColor(TFT_WHITE, TFT_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Settings Menu", 480/2, 150);
  tft.drawString("WiFi: " + String(ssid), 480/2, 180);
  tft.drawString("IP: " + WiFi.localIP().toString(), 480/2, 210);
  tft.drawString("Sensors: " + String(spotCount) + "/" + String(MAX_SENSORS), 480/2, 240);
}

void sensorTask(void * parameter) {
  int bgPollIndex = 0;
   
  for(;;) { 

    if(WiFi.status() != WL_CONNECTED) WiFi.reconnect();

    String targetID = "";
    int currentLimit = 0;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    if (bgPollIndex < spotCount) {
       targetID = spotIDs[bgPollIndex];
       currentLimit = spotCount;
    }
    xSemaphoreGive(dataMutex);

    if (targetID != "") {
       
      String newStatus = pollSensor(targetID); 

      xSemaphoreTake(dataMutex, portMAX_DELAY);
      bool statusChanged = (spotStatus[bgPollIndex] != newStatus);
      spotStatus[bgPollIndex] = newStatus;
      xSemaphoreGive(dataMutex);
      if (newStatus != "NO_REPLY" && newStatus != "UNKNOWN") {
        sendToAWS(targetID, newStatus);
      }
    }
    bgPollIndex++;
    if (bgPollIndex >= currentLimit) bgPollIndex = 0;
    vTaskDelay(100 / portTICK_PERIOD_MS); 
  }
}

String pollSensor(String id) {
  digitalWrite(RS485_DE_PIN, HIGH); 
  delayMicroseconds(50); 
  RS485Serial.println(id);
  RS485Serial.flush();   
  digitalWrite(RS485_DE_PIN, LOW); 
   
  unsigned long startTime = millis();
  String response = "";
  bool received = false;

  while (millis() - startTime < POLL_TIMEOUT) {
    if (RS485Serial.available()) {
      char c = RS485Serial.read();
      if (c == '\n') { received = true; break; }
      if (c != '\r') response += c;
    }
  }

  if (received) {
    response.trim();
    if (response.indexOf("OCCUPIED") != -1) return "OCCUPIED";
    if (response.indexOf("VACANT") != -1) return "VACANT";
    return "UNKNOWN";
  }
  return "NO_REPLY";
}

void sendToAWS(String spotId, String status){
  String awsStatus = "Available"; 
  if (status == "OCCUPIED") {
    awsStatus = "Taken";
  } else if (status == "VACANT") {
    awsStatus = "Available";
  } else {
    return; 
  }
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); 
  
  if(http.begin(client, serverName)){
    http.addHeader("Content-Type", "application/json");
    StaticJsonDocument<200> doc;
    doc["action"] = "updateSensorStatus"; 
    doc["spotId"] = spotId;
    doc["status"] = awsStatus; 

    String requestBody;
    serializeJson(doc, requestBody);
    
    int httpResponseCode = http.POST(requestBody);
    
    if (httpResponseCode > 0) {
      Serial.println("AWS Success (" + spotId + "): " + awsStatus);
    } else {
      Serial.println("AWS Failed (" + spotId + "): " + String(httpResponseCode));
    }
    
    http.end();
  }
}

void setup() {
  Serial.begin(115200);

  dataMutex = xSemaphoreCreateMutex();

  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW); 
  RS485Serial.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

  tft.init();
  tft.setRotation(1); 
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
  tft.setTouch(calData); 

  int logoX = (tft.width() - 200) / 2;
  int logoY = (tft.height() - 184) / 2 - 30; 
  tft.pushImage(logoX, logoY, 200, 184, logo);

  WiFi.begin(ssid, password);
  int progressX = (tft.width() - 280) / 2;
  int progressY = logoY + 184 + 20; 
  int progress = 0;
  while(progress < 100) {
    String msg = "Connecting WiFi...";
    if(WiFi.status() == WL_CONNECTED) { msg = "Connected!"; progress += 5; } 
    else { progress += 1; }
    drawProgressBar(progressX, progressY, 280, 20, progress, msg);
    delay(50);
  }


  xTaskCreatePinnedToCore(
    sensorTask,  
    "SensorTask", 
    10000,        
    NULL,        
    1,            
    NULL,         
    0            
  );

  tft.fillScreen(TFT_BG);
  drawDashboard();
}


void loop() {
  uint16_t t_x = 0, t_y = 0;
  bool pressed = tft.getTouch(&t_x, &t_y);

  if (pressed) {
    if (currentScreen == SCREEN_DASHBOARD) {
      if (t_x > BTN_SENSOR_X && t_x < BTN_SENSOR_X + BTN_W && 
          t_y > BTN_SENSOR_Y && t_y < BTN_SENSOR_Y + BTN_H) {
         
        tft.drawRoundRect(BTN_SENSOR_X, BTN_SENSOR_Y, BTN_W, BTN_H, 8, TFT_RED);
        delay(100); 
        currentScreen = SCREEN_SENSORLIST;
        screenNeedsUpdate = true; 
        delay(200); 
      }
      else if (t_x > BTN_SETTING_X && t_x < BTN_SETTING_X + BTN_W && 
               t_y > BTN_SETTING_Y && t_y < BTN_SETTING_Y + BTN_H) {
         
        tft.drawRoundRect(BTN_SETTING_X, BTN_SETTING_Y, BTN_W, BTN_H, 8, TFT_RED);
        delay(100);
        currentScreen = SCREEN_SETTINGS;
        screenNeedsUpdate = true; 
        delay(200);
      }
    }
    else if (currentScreen == SCREEN_SENSORLIST) {
      if (t_x > BTN_BACK_X && t_x < BTN_BACK_X + BTN_BACK_W && 
          t_y > BTN_BACK_Y && t_y < BTN_BACK_Y + BTN_BACK_H) {
         
        tft.drawRoundRect(BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H, 8, TFT_WHITE);
        delay(100);
        currentScreen = SCREEN_DASHBOARD;
        screenNeedsUpdate = true;
        delay(200);
      }
      if (t_x > BTN_ADD_X && t_x < BTN_ADD_X + BTN_ADD_W && 
          t_y > BTN_ADD_Y && t_y < BTN_ADD_Y + BTN_ADD_H) {
         
        tft.drawRoundRect(BTN_ADD_X, BTN_ADD_Y, BTN_ADD_W, BTN_ADD_H, 8, TFT_WHITE);
        delay(100);
        tempInput = ""; 
        currentScreen = SCREEN_ADD_SENSOR;
        screenNeedsUpdate = true;
        delay(200);
      }
    }
    else if (currentScreen == SCREEN_ADD_SENSOR) {
      int actionY = 100 + 3*(50+10); 

      if (t_y > actionY && t_y < actionY+50 && t_x > 40 && t_x < 140) {
         if (tempInput.length() > 0) tempInput.remove(tempInput.length()-1);
         screenNeedsUpdate = true; delay(200);
      }

      if (t_y > actionY && t_y < actionY+50 && t_x > 160 && t_x < 260) {
         currentScreen = SCREEN_SENSORLIST;
         screenNeedsUpdate = true; delay(200);
      }

      if (t_y > actionY && t_y < actionY+50 && t_x > 280 && t_x < 380) {
          

         xSemaphoreTake(dataMutex, portMAX_DELAY);
         if (tempInput.length() > 0 && spotCount < MAX_SENSORS) {
            spotIDs[spotCount] = tempInput;
            spotStatus[spotCount] = "WAITING";
            spotCount++;
         }
         xSemaphoreGive(dataMutex);

         currentScreen = SCREEN_SENSORLIST;
         screenNeedsUpdate = true; delay(200);
      }
    
      if (t_y > 100 && t_y < 150) {
        if(t_x > 40 && t_x < 100) tempInput += "A";
        if(t_x > 110 && t_x < 170) tempInput += "B";
        if(t_x > 180 && t_x < 240) tempInput += "C";
        if(t_x > 250 && t_x < 310) tempInput += "D";
        if(t_x > 320 && t_x < 380) tempInput += "E";
        screenNeedsUpdate = true; delay(200);
      }
      if (t_y > 160 && t_y < 210) {
        if(t_x > 40 && t_x < 100) tempInput += "1";
        if(t_x > 110 && t_x < 170) tempInput += "2";
        if(t_x > 180 && t_x < 240) tempInput += "3";
        if(t_x > 250 && t_x < 310) tempInput += "4";
        if(t_x > 320 && t_x < 380) tempInput += "5";
        screenNeedsUpdate = true; delay(200);
      }
      if (t_y > 220 && t_y < 270) {
        if(t_x > 40 && t_x < 100) tempInput += "6";
        if(t_x > 110 && t_x < 170) tempInput += "7";
        if(t_x > 180 && t_x < 240) tempInput += "8";
        if(t_x > 250 && t_x < 310) tempInput += "9";
        if(t_x > 320 && t_x < 380) tempInput += "0";
        screenNeedsUpdate = true; delay(200);
      }
    }
    // ---------------- SETTINGS ----------------
    else if (currentScreen == SCREEN_SETTINGS) {
       if (t_x > BTN_BACK_X && t_x < BTN_BACK_X + BTN_BACK_W && 
           t_y > BTN_BACK_Y && t_y < BTN_BACK_Y + BTN_BACK_H) {
         
        tft.drawRoundRect(BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H, 8, TFT_WHITE);
        delay(100);
        currentScreen = SCREEN_DASHBOARD;
        screenNeedsUpdate = true;
        delay(200);
      }
    }
  }

  if (screenNeedsUpdate) {
    if (currentScreen == SCREEN_DASHBOARD) drawDashboard();
    else if (currentScreen == SCREEN_SENSORLIST) drawSensorList();
    else if (currentScreen == SCREEN_ADD_SENSOR) drawAddSensorScreen(); 
    else if (currentScreen == SCREEN_SETTINGS) drawSettings();
    screenNeedsUpdate = false;
  }
   
  // Refresh the list periodically just to show updated statuses
  static unsigned long lastRefresh = 0;
  if (millis() - lastRefresh > 1000) {
    if (currentScreen == SCREEN_SENSORLIST && !pressed) {
      drawSensorList(); 
    }
    lastRefresh = millis();
  }
}
