/*
set_time.ino
This is a utility that sets the time on a PCF2129 and reads what it currently has stored.

To use, open in arduino ide (sorry not sorry, was too fast to code here and works well)
and flash the ESP. Connect to the serial monitor and the current stored time will be read out. 
To update the time, send it as a serial message e.g "2026-06-19 20:47:00" to set the time I wrote this. 

Written by DHartley for Sheffield Formula Racing 2026
*/

#include <Wire.h>

#define PCF2129_ADDR 0x51

unsigned long lastReadTime = 0;

// Convert decimal to binary coded decimal
byte decToBcd(byte val) {
  return ((val / 10 * 16) + (val % 10));
}

// Convert binary coded decimal to decimal
byte bcdToDec(byte val) {
  return ((val / 16 * 10) + (val % 16));
}

void setup() {
  Wire.begin(21, 2);
  Serial.begin(115200);
  while (!Serial); // Wait for serial monitor to open

  Serial.println("--- PCF2129 RTC Monitor ---");
  Serial.println("Reading stored time... (To set new time, send: YYYY-MM-DD HH:MM:SS)");
  Serial.println("------------------------------------");
}

void loop() {
  if (Serial.available() > 0) { //If there is a message, set time
    String inputString = Serial.readStringUntil('\n');
    inputString.trim(); // Clean up loose spaces or newlines

    // Process if it matches the 19-character timestamp format
    if (inputString.length() >= 19) {
      int fullYear = inputString.substring(0, 4).toInt();
      int month    = inputString.substring(5, 7).toInt();
      int day      = inputString.substring(8, 10).toInt();
      int hour     = inputString.substring(11, 13).toInt();
      int minute   = inputString.substring(14, 16).toInt();
      int second   = inputString.substring(17, 19).toInt();
      int year     = fullYear % 100; 
      
      Wire.beginTransmission(PCF2129_ADDR);
      Wire.write(0x03); // Start at Seconds register
      Wire.write(decToBcd(second));
      Wire.write(decToBcd(minute));
      Wire.write(decToBcd(hour));
      Wire.write(decToBcd(day));
      Wire.write(decToBcd(1)); // Weekday placeholder
      Wire.write(decToBcd(month));
      Wire.write(decToBcd(year));
      Wire.endTransmission();

      // Clear OSF (Oscillator Stop Flag) to make sure clock runs
      Wire.beginTransmission(PCF2129_ADDR);
      Wire.write(0x03); 
      Wire.endTransmission();
      Wire.requestFrom(PCF2129_ADDR, 1);
      byte secReg = Wire.read();
      
      secReg &= 0x7F; // Clear Bit 7
      
      Wire.beginTransmission(PCF2129_ADDR);
      Wire.write(0x03);
      Wire.write(secReg);
      Wire.endTransmission();

      Serial.println("\n>>> SUCCESS: RTC time updated! <<< \n");
    } else {
      Serial.println("\n>>> ERROR: Wrong format. Use YYYY-MM-DD HH:MM:SS <<<\n");
    }
  }

  //Read stored time every second
  if (millis() - lastReadTime >= 1000) {
    lastReadTime = millis();

    Wire.beginTransmission(PCF2129_ADDR);
    Wire.write(0x03); 
    uint8_t err = Wire.endTransmission();

    if (err == 0) {
      Wire.requestFrom(PCF2129_ADDR, 7);

      if (Wire.available() == 7) {
        byte seconds = bcdToDec(Wire.read() & 0x7F); 
        byte minutes = bcdToDec(Wire.read());
        byte hours   = bcdToDec(Wire.read() & 0x3F); 
        byte days    = bcdToDec(Wire.read() & 0x3F);
        Wire.read(); // Skip weekday byte
        byte month   = bcdToDec(Wire.read() & 0x1F);
        byte year    = bcdToDec(Wire.read());

        // Print date and time out to the Serial monitor
        Serial.print("RTC Time: 20");
        if (year < 10) Serial.print("0");
        Serial.print(year);
        Serial.print("-");
        if (month < 10) Serial.print("0");
        Serial.print(month);
        Serial.print("-");
        if (days < 10) Serial.print("0");
        Serial.print(days);
        Serial.print(" ");
        if (hours < 10) Serial.print("0");
        Serial.print(hours);
        Serial.print(":");
        if (minutes < 10) Serial.print("0");
        Serial.print(minutes);
        Serial.print(":");
        if (seconds < 10) Serial.print("0");
        Serial.println(seconds);
      }
    } else {
      Serial.println("I2C Error: Could not communicate with PCF2129 chip.");
    }
  }
}
