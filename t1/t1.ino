#include <SoftwareSerial.h>
#include <LiquidCrystal.h>
#include "MQ135.h"

// Pin Definitions
const int SENSOR_PIN = A0;
const int LED_PIN = 8;

// Constants
const int BAUD_RATE = 115200;
const int ESP_RX_PIN = 9;
const int ESP_TX_PIN = 10;
const int WIFI_TIMEOUT = 2000;
const int SERVER_PORT = 80;
const int LCD_COLUMNS = 16;
const int LCD_ROWS = 2;

// Global Variables
SoftwareSerial espSerial(ESP_RX_PIN, ESP_TX_PIN);
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Function Declarations
void sendData(String command, const int timeout, boolean debug);
String getAirQualityStatus(float ppm);

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);
  
  // LCD setup
  lcd.begin(LCD_COLUMNS, LCD_ROWS);
  lcd.setCursor(0, 0);
  lcd.print("circuitdigest");
  lcd.setCursor(0, 1);
  lcd.print("Sensor Warming");
  delay(1000);
  lcd.clear();
  
  // Serial communication setup
  Serial.begin(BAUD_RATE);
  espSerial.begin(BAUD_RATE);
  
  // ESP8266 setup
  sendData("AT+RST\r\n", WIFI_TIMEOUT, true); // Reset module
  sendData("AT+CWMODE=2\r\n", WIFI_TIMEOUT, true); // Configure as access point
  sendData("AT+CIFSR\r\n", WIFI_TIMEOUT, true); // Get IP address
  sendData("AT+CIPMUX=1\r\n", WIFI_TIMEOUT, true); // Configure for multiple connections
  sendData("AT+CIPSERVER=1," + String(SERVER_PORT) + "\r\n", WIFI_TIMEOUT, true); // Turn on server on port 80
}

void loop() {
  // Read air quality
  MQ135 gasSensor(SENSOR_PIN);
  float airQualityPPM = gasSensor.getPPM();
  
  // Update LCD
  lcd.setCursor(0, 0);
  lcd.print("Air Quality: ");
  lcd.print(airQualityPPM);
  lcd.print(" PPM");
  
  String status = getAirQualityStatus(airQualityPPM);
  lcd.setCursor(0, 1);
  lcd.print(status);
  
  // Update LED
  digitalWrite(LED_PIN, (status == "Danger! Move to Fresh Air") ? HIGH : LOW);
  
  // Check for incoming data from ESP8266
  if (espSerial.available()) {
    if (espSerial.find("+IPD,")) {
      delay(1000);
      int connectionId = espSerial.read() - '0'; // Convert ASCII to integer
      
      // Prepare HTTP response
      String response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
      response += "<h1>IOT Air Pollution Monitoring System</h1>";
      response += "<p>Air Quality is ";
      response += airQualityPPM;
      response += " PPM</p>";
      response += "<p>";
      response += status;
      response += "</p>";
      
      // Send response to client
      String cipSend = "AT+CIPSEND=";
      cipSend += connectionId;
      cipSend += ",";
      cipSend += response.length();
      cipSend += "\r\n";
      sendData(cipSend, WIFI_TIMEOUT, true);
      sendData(response, WIFI_TIMEOUT, true);
      
      // Close connection
      String closeCommand = "AT+CIPCLOSE=";
      closeCommand += connectionId;
      closeCommand += "\r\n";
      sendData(closeCommand, WIFI_TIMEOUT, true);
    }
  }
  
  delay(1000);
}

// Function to send data to ESP8266
void sendData(String command, const int timeout, boolean debug) {
  espSerial.print(command);
  long int startTime = millis();
  String response = "";
  while ((millis() - startTime) < timeout) {
    if (espSerial.available()) {
      char c = espSerial.read();
      response += c;
    }
  }
  if (debug) {
    Serial.print(response);
  }
}

// Function to get air quality status
String getAirQualityStatus(float ppm) {
  if (ppm <= 1000) {
    return "Fresh Air";
  } else if (ppm <= 2000) {
    return "Poor Air, Open Windows";
  } else {
    return "Danger! Move to Fresh Air";
  }
}