//Prevent double loading
#pragma once

//Libraries
#include <Arduino.h>
#include <SD.h>

//Own scripts
#include "ecg_sampler.h"

class CsvRecorder {
public:
    bool begin(const char* filePath);
    bool writeHeader();
    bool writeSample(const EcgSample& sample);
    void close();
    bool isOpen() const;

private:
    File dataFile;
};