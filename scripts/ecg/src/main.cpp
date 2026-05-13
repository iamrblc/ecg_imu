#include <Arduino.h>
#include <ArduinoJson.h>
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
unsigned long nextAlwaysSampleMs = 0;  // For continuous sampling even when not recording
unsigned long systemBootTimeMs = 0;    // When the system started
bool gHasUnixTime = false;
uint32_t gSampleCount = 0;
uint64_t gRecordingStartTimestamp = 0;

namespace {

constexpr time_t kMinValidUnixTime = 1704067200;
struct WifiCredentials {
    String ssid;
    String password;
};

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

bool connectToWifi(const char* ssid, const char* password) {
    if (ssid == nullptr || ssid[0] == '\0') {
        Serial.println("WiFi skipped: SSID not configured.");
        return false;
    }

    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password != nullptr ? password : "");

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

bool loadWifiCredentialsFromSd(WifiCredentials& credentials) {
    if (!SD.begin(Pins::kSdCsPin)) {
        Serial.println("WiFi credentials file unavailable: SD not ready.");
        return false;
    }

    if (!SD.exists(WifiConfig::kCredentialsFilePath)) {
        Serial.println("WiFi credentials file not found on SD.");
        return false;
    }

    File file = SD.open(WifiConfig::kCredentialsFilePath, FILE_READ);
    if (!file) {
        Serial.println("Failed to open WiFi credentials file.");
        return false;
    }

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print("Invalid WiFi credentials JSON: ");
        Serial.println(error.c_str());
        return false;
    }

    const char* ssid = doc["ssid"] | "";
    const char* password = doc["password"] | "";

    if (ssid[0] == '\0') {
        Serial.println("Invalid WiFi credentials JSON: ssid is empty.");
        return false;
    }

    credentials.ssid = ssid;
    credentials.password = password;
    return true;
}

bool saveWifiCredentialsToSd(const WifiCredentials& credentials) {
    if (!SD.begin(Pins::kSdCsPin)) {
        Serial.println("Skipping WiFi credentials update: SD not ready.");
        return false;
    }

    if (SD.exists(WifiConfig::kCredentialsFilePath) && !SD.remove(WifiConfig::kCredentialsFilePath)) {
        Serial.println("Failed to replace WiFi credentials file.");
        return false;
    }

    File file = SD.open(WifiConfig::kCredentialsFilePath, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open WiFi credentials file for writing.");
        return false;
    }

    JsonDocument doc;
    doc["ssid"] = credentials.ssid;
    doc["password"] = credentials.password;
    doc["ip"] = WiFi.localIP().toString();

    if (serializeJson(doc, file) == 0) {
        file.close();
        Serial.println("Failed to write WiFi credentials JSON.");
        return false;
    }

    file.close();
    Serial.println("WiFi credentials file updated.");
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
    
    // Initialize timing for always-on sampling
    systemBootTimeMs = millis();
    nextAlwaysSampleMs = systemBootTimeMs;
    
    // Initialize WebState
    WebState::begin();

    WifiCredentials activeCredentials{WifiConfig::kSsid, WifiConfig::kPassword};
    WifiCredentials sdCredentials;
    bool wifiConnected = false;

    if (loadWifiCredentialsFromSd(sdCredentials)) {
        Serial.println("Attempting WiFi connection using /wifi.json credentials.");
        wifiConnected = connectToWifi(sdCredentials.ssid.c_str(), sdCredentials.password.c_str());
        if (wifiConnected) {
            activeCredentials = sdCredentials;
        } else {
            Serial.println("Falling back to WiFi credentials from config.h.");
        }
    } else {
        Serial.println("Using WiFi credentials from config.h.");
    }

    if (!wifiConnected) {
        wifiConnected = connectToWifi(activeCredentials.ssid.c_str(), activeCredentials.password.c_str());
    }

    if (wifiConnected) {
        saveWifiCredentialsToSd(activeCredentials);
        syncUnixTime();
        WebState::setTimeSynced(gHasUnixTime);
        
        // Initialize web server after WiFi is ready
        gWebServer.begin();
    }

    ecgSampler.begin();
    recordControl.begin();

    // Initialize SD and create /recordings directory for file listing
    Serial.println("Initializing SD card for recordings directory...");
    if (SD.begin(Pins::kSdCsPin)) {
        if (!SD.exists(WebServerConfig::kRecordingsPath)) {
            if (SD.mkdir(WebServerConfig::kRecordingsPath)) {
                Serial.println("/recordings directory created");
            }
        } else {
            Serial.println("/recordings directory already exists");
        }
    } else {
        Serial.println("Warning: SD card not available yet");
    }

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

    // Determine which timing to use: recording vs always-on sampling
    bool isRecording = WebState::isRecording();
    unsigned long& nextDueMs = isRecording ? nextSampleDueMs : nextAlwaysSampleMs;

    // Check if it's time for a new sample
    if (nowMs < nextDueMs) {
        // Not yet time for a sample, small delay
        delay(1);
        gWebServer.broadcastLastSample();  // Still broadcast last sample
        return;
    }

    // Calculate elapsed time appropriately based on state
    unsigned long elapsedTime;
    if (isRecording) {
        // During recording, elapsed time is relative to recording start
        elapsedTime = nextDueMs - startTime;
    } else {
        // When not recording, elapsed time is relative to system boot
        elapsedTime = nowMs - systemBootTimeMs;
    }

    // Read ECG sample
    EcgSample sample = ecgSampler.readSample(elapsedTime, getUnixTimeMs());
    
    // Update timing for next sample
    nextDueMs += RecordingConfig::kSampleIntervalMs;
    if (nowMs > nextDueMs) {
        nextDueMs = nowMs + RecordingConfig::kSampleIntervalMs;
    }

    // Write to CSV only if recording
    if (isRecording) {
        if (!csvRecorder.writeSample(sample)) {
            Serial.println("Failed to write ECG sample!");
            stopRecording();
            delay(100);
            return;
        }
        gSampleCount++;

        // Print to serial for monitoring (optional, reduce verbosity)
        if (gSampleCount % 100 == 0) {
            Serial.print("Samples collected: ");
            Serial.print(gSampleCount);
            Serial.print(" | ECG: ");
            Serial.println(sample.ecgRaw);
        }
    }

    // Always update WebState with last sample and broadcast (whether recording or not)
    WebState::setLastSample(sample);
    gWebServer.broadcastLastSample();
}
