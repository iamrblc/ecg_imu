//Prevent double loading
#pragma once

struct EcgSample {
    unsigned long timestampMs;
    int ecgValue;
    int loPlus;
    int loMinus;
};

class EcgSampler {
public:
    void begin();
    EcgSample readSample(unsigned long timestampMs) const;
};