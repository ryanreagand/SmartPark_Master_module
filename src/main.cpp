#include <Arduino.h>
#include <HardwareSerial.h>

// ===== PIN DEFINITIONS (ESP32-S3) =====
#define RS485_RX_PIN 18
#define RS485_TX_PIN 17
#define RS485_DE_PIN 4  // Direction Pin (Connect DE and RE here)

// ===== SETTINGS =====
#define RS485_BAUD 9600
#define POLL_TIMEOUT 200     // Wait 200ms for a reply before giving up
#define POLL_INTERVAL 1000   // Poll every 1 second

// List of Spots to Poll (You can add more here: "A10", "A11", "B01", etc.)
String spotIDs[] = {"A10 "}; // Note: Space added to match the default formatting
int spotCount = 1;

// RS485 Control Logic
#define RS485_TRANSMIT HIGH
#define RS485_RECEIVE  LOW

HardwareSerial RS485Serial(1);

// Function Prototype
void pollSensor(String id);

void setup() {
  // Debug Serial (USB)
  Serial.begin(115200);
  while (!Serial) delay(10); 
  
  // RS485 Control Pin
  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, RS485_RECEIVE); // Default to Listening

  // RS485 UART Initialization
  // Pin 17/18 are standard UART1 for ESP32-S3
  RS485Serial.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

  Serial.println("\n=== PARKING MASTER SYSTEM STARTED ===");
  Serial.println("Master polling list:");
  for(int i=0; i<spotCount; i++) {
    Serial.println(" - " + spotIDs[i]);
  }
  Serial.println("=====================================\n");
  delay(1000);
}

void loop() {
  // Loop through all defined spots
  for (int i = 0; i < spotCount; i++) {
    // Remove extra spaces for the console print, but keep them if your protocol needs them
    String currentSpot = spotIDs[i];
    currentSpot.trim(); 
    
    pollSensor(currentSpot);
    
    delay(200); // Short delay between polling different sensors
  }

  // Wait before starting the list over
  delay(POLL_INTERVAL);
}

void pollSensor(String id) {
  Serial.print("Polling Spot [");
  Serial.print(id);
  Serial.print("]... ");

  // --- 1. SEND REQUEST ---
  // Switch to Transmit Mode
  digitalWrite(RS485_DE_PIN, RS485_TRANSMIT);
  delayMicroseconds(50); // Hardware switching delay
  
  // Send ID + Newline (Crucial: Slave waits for newline)
  RS485Serial.println(id);
  RS485Serial.flush();   // Wait for data to completely leave the buffer
  
  // Switch back to Receive Mode immediately
  digitalWrite(RS485_DE_PIN, RS485_RECEIVE);
  
  // --- 2. WAIT FOR REPLY ---
  unsigned long startTime = millis();
  bool responseReceived = false;
  String response = "";

  while (millis() - startTime < POLL_TIMEOUT) {
    if (RS485Serial.available()) {
      char c = RS485Serial.read();
      
      // Stop reading at newline
      if (c == '\n') {
        responseReceived = true;
        break;
      }
      
      // Accumulate characters (ignore carriage return)
      if (c != '\r') { 
        response += c;
      }
    }
  }

  // --- 3. PROCESS RESPONSE ---
  if (responseReceived) {
    response.trim();
    
    // Parse the response (Expected format: "A10:OCCUPIED" or "A10:VACANT")
    if (response.startsWith(id)) {
      if (response.indexOf("OCCUPIED") != -1) {
        Serial.println("Response: 🔴 OCCUPIED");
      } 
      else if (response.indexOf("VACANT") != -1) {
        Serial.println("Response: 🟢 VACANT");
      }
      else {
        Serial.print("Response: "); // Unknown status
        Serial.println(response);
      }
    } else {
      Serial.print("Error: ID Mismatch (Received: ");
      Serial.print(response);
      Serial.println(")");
    }
    
  } else {
    Serial.println("NO REPLY (Timeout)");
  }
}