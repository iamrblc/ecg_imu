#include "record_control.h"

#include <Arduino.h>

#include "config.h"

namespace {
constexpr unsigned long kDebounceMs = 30;
constexpr unsigned long kBlinkOnMs = 180;
constexpr unsigned long kBlinkOffMs = 180;
}

void RecordControl::begin() {
    pinMode(Pins::kRecordButtonPin, INPUT_PULLUP);

    pinMode(Pins::kLedRedPin, OUTPUT);
    pinMode(Pins::kLedGreenPin, OUTPUT);
    pinMode(Pins::kLedBluePin, OUTPUT);

    lastRawPressed = (digitalRead(Pins::kRecordButtonPin) == LOW);
    stablePressed = lastRawPressed;
    lastChangeMs = millis();

    setRecording(false);
}

RecordControl::Event RecordControl::pollEvent(unsigned long nowMs) {
    const bool rawPressed = (digitalRead(Pins::kRecordButtonPin) == LOW);

    if (rawPressed != lastRawPressed) {
        lastRawPressed = rawPressed;
        lastChangeMs = nowMs;
    }

    if ((nowMs - lastChangeMs) < kDebounceMs || stablePressed == rawPressed) {
        return Event::None;
    }

    stablePressed = rawPressed;

    if (stablePressed && !recording) {
        return Event::StartRequested;
    }

    if (!stablePressed && recording) {
        return Event::StopRequested;
    }

    return Event::None;
}

void RecordControl::setRecording(bool isRecording) {
    recording = isRecording;
    setRecordingIndicator(recording);
}

bool RecordControl::isRecording() const {
    return recording;
}

bool RecordControl::isLatchPressed() const {
    return stablePressed;
}

void RecordControl::blinkStopPattern() {
    for (int i = 0; i < 3; ++i) {
        setRecordingIndicator(true);
        delay(kBlinkOnMs);
        setRecordingIndicator(false);
        delay(kBlinkOffMs);
    }
}

void RecordControl::setColor(bool redOn, bool greenOn, bool blueOn) {
    // RGB module is active-low (LOW turns channel on).
    digitalWrite(Pins::kLedRedPin, redOn ? LOW : HIGH);
    digitalWrite(Pins::kLedGreenPin, greenOn ? LOW : HIGH);
    digitalWrite(Pins::kLedBluePin, blueOn ? LOW : HIGH);
}

void RecordControl::setRecordingIndicator(bool on) {
    setColor(false, false, on);
}