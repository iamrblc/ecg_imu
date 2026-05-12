#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

// ECG Module Pins
const int ECG_PIN = A0;
const int LO_PLUS = D2;
const int LO_MINUS = D3;

// SD Card Module Pin
const int SD_CS = D10;

// Data collection parameters
const int SAMPLE_RATE = 200;  // Hz
const unsigned long SAMPLE_INTERVAL = 1000 / SAMPLE_RATE;  // milliseconds (5 ms)
const unsigned long COLLECTION_TIME = 60000;  // 1 minute in milliseconds

File dataFile;
unsigned long startTime;

void setup() {
    delay(3000);  // Give serial monitor time to connect

    Serial.begin(115200);
    
    // Initialize ECG pins
    pinMode(LO_PLUS, INPUT);
    pinMode(LO_MINUS, INPUT);
    
    // Configure ADC
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    
    Serial.println("Initializing SD card...");
    
    // Initialize SD card
    if (!SD.begin(SD_CS)) {
        Serial.println("SD initialization failed!");
        while (true);
    }
    
    Serial.println("SD initialization successful!");
    
    // Open/create CSV file
    dataFile = SD.open("/ecg_data.csv", FILE_WRITE);
    
    if (!dataFile) {
        Serial.println("Failed to open file!");
        while (true);
    }
    
    // Write CSV header
    dataFile.println("timestamp_ms,ecg_value,lo_plus,lo_minus");
    dataFile.flush();
    
    Serial.println("CSV header written. Starting ECG data collection...");
    Serial.println("Collecting data for 60 seconds at 200 Hz...");
    
    startTime = millis();
}

void loop() {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - startTime;
    
    // Check if collection time is complete
    if (elapsedTime >= COLLECTION_TIME) {
        dataFile.close();
        Serial.println("\nData collection complete!");
        Serial.print("Total samples collected: ");
        Serial.println(elapsedTime / SAMPLE_INTERVAL);
        while (true);  // Stop here
    }
    
    // Read sensor values
    int loPlus = digitalRead(LO_PLUS);
    int loMinus = digitalRead(LO_MINUS);
    int ecgValue = analogRead(ECG_PIN);
    
    // Write data to SD card
    dataFile.print(elapsedTime);
    dataFile.print(",");
    dataFile.print(ecgValue);
    dataFile.print(",");
    dataFile.print(loPlus);
    dataFile.print(",");
    dataFile.println(loMinus);
    dataFile.flush();
    
    // Print to serial for monitoring
    Serial.print("t=");
    Serial.print(elapsedTime);
    Serial.print("ms ecg=");
    Serial.print(ecgValue);
    Serial.print(" lo+=");
    Serial.print(loPlus);
    Serial.print(" lo-=");
    Serial.println(loMinus);
    
    // Wait for next sample (5 ms for 200 Hz)
    delay(SAMPLE_INTERVAL);
}
