#include <Arduino.h>
#include <sys/time.h>
#include <time.h>
#include <WiFi.h>

//Individual logic scripts
#include "csv_recorder.h"
#include "ecg_sampler.h"
#include "config.h"
#include "record_control.h"

EcgSampler ecgSampler;
CsvRecorder csvRecorder;
RecordControl recordControl;
unsigned long startTime;
unsigned long nextSampleDueMs;
bool gHasUnixTime = false;

namespace {

constexpr time_t kMinValidUnixTime = 1704067200;

const char* wifiStatusToString(wl_status_t status) {
    switch (status) {
        case WL_IDLE_STATUS:
            return "WL_IDLE_STATUS";
        case WL_NO_SSID_AVAIL:
            return "WL_NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED:
            return "WL_SCAN_COMPLETED";
        case WL_CONNECTED:
            return "WL_CONNECTED";
        case WL_CONNECT_FAILED:
            return "WL_CONNECT_FAILED";
        case WL_CONNECTION_LOST:
            return "WL_CONNECTION_LOST";
        case WL_DISCONNECTED:
            return "WL_DISCONNECTED";
        default:
            return "WL_STATUS_UNKNOWN";
    }
}

uint64_t getUnixTimeMs() {
    if (!gHasUnixTime) {
        return 0;
    }

    timeval now{};
    gettimeofday(&now, nullptr);
    return static_cast<uint64_t>(now.tv_sec) * 1000ULL
        + static_cast<uint64_t>(now.tv_usec / 1000ULL);
}

bool syncUnixTime() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Skipping NTP sync: WiFi is not connected.");
        return false;
    }

    Serial.println("Syncing Unix time with NTP...");
    configTime(0, 0, WifiConfig::kNtpServerPrimary, WifiConfig::kNtpServerSecondary);

    const unsigned long startedAtMs = millis();
    time_t now = time(nullptr);
    while (now < kMinValidUnixTime && (millis() - startedAtMs) < WifiConfig::kNtpSyncTimeoutMs) {
        delay(250);
        Serial.print("#");
        now = time(nullptr);
    }
    Serial.println();

    if (now < kMinValidUnixTime) {
        Serial.println("NTP sync failed. Recording will continue with elapsed time only.");
        return false;
    }

    gHasUnixTime = true;
    Serial.print("Unix time synced. Current epoch seconds: ");
    Serial.println(static_cast<unsigned long>(now));
    return true;
}

bool connectToWifi() {
    if (WifiConfig::kSsid[0] == '\0') {
        Serial.println("WiFi skipped: SSID not configured.");
        return false;
    }

    Serial.print("Connecting to WiFi: ");
    Serial.println(WifiConfig::kSsid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WifiConfig::kSsid, WifiConfig::kPassword);

    const unsigned long startedAtMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startedAtMs) < WifiConfig::kConnectTimeoutMs) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    const wl_status_t status = WiFi.status();
    if (status != WL_CONNECTED) {
        Serial.print("WiFi connection failed. Status=");
        Serial.print(static_cast<int>(status));
        Serial.print(" (");
        Serial.print(wifiStatusToString(status));
        Serial.println("). Continuing without network time.");
        Serial.print("Reported IP after timeout: ");
        Serial.println(WiFi.localIP());
        return false;
    }

    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
}

bool startRecording() {
    Serial.println("Initializing SD card...");

    if (!csvRecorder.begin(RecordingConfig::kDefaultFilePath, true)) {
        Serial.println("SD initialization failed!");
        return false;
    }

    Serial.println("SD initialization successful!");

    if (!csvRecorder.writeHeader()) {
        Serial.println("Failed to open file!");
        csvRecorder.close();
        return false;
    }

    startTime = millis();
    nextSampleDueMs = startTime;
    recordControl.setRecording(true);
    Serial.println("Recording started.");
    return true;
}

void stopRecording() {
    if (!recordControl.isRecording()) {
        return;
    }

    const unsigned long elapsedTime = millis() - startTime;
    csvRecorder.close();
    recordControl.setRecording(false);

    Serial.println("Recording stopped.");
    Serial.print("Total samples collected: ");
    Serial.println(elapsedTime / RecordingConfig::kSampleIntervalMs);
    recordControl.blinkStopPattern();
}
}

void setup() {
    delay(3000);  // Give serial monitor time to connect

    Serial.begin(115200);
    const bool wifiConnected = connectToWifi();
    if (wifiConnected) {
        syncUnixTime();
    }

    ecgSampler.begin();
    recordControl.begin();

    Serial.println("System ready. Push latch in to start recording.");

    if (recordControl.isLatchPressed()) {
        startRecording();
    }
}

void loop() {
    const unsigned long nowMs = millis();

    switch (recordControl.pollEvent(nowMs)) {
        case RecordControl::Event::StartRequested:
            startRecording();
            break;
        case RecordControl::Event::StopRequested:
            stopRecording();
            break;
        case RecordControl::Event::None:
            break;
    }

    if (!recordControl.isRecording()) {
        delay(5);
        return;
    }

    if (nowMs < nextSampleDueMs) {
        delay(1);
        return;
    }

    const unsigned long elapsedTime = nextSampleDueMs - startTime;
    EcgSample sample = ecgSampler.readSample(elapsedTime, getUnixTimeMs());
    nextSampleDueMs += RecordingConfig::kSampleIntervalMs;

    if (nowMs > nextSampleDueMs) {
        nextSampleDueMs = nowMs + RecordingConfig::kSampleIntervalMs;
    }

    if (!csvRecorder.writeSample(sample)) {
        Serial.println("Failed to write ECG sample!");
        stopRecording();
        delay(100);
        return;
    }

    // Print to serial for monitoring
    Serial.print("timestamp=");
    Serial.print(static_cast<unsigned long>(sample.timestampUnixMs / 1000ULL));
    Serial.print(" time=");
    Serial.print(sample.elapsedTimeMs);
    Serial.print("ms ecg_raw=");
    Serial.print(sample.ecgRaw);
    Serial.print(" ecg_proc=");
    Serial.print(sample.ecgProcessed);
    Serial.print(" lo_pos=");
    Serial.print(sample.loPos);
    Serial.print(" lo_neg=");
    Serial.println(sample.loNeg);
}
