//Prevent double loading
#pragma once

#include <stdint.h>

struct EcgSample {
    uint64_t timestampUnixMs;
    unsigned long elapsedTimeMs;
    int ecgRaw;
    int ecgProcessed;
    int loPos;
    int loNeg;
};

class EcgSampler {
public:
    void begin();
    EcgSample readSample(unsigned long elapsedTimeMs, uint64_t timestampUnixMs) const;

private:
    int processSample(int rawValue) const;
};