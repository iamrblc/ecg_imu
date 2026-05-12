//Prevent double loading
#pragma once

//Libraries
#include <Arduino.h>
#include <SD.h>

//Own scripts
#include "config.h"
#include "ecg_sampler.h"

class CsvRecorder {
public:
    bool begin(const char* filePath, bool overwrite = false);
    bool writeHeader();
    bool writeSample(const EcgSample& sample);
    void close();
    bool isOpen() const;
    
    // Write metadata JSON file with same name as CSV
    bool writeMetadataJson(const char* csvFilePath, uint32_t sampleCount, uint64_t startTimestamp, uint64_t endTimestamp);

private:
    bool flushBufferedRows();

    File dataFile;
    EcgSample rowBuffer[RecordingConfig::kWriteBatchRows]{};
    unsigned int bufferedRows = 0;
};