#pragma once

#include <stdint.h>
#include <cstddef>
#include "ecg_sampler.h"

// Global recording state shared between button, web interface, and recording modules
struct RecordingMetadata {
    char userInputFilename[64];      // User input from web form (e.g., "test_recording")
    char dogId[64];                  // Optional dog ID
    char experimentId[64];           // Optional experiment ID
};

class WebState {
public:
    // Initialize state
    static void begin();

    // Recording state getters/setters (thread-safe via interrupt disable)
    static bool isRecording();
    static void setRecording(bool value);
    static void toggleRecording();

    // Metadata getters/setters
    static void setMetadata(const RecordingMetadata& metadata);
    static RecordingMetadata getMetadata();

    // Current filename getters/setters
    static void setCurrentFilename(const char* filename);
    static void getCurrentFilename(char* buffer, size_t bufSize);

    // Last ECG sample getters/setters (for live streaming)
    static void setLastSample(const EcgSample& sample);
    static EcgSample getLastSample();

    // Time sync status
    static bool isTimeSynced();
    static void setTimeSynced(bool value);

    // Filename generation from metadata and current timestamp
    // Returns generated filename in format: "userInput_yymmddhhMM.csv" or "yymmddhhMM.csv" if no input
    static void generateFilename(char* buffer, size_t bufSize, uint64_t timestampMs);

private:
    static bool recording;
    static RecordingMetadata metadata;
    static char currentFilename[128];
    static EcgSample lastSample;
    static bool timeSynced;

    // Helper to disable/enable interrupts for thread-safety
    static void disableInterrupts();
    static void enableInterrupts();
};
