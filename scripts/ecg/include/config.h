//Prevent double loading
#pragma once

//Includes
#include <Arduino.h>

//Set constants with constexpr (must be computable at compile time, unlike const)
namespace Pins {
constexpr int kEcgPin = A0;         //k prefix is just convention
constexpr int kLoPlusPin = D2;
constexpr int kLoMinusPin = D3;
constexpr int kRecordButtonPin = D4;
constexpr int kSdCsPin = D10;
constexpr int kLedRedPin = 14;
constexpr int kLedGreenPin = 15;
constexpr int kLedBluePin = 16;
}

namespace RecordingConfig {
constexpr int kSampleIntervalMs = 5; //200 Hz => 1000 ms / 200
constexpr unsigned long kCollectionTimeMs = 60000;
constexpr const char* kDefaultFilePath = "/ecg_data.csv";
}