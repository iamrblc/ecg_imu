//Prevent double loading
#pragma once

#include <stdint.h>

struct EcgSample {
    uint64_t timestampUnixMs;
    unsigned long elapsedTimeMs;
    int ecgRaw;
    int ecgProcessed;
    int ecgProcessedLive;
    int loPos;
    int loNeg;
};

class EcgSampler {
public:
    void begin();
    EcgSample readSample(unsigned long elapsedTimeMs, uint64_t timestampUnixMs);

    void resetRecordingFilter();
    void resetLiveFilter();

private:
    struct NotchState {
        float x1;
        float x2;
        float y1;
        float y2;
    };

    int processWithNotch(int rawValue, NotchState& state);
    static void resetNotchState(NotchState& state);

    NotchState recordingFilterState_{};
    NotchState liveFilterState_{};
};