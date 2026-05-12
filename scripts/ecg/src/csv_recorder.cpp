#include "csv_recorder.h"

#include <inttypes.h>
#include <SPI.h>

#include "config.h"

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