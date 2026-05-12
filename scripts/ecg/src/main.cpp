#include <Arduino.h>
#include <sys/time.h>
#include <time.h>
#include <WiFi.h>
#include <SD.h>

//Individual logic scripts
#include "csv_recorder.h"
#include "ecg_sampler.h"
#include "config.h"
#include "record_control.h"
#include "web_state.h"
#include "web_server.h"

EcgSampler ecgSampler;
CsvRecorder csvRecorder;
RecordControl recordControl;
unsigned long startTime;
unsigned long nextSampleDueMs;
bool gHasUnixTime = false;
uint32_t gSampleCount = 0;
uint64_t gRecordingStartTimestamp = 0;

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

    if (!SD.begin(Pins::kSdCsPin)) {
        Serial.println("SD initialization failed!");
        return false;
    }

    // Create /recordings directory if it doesn't exist
    if (!SD.exists(WebServerConfig::kRecordingsPath)) {
        if (!SD.mkdir(WebServerConfig::kRecordingsPath)) {
            Serial.println("Failed to create /recordings directory!");
            return false;
        }
    }

    Serial.println("SD initialization successful!");

    // Generate filename from metadata and timestamp
    char filename[128];
    uint64_t timestamp = gHasUnixTime ? getUnixTimeMs() : 0;
    WebState::generateFilename(filename, sizeof(filename), timestamp);
    
    // Build full path: /recordings/filename.csv
    String filePath = String(WebServerConfig::kRecordingsPath) + "/" + filename + ".csv";
    
    if (!csvRecorder.begin(filePath.c_str(), true)) {
        Serial.print("Failed to open recording file: ");
        Serial.println(filePath);
        return false;
    }

    Serial.println("SD initialization successful!");

    if (!csvRecorder.writeHeader()) {
        Serial.println("Failed to write header!");
        csvRecorder.close();
        return false;
    }

    startTime = millis();
    nextSampleDueMs = startTime;
    gSampleCount = 0;
    gRecordingStartTimestamp = gHasUnixTime ? getUnixTimeMs() : 0;
    
    // Update WebState with current filename
    WebState::setCurrentFilename(filename);
    WebState::setRecording(true);
    
    recordControl.setRecording(true);
    Serial.printf("Recording started: %s\n", filePath.c_str());
    return true;
}

void stopRecording() {
    if (!csvRecorder.isOpen()) {
        return;
    }

    const unsigned long elapsedTime = millis() - startTime;
    const uint64_t endTimestamp = gHasUnixTime ? getUnixTimeMs() : 0;
    
    // Get the current filename to create metadata JSON
    char filename[128];
    WebState::getCurrentFilename(filename, sizeof(filename));
    String filePath = String(WebServerConfig::kRecordingsPath) + "/" + filename + ".csv";
    
    // Write metadata JSON before closing
    csvRecorder.writeMetadataJson(filePath.c_str(), gSampleCount, gRecordingStartTimestamp, endTimestamp);
    
    csvRecorder.close();
    WebState::setRecording(false);
    recordControl.setRecording(false);

    Serial.println("Recording stopped.");
    Serial.print("Total samples collected: ");
    Serial.println(gSampleCount);
    recordControl.blinkStopPattern();
}
}

void setup() {
    delay(3000);  // Give serial monitor time to connect

    Serial.begin(115200);
    
    // Initialize WebState
    WebState::begin();
    
    const bool wifiConnected = connectToWifi();
    if (wifiConnected) {
        syncUnixTime();
        WebState::setTimeSynced(gHasUnixTime);
        
        // Initialize web server after WiFi is ready
        gWebServer.begin();
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

    // Handle button events
    switch (recordControl.pollEvent(nowMs)) {
        case RecordControl::Event::StartRequested:
            if (!WebState::isRecording()) {  // Only start if not already recording
                startRecording();
            }
            break;
        case RecordControl::Event::StopRequested:
            if (WebState::isRecording()) {  // Only stop if recording
                stopRecording();
            }
            break;
        case RecordControl::Event::None:
            break;
    }

    // Check if web interface triggered a recording state change
    bool webWantsRecording = WebState::isRecording();
    bool currentlyRecording = csvRecorder.isOpen();
    
    if (webWantsRecording && !currentlyRecording) {
        startRecording();
    } else if (!webWantsRecording && currentlyRecording) {
        stopRecording();
    }

    if (!WebState::isRecording()) {
        // Broadcast last sample even when not recording (for status display)
        gWebServer.broadcastLastSample();
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

    // Update sample counters
    gSampleCount++;

    // Update WebState with last sample for web interface display
    WebState::setLastSample(sample);

    // Broadcast sample to WebSocket clients
    gWebServer.broadcastLastSample();

    // Print to serial for monitoring (optional, reduce verbosity)
    if (gSampleCount % 100 == 0) {
        Serial.print("Samples collected: ");
        Serial.print(gSampleCount);
        Serial.print(" | ECG: ");
        Serial.println(sample.ecgRaw);
    }
}
