#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

// SIM800L on D4 (TX), D5 (RX)
SoftwareSerial SIM800(D5, D4);

// GPS on D6 (TX), D7 (RX)
SoftwareSerial gpsSerial(D7, D6);

TinyGPSPlus gps;

String smsBuffer = "";
String cmd = "";
String senderNumber = "+639541045141";
unsigned long lastGPSRead = 0;
bool networkConnected = false;

void showGPS();
void sendGPSviaSMS();
void sendSMS(String message);
void processSMS();

void setup() {
  Serial.begin(9600);
  SIM800.begin(9600);
  gpsSerial.begin(9600);
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH); // LED off initially

  Serial.println("\n=== GPS Tracker ===");
  Serial.println("Initializing SIM800L...");
  delay(8000); // Wait for module to be ready
  
  SIM800.listen();
  
  // Basic AT commands
  Serial.println("Sending AT commands...");
  SIM800.println("AT");
  delay(1000);
  
  SIM800.println("AT+CMGF=1");  // Text mode
  delay(1000);
  
  // SMS notification - deliver directly to serial
  SIM800.println("AT+CNMI=2,2,0,0,0");
  delay(1000);
  
  // Delete all SMS to free memory
  SIM800.println("AT+CMGDA=\"DEL ALL\"");
  delay(2000);
  
  // Check network once at startup
  Serial.println("Checking network...");
  SIM800.println("AT+CREG?");
  delay(1000);
  
  String response = "";
  while (SIM800.available()) {
    response += (char)SIM800.read();
  }
  
  if (response.indexOf("+CREG: 0,1") >= 0 || response.indexOf("+CREG: 0,5") >= 0) {
    networkConnected = true;
    digitalWrite(2, LOW);  // LED ON
    Serial.println("✓ Network connected");
  } else {
    Serial.println("✗ Network not connected");
    Serial.println(response);
  }

  Serial.println("\n=== READY ===");
  Serial.println("Send 'CHECK' via SMS to get location");
}

void loop() {
  // READ GPS periodically (every 200ms for 100ms)
  if (millis() - lastGPSRead > 200) {
    gpsSerial.listen();
    unsigned long start = millis();
    while (millis() - start < 100) {
      if (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
      }
    }
    lastGPSRead = millis();
  }

  // MAIN FOCUS: Listen to SIM800 for SMS
  SIM800.listen();
  
  // Check for incoming data
  if (SIM800.available()) {
    unsigned long startRead = millis();
    
    // Read all available data with timeout
    while (millis() - startRead < 3000) { // 3 second timeout
      if (SIM800.available()) {
        char c = SIM800.read();
        Serial.write(c); // Echo to serial
        smsBuffer += c;
        startRead = millis(); // Reset timeout when data arrives
      }
      
      // If we see +CMT: header, wait for complete message
      if (smsBuffer.indexOf("+CMT:") >= 0) {
        delay(1000); // Wait for message body
        // Read remaining data
        while (SIM800.available()) {
          char c = SIM800.read();
          Serial.write(c);
          smsBuffer += c;
        }
        // Process the complete SMS
        processSMS();
        break;
      }
      
      // Prevent overflow
      if (smsBuffer.length() > 300) {
        processSMS(); // Try to process anyway
        break;
      }
    }
  }

  // SERIAL COMMANDS (non-blocking)
  if (Serial.available()) {
    char c = Serial.read();
    cmd += c;

    if (c == '\n') {
      cmd.trim();
      // Only CHECK command is allowed
      if (cmd.equalsIgnoreCase("CHECK")) {
        // Simulate SMS CHECK command
        processSMS();
      }
      cmd = "";
    }
  }
}

void processSMS() {
  Serial.println("\n\n=== PROCESSING SMS ===");
  Serial.print("Buffer length: ");
  Serial.println(smsBuffer.length());
  
  Serial.println("--- Buffer Content ---");
  Serial.println(smsBuffer);
  Serial.println("--- End Buffer ---");
  
  // Convert to uppercase for comparison
  String upperBuffer = smsBuffer;
  upperBuffer.toUpperCase();
  
  // Look for CHECK anywhere in the message
  if (upperBuffer.indexOf("CHECK") >= 0) {
    Serial.println("\n✓✓✓ CHECK COMMAND DETECTED! ✓✓✓");
    
    // Extract sender number from +CMT: line
    int cmtIndex = smsBuffer.indexOf("+CMT:");
    if (cmtIndex >= 0) {
      int firstQuote = smsBuffer.indexOf("\"", cmtIndex);
      int secondQuote = smsBuffer.indexOf("\"", firstQuote + 1);
      if (firstQuote >= 0 && secondQuote > firstQuote) {
        String extractedNumber = smsBuffer.substring(firstQuote + 1, secondQuote);
        if (extractedNumber.length() >= 10) {
          senderNumber = extractedNumber;
          Serial.print("Reply to: ");
          Serial.println(senderNumber);
        }
      }
    }
    
    // Send GPS location
    delay(500);
    sendGPSviaSMS();
    
  } else {
    Serial.println("✗ No CHECK command found");
  }
  
  // Clear buffer
  smsBuffer = "";
  Serial.println("=== SMS Processing Done ===\n");
}

void showGPS() {
  Serial.println("\n=== GPS Status ===");
  if (gps.location.isValid()) {
    Serial.print("https://maps.google.com/?q=");
    Serial.print(gps.location.lat(), 6);
    Serial.print(",");
    Serial.println(gps.location.lng(), 6);

    Serial.print("Speed: ");
    Serial.print(gps.speed.kmph(), 1);
    Serial.println(" km/h");
    
    Serial.print("Altitude: ");
    Serial.print(gps.altitude.meters(), 1);
    Serial.println(" m");
  } else {
    Serial.println("Waiting for GPS fix...");
  }

  Serial.print("Satellites: ");
  Serial.print(gps.satellites.value());
  Serial.print(" | HDOP: ");
  Serial.println(gps.hdop.value());
}

void sendGPSviaSMS() {
  String message;

  Serial.println("\n📍 Preparing GPS message...");
  
  if (gps.location.isValid()) {
    message = String(gps.location.lat(), 6);
    message += ",";
    message += String(gps.location.lng(), 6);
  } else {
    message = "No GPS fix. Sats: ";
    message += String(gps.satellites.value());
  }

  sendSMS(message);
}

void sendSMS(String message) {
  Serial.println("\n📤 Sending SMS...");
  Serial.print("To: ");
  Serial.println(senderNumber);
  Serial.print("Msg: ");
  Serial.println(message);

  SIM800.listen();
  delay(200);
  
  SIM800.println("AT+CMGF=1");
  delay(500);

  SIM800.print("AT+CMGS=\"");
  SIM800.print(senderNumber);
  SIM800.println("\"");
  delay(1000);

  SIM800.print(message);
  delay(200);
  SIM800.write(26); // CTRL+Z
  
  Serial.println("Waiting for send confirmation...");
  delay(5000);
  
  // Read response
  while (SIM800.available()) {
    Serial.write(SIM800.read());
  }
  
  Serial.println("\n✓ SMS Sent!");
}