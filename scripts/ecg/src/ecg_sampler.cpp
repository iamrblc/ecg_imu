#include "ecg_sampler.h"

#include <Arduino.h>
#include <math.h>

#include "config.h"

namespace {

// 50 Hz notch filter at 200 Hz sample rate.
constexpr float kNotchB0 = 0.95125f;
constexpr float kNotchB1 = 0.0f;
constexpr float kNotchB2 = 0.95125f;
constexpr float kNotchA1 = 0.0f;
constexpr float kNotchA2 = 0.9025f;

constexpr int kAdcMin = 0;
constexpr int kAdcMax = 4095;

}  // namespace

void EcgSampler::resetNotchState(NotchState& state) {
    state.x1 = 0.0f;
    state.x2 = 0.0f;
    state.y1 = 0.0f;
    state.y2 = 0.0f;
}

void EcgSampler::begin() {
    pinMode(Pins::kLoPlusPin, INPUT);
    pinMode(Pins::kLoMinusPin, INPUT);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    resetRecordingFilter();
    resetLiveFilter();
}

EcgSample EcgSampler::readSample(unsigned long elapsedTimeMs, uint64_t timestampUnixMs) {
    EcgSample sample{};
    sample.timestampUnixMs = timestampUnixMs;
    sample.elapsedTimeMs = elapsedTimeMs;
    sample.loPos = digitalRead(Pins::kLoPlusPin);
    sample.loNeg = digitalRead(Pins::kLoMinusPin);
    sample.ecgRaw = analogRead(Pins::kEcgPin);
    sample.ecgProcessed = processWithNotch(sample.ecgRaw, recordingFilterState_);
    sample.ecgProcessedLive = processWithNotch(sample.ecgRaw, liveFilterState_);
    return sample;
}

void EcgSampler::resetRecordingFilter() {
    resetNotchState(recordingFilterState_);
}

void EcgSampler::resetLiveFilter() {
    resetNotchState(liveFilterState_);
}

int EcgSampler::processWithNotch(int rawValue, NotchState& state) {
    const float x0 = static_cast<float>(rawValue);
    const float y0 =
        (kNotchB0 * x0)
        + (kNotchB1 * state.x1)
        + (kNotchB2 * state.x2)
        - (kNotchA1 * state.y1)
        - (kNotchA2 * state.y2);

    state.x2 = state.x1;
    state.x1 = x0;
    state.y2 = state.y1;
    state.y1 = y0;

    int output = static_cast<int>(lroundf(y0));
    if (output < kAdcMin) {
        output = kAdcMin;
    } else if (output > kAdcMax) {
        output = kAdcMax;
    }
    return output;
}