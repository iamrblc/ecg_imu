#include "csv_recorder.h"

#include <inttypes.h>
#include <SPI.h>
#include <ArduinoJson.h>

#include "config.h"
#include "web_state.h"

namespace {

void printUint64(File& file, uint64_t value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
    file.print(buffer);
}
}

bool CsvRecorder::begin(const char* filePath, bool overwrite) {
    if (!SD.begin(Pins::kSdCsPin)) {
        return false;
    }

    if (overwrite && SD.exists(filePath)) {
        SD.remove(filePath);
    }

    dataFile = SD.open(filePath, FILE_WRITE);
    bufferedRows = 0;
    return static_cast<bool>(dataFile);
}

bool CsvRecorder::writeHeader() {
    if (!dataFile) {
        return false;
    }

    dataFile.println("timestamp,time,ecg_raw,ecg_proc,lo_pos,lo_neg");
    dataFile.flush();
    return true;
}

bool CsvRecorder::writeSample(const EcgSample& sample) {
    if (!dataFile) {
        return false;
    }

    rowBuffer[bufferedRows] = sample;
    ++bufferedRows;

    if (bufferedRows >= RecordingConfig::kWriteBatchRows) {
        if (!flushBufferedRows()) {
            return false;
        }
    }

    return true;
}

void CsvRecorder::close() {
    if (dataFile) {
        flushBufferedRows();
        dataFile.flush();
        dataFile.close();
        bufferedRows = 0;
    }
}

bool CsvRecorder::isOpen() const {
    return static_cast<bool>(dataFile);
}

bool CsvRecorder::flushBufferedRows() {
    if (!dataFile) {
        return false;
    }

    for (unsigned int i = 0; i < bufferedRows; ++i) {
        const EcgSample& sample = rowBuffer[i];
        printUint64(dataFile, sample.timestampUnixMs);
        dataFile.print(",");
        dataFile.print(sample.elapsedTimeMs);
        dataFile.print(",");
        dataFile.print(sample.ecgRaw);
        dataFile.print(",");
        dataFile.print(sample.ecgProcessed);
        dataFile.print(",");
        dataFile.print(sample.loPos);
        dataFile.print(",");
        dataFile.println(sample.loNeg);
    }

    dataFile.flush();
    bufferedRows = 0;
    return true;
}

bool CsvRecorder::writeMetadataJson(const char* csvFilePath, uint32_t sampleCount, uint64_t startTimestamp, uint64_t endTimestamp) {
    // Convert CSV filename to JSON filename
    String jsonPath = String(csvFilePath);
    jsonPath.replace(".csv", ".json");

    // Open JSON file for writing
    File jsonFile = SD.open(jsonPath.c_str(), FILE_WRITE);
    if (!jsonFile) {
        Serial.print("Failed to open JSON file: ");
        Serial.println(jsonPath);
        return false;
    }

    // Get metadata from WebState
    RecordingMetadata md = WebState::getMetadata();

    // Build JSON document
    JsonDocument doc;
    doc["filename_user_input"] = md.userInputFilename;
    doc["filename_full"] = String(csvFilePath).substring(String(csvFilePath).lastIndexOf('/') + 1);
    doc["timestamp_start_unix"] = startTimestamp;
    doc["timestamp_end_unix"] = endTimestamp;
    doc["duration_seconds"] = (endTimestamp - startTimestamp) / 1000;
    doc["sample_rate_hz"] = 200;  // From config
    doc["samples_count"] = sampleCount;

    JsonObject metadata = doc["metadata"].to<JsonObject>();
    metadata["dog_id"] = md.dogId;
    metadata["experiment_id"] = md.experimentId;

    // Write JSON to file
    serializeJson(doc, jsonFile);
    jsonFile.close();

    Serial.print("Metadata JSON written: ");
    Serial.println(jsonPath);
    return true;
}