#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

const int SD_CS = D10;

void setup() {
    delay(3000);
    Serial.begin(115200);

    Serial.println("Initializing SD card...");

    if (!SD.begin(SD_CS)) {
        Serial.println("SD initialization failed!");
        return;
    }

    Serial.println("SD initialization successful!");

    File testFile = SD.open("/test.txt", FILE_WRITE);

    if (testFile) {
        testFile.println("Hello from ESP32");
        testFile.close();

        Serial.println("File written.");
    } else {
        Serial.println("Failed to open file.");
    }
}

void loop() {
}