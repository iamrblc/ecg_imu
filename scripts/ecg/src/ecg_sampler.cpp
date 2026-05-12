#include "ecg_sampler.h"

#include <Arduino.h>

#include "config.h"

void EcgSampler::begin() {
    pinMode(Pins::kLoPlusPin, INPUT);
    pinMode(Pins::kLoMinusPin, INPUT);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
}

EcgSample EcgSampler::readSample(unsigned long elapsedTimeMs, uint64_t timestampUnixMs) const {
    EcgSample sample{};
    sample.timestampUnixMs = timestampUnixMs;
    sample.elapsedTimeMs = elapsedTimeMs;
    sample.loPos = digitalRead(Pins::kLoPlusPin);
    sample.loNeg = digitalRead(Pins::kLoMinusPin);
    sample.ecgRaw = analogRead(Pins::kEcgPin);
    sample.ecgProcessed = processSample(sample.ecgRaw);
    return sample;
}

int EcgSampler::processSample(int rawValue) const {
    // Placeholder processing step: currently pass-through.
    return rawValue;
}