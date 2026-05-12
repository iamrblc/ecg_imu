#include <Arduino.h>

//Individual logic scripts
#include "csv_recorder.h"
#include "ecg_sampler.h"
#include "config.h"

EcgSampler ecgSampler;
CsvRecorder csvRecorder;
unsigned long startTime;

void setup() {
    delay(3000);  // Give serial monitor time to connect

    Serial.begin(115200);

    ecgSampler.begin();
    
    Serial.println("Initializing SD card...");

    if (!csvRecorder.begin(RecordingConfig::kDefaultFilePath)) {
        Serial.println("SD initialization failed!");
        while (true);
    }
    
    Serial.println("SD initialization successful!");

    if (!csvRecorder.writeHeader()) {
        Serial.println("Failed to open file!");
        while (true);
    }
    
    Serial.println("CSV header written. Starting ECG data collection...");
    Serial.println("Collecting data for 60 seconds at 200 Hz...");
    
    startTime = millis();
}

void loop() {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - startTime;
    
    // Check if collection time is complete
    if (elapsedTime >= RecordingConfig::kCollectionTimeMs) {
        csvRecorder.close();
        Serial.println("\nData collection complete!");
        Serial.print("Total samples collected: ");
        Serial.println(elapsedTime / RecordingConfig::kSampleIntervalMs);
        while (true);  // Stop here
    }
    
    EcgSample sample = ecgSampler.readSample(elapsedTime);

    if (!csvRecorder.writeSample(sample)) {
        Serial.println("Failed to write ECG sample!");
        csvRecorder.close();
        while (true);
    }
    
    // Print to serial for monitoring
    Serial.print("t=");
    Serial.print(sample.timestampMs);
    Serial.print("ms ecg=");
    Serial.print(sample.ecgValue);
    Serial.print(" lo+=");
    Serial.print(sample.loPlus);
    Serial.print(" lo-=");
    Serial.println(sample.loMinus);
    
    // Wait for next sample (5 ms for 200 Hz)
    delay(RecordingConfig::kSampleIntervalMs);
}
