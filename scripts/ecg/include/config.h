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
constexpr unsigned int kWriteBatchRows = 50;
}

namespace WifiConfig {
//constexpr const char* kSsid = "OnePlus8";
//constexpr const char* kPassword = "m4mp79k5";
constexpr const char* kSsid = "Vodafone-5AB5";
constexpr const char* kPassword = "aNQ27yx5ewpeaeh5";
constexpr unsigned long kConnectTimeoutMs = 30000;
constexpr const char* kNtpServerPrimary = "pool.ntp.org";
constexpr const char* kNtpServerSecondary = "time.nist.gov";
constexpr unsigned long kNtpSyncTimeoutMs = 10000;
}

namespace WebServerConfig {
constexpr const char* kMdnsHostname = "caninecg";
constexpr uint16_t kPort = 80;
constexpr unsigned long kWebSocketUpdateIntervalMs = 33;  // ~30 Hz
constexpr const char* kRecordingsPath = "/recordings";
}