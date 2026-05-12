#include <Arduino.h>

//Individual logic scripts
#include "csv_recorder.h"
#include "ecg_sampler.h"
#include "config.h"
#include "record_control.h"

EcgSampler ecgSampler;
CsvRecorder csvRecorder;
RecordControl recordControl;
unsigned long startTime;

namespace {

bool startRecording() {
    Serial.println("Initializing SD card...");

    if (!csvRecorder.begin(RecordingConfig::kDefaultFilePath, true)) {
        Serial.println("SD initialization failed!");
        return false;
    }

    Serial.println("SD initialization successful!");

    if (!csvRecorder.writeHeader()) {
        Serial.println("Failed to open file!");
        csvRecorder.close();
        return false;
    }

    startTime = millis();
    recordControl.setRecording(true);
    Serial.println("Recording started.");
    return true;
}

void stopRecording() {
    if (!recordControl.isRecording()) {
        return;
    }

    const unsigned long elapsedTime = millis() - startTime;
    csvRecorder.close();
    recordControl.setRecording(false);

    Serial.println("Recording stopped.");
    Serial.print("Total samples collected: ");
    Serial.println(elapsedTime / RecordingConfig::kSampleIntervalMs);
    recordControl.blinkStopPattern();
}
}

void setup() {
    delay(3000);  // Give serial monitor time to connect

    Serial.begin(115200);

    ecgSampler.begin();
    recordControl.begin();

    Serial.println("System ready. Push latch in to start recording.");

    if (recordControl.isLatchPressed()) {
        startRecording();
    }
}

void loop() {
    const unsigned long nowMs = millis();

    switch (recordControl.pollEvent(nowMs)) {
        case RecordControl::Event::StartRequested:
            startRecording();
            break;
        case RecordControl::Event::StopRequested:
            stopRecording();
            break;
        case RecordControl::Event::None:
            break;
    }

    if (!recordControl.isRecording()) {
        delay(5);
        return;
    }

    const unsigned long elapsedTime = nowMs - startTime;
    EcgSample sample = ecgSampler.readSample(elapsedTime);

    if (!csvRecorder.writeSample(sample)) {
        Serial.println("Failed to write ECG sample!");
        stopRecording();
        delay(100);
        return;
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
