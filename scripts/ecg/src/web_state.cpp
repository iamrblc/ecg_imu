#include "web_state.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

// Static member initialization
bool WebState::recording = false;
RecordingMetadata WebState::metadata = {"", "", ""};
char WebState::currentFilename[128] = "";
EcgSample WebState::lastSample = {0, 0, 0, 0, 0, 0, 0};
bool WebState::timeSynced = false;

void WebState::begin() {
    recording = false;
    memset(&metadata, 0, sizeof(metadata));
    memset(currentFilename, 0, sizeof(currentFilename));
    timeSynced = false;
}

void WebState::disableInterrupts() {
    noInterrupts();
}

void WebState::enableInterrupts() {
    interrupts();
}

bool WebState::isRecording() {
    disableInterrupts();
    bool result = recording;
    enableInterrupts();
    return result;
}

void WebState::setRecording(bool value) {
    disableInterrupts();
    recording = value;
    enableInterrupts();
}

void WebState::toggleRecording() {
    disableInterrupts();
    recording = !recording;
    enableInterrupts();
}

void WebState::setMetadata(const RecordingMetadata& md) {
    disableInterrupts();
    memcpy(&metadata, &md, sizeof(RecordingMetadata));
    enableInterrupts();
}

RecordingMetadata WebState::getMetadata() {
    disableInterrupts();
    RecordingMetadata result = metadata;
    enableInterrupts();
    return result;
}

void WebState::setCurrentFilename(const char* filename) {
    disableInterrupts();
    strncpy(currentFilename, filename, sizeof(currentFilename) - 1);
    currentFilename[sizeof(currentFilename) - 1] = '\0';
    enableInterrupts();
}

void WebState::getCurrentFilename(char* buffer, size_t bufSize) {
    disableInterrupts();
    strncpy(buffer, currentFilename, bufSize - 1);
    buffer[bufSize - 1] = '\0';
    enableInterrupts();
}

void WebState::setLastSample(const EcgSample& sample) {
    disableInterrupts();
    lastSample = sample;
    enableInterrupts();
}

EcgSample WebState::getLastSample() {
    disableInterrupts();
    EcgSample result = lastSample;
    enableInterrupts();
    return result;
}

bool WebState::isTimeSynced() {
    disableInterrupts();
    bool result = timeSynced;
    enableInterrupts();
    return result;
}

void WebState::setTimeSynced(bool value) {
    disableInterrupts();
    timeSynced = value;
    enableInterrupts();
}

void WebState::generateFilename(char* buffer, size_t bufSize, uint64_t timestampMs) {
    disableInterrupts();
    RecordingMetadata md = metadata;
    enableInterrupts();

    // Convert timestampMs to date/time format: yymmddhhMM
    // timestampMs is Unix time in milliseconds
    uint32_t timestampS = timestampMs / 1000;
    
    // Calculate date/time from Unix timestamp (assumes UTC)
    // This is a simplified version; for production, use a proper time library
    time_t timeVal = (time_t)timestampS;
    struct tm* timeinfo = gmtime(&timeVal);
    
    char dateTimeStr[12];  // yymmddhhMM = 10 chars + null
    strftime(dateTimeStr, sizeof(dateTimeStr), "%y%m%d%H%M", timeinfo);
    
    // Format: userInput_yymmddhhMM.csv or yymmddhhMM.csv if no input
    if (md.userInputFilename[0] != '\0') {
        snprintf(buffer, bufSize, "%s_%s", md.userInputFilename, dateTimeStr);
    } else {
        strncpy(buffer, dateTimeStr, bufSize - 1);
    }
    buffer[bufSize - 1] = '\0';
}
