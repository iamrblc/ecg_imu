#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

const int SD_CS = D10;

File dataFile;

void setup() {
    delay(3000);

    Serial.begin(115200);

    Serial.println("Initializing SD card...");

    if (!SD.begin(SD_CS)) {
        Serial.println("SD initialization failed!");
        while (true);
    }

    Serial.println("SD initialization successful!");

    dataFile = SD.open("/data.csv", FILE_WRITE);

    if (!dataFile) {
        Serial.println("Failed to open file!");
        while (true);
    }

    dataFile.println("time_ms");
    dataFile.flush();

    Serial.println("CSV header written.");
}

void loop() {
    for (int i = 0; i < 10; i++) {
        unsigned long t = millis();

        dataFile.println(t);
        dataFile.flush();

        Serial.print("Wrote: ");
        Serial.println(t);

        delay(1000);
    }

    dataFile.close();

    Serial.println("Done writing.");

    while (true);
}