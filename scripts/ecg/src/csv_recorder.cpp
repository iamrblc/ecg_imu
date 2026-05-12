#include "csv_recorder.h"

#include <SPI.h>

#include "config.h"

bool CsvRecorder::begin(const char* filePath, bool overwrite) {
    if (!SD.begin(Pins::kSdCsPin)) {
        return false;
    }

    if (overwrite && SD.exists(filePath)) {
        SD.remove(filePath);
    }

    dataFile = SD.open(filePath, FILE_WRITE);
    return static_cast<bool>(dataFile);
}

bool CsvRecorder::writeHeader() {
    if (!dataFile) {
        return false;
    }

    dataFile.println("timestamp_ms,ecg_value,lo_plus,lo_minus");
    dataFile.flush();
    return true;
}

bool CsvRecorder::writeSample(const EcgSample& sample) {
    if (!dataFile) {
        return false;
    }

    dataFile.print(sample.timestampMs);
    dataFile.print(",");
    dataFile.print(sample.ecgValue);
    dataFile.print(",");
    dataFile.print(sample.loPlus);
    dataFile.print(",");
    dataFile.println(sample.loMinus);
    dataFile.flush();
    return true;
}

void CsvRecorder::close() {
    if (dataFile) {
        dataFile.close();
    }
}

bool CsvRecorder::isOpen() const {
    return static_cast<bool>(dataFile);
}