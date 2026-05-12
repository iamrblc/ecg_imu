#include "ecg_sampler.h"

#include <Arduino.h>

#include "config.h"

void EcgSampler::begin() {
    pinMode(Pins::kLoPlusPin, INPUT);
    pinMode(Pins::kLoMinusPin, INPUT);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
}

EcgSample EcgSampler::readSample(unsigned long timestampMs) const {
    EcgSample sample{};
    sample.timestampMs = timestampMs;
    sample.loPlus = digitalRead(Pins::kLoPlusPin);
    sample.loMinus = digitalRead(Pins::kLoMinusPin);
    sample.ecgValue = analogRead(Pins::kEcgPin);
    return sample;
}