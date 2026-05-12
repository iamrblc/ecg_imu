#include "web_server.h"
#include <ESPAsyncWebServer.h>
#include <FFat.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SD.h>
#include "config.h"
#include <ESPmDNS.h>

WebServer gWebServer;

WebServer::WebServer() : server(nullptr), ws(nullptr), lastBroadcastMs(0) {
}

void WebServer::begin() {
    // Initialize FFat for static files
    if (!FFat.begin(true)) {
        Serial.println("Failed to mount FFat");
        return;
    }

    // Create web server on port 80
    server = new AsyncWebServer(WebServerConfig::kPort);

    // Create WebSocket endpoint
    ws = new AsyncWebSocket("/ws/live");
    ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                       void* arg, uint8_t* data, size_t len) {
        this->handleWebSocketEvent(server, client, type, arg, data, len);
    });
    server->addHandler(ws);

    // HTTP Endpoints
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleRoot(request);
    });

    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleStatus(request);
    });

    server->on("/api/recording/start", HTTP_POST, 
               [this](AsyncWebServerRequest* request) {},
               nullptr,
               [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
                   this->handleRecordingStart(request, data, len, index, total);
               });

    server->on("/api/recording/stop", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleRecordingStop(request);
    });

    server->on("/api/files", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleFilesList(request);
    });

    server->on("/download", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleDownload(request);
    });

    // Serve static files from FFat
    server->serveStatic("/", FFat, "/").setDefaultFile("index.html");

    // Handle 404
    server->onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "application/json", "{\"error\":\"Not found\"}");
    });

    // Start server
    server->begin();

    // Initialize mDNS
    if (!MDNS.begin(WebServerConfig::kMdnsHostname)) {
        Serial.println("Error setting up mDNS responder!");
    } else {
        Serial.printf("mDNS responder started at %s.local\n", WebServerConfig::kMdnsHostname);
    }

    Serial.printf("Web server started on port %d\n", WebServerConfig::kPort);
}

bool WebServer::isRunning() {
    return server != nullptr;
}

void WebServer::handleRoot(AsyncWebServerRequest* request) {
    // Serve index.html from FFat
    request->send(FFat, "/index.html", "text/html");
}

void WebServer::handleStatus(AsyncWebServerRequest* request) {
    // Build JSON response with current system state
    String jsonResponse = buildStatusJson();
    request->send(200, "application/json", jsonResponse);
}

void WebServer::handleRecordingStart(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    // Parse JSON body
    DynamicJsonDocument doc(256);
    deserializeJson(doc, data, len);

    // Extract metadata from form
    RecordingMetadata metadata = {};
    const char* userInput = doc["filename"] | "";
    const char* dogId = doc["dog_id"] | "";
    const char* expId = doc["experiment_id"] | "";

    strncpy(metadata.userInputFilename, userInput, sizeof(metadata.userInputFilename) - 1);
    strncpy(metadata.dogId, dogId, sizeof(metadata.dogId) - 1);
    strncpy(metadata.experimentId, expId, sizeof(metadata.experimentId) - 1);

    // Update global state
    WebState::setMetadata(metadata);
    WebState::setRecording(true);

    // Generate and set filename with current timestamp
    char filename[128];
    WebState::generateFilename(filename, sizeof(filename), millis());  // TODO: Use actual Unix time when available
    WebState::setCurrentFilename(filename);

    request->send(200, "application/json", "{\"status\":\"recording started\"}");
    Serial.printf("Recording started: %s\n", filename);
}

void WebServer::handleRecordingStop(AsyncWebServerRequest* request) {
    WebState::setRecording(false);
    request->send(200, "application/json", "{\"status\":\"recording stopped\"}");
    Serial.println("Recording stopped");
}

void WebServer::handleFilesList(AsyncWebServerRequest* request) {
    sendFilesList(request);
}

void WebServer::handleDownload(AsyncWebServerRequest* request) {
    if (request->hasParam("file")) {
        String filename = request->getParam("file")->value();
        String filepath = String(WebServerConfig::kRecordingsPath) + "/" + filename;

        if (SD.exists(filepath.c_str())) {
            request->send(SD, filepath.c_str(), String(), true);
        } else {
            request->send(404, "application/json", "{\"error\":\"File not found\"}");
        }
    } else {
        request->send(400, "application/json", "{\"error\":\"Missing file parameter\"}");
    }
}

void WebServer::handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                                     void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("Client %u connected\n", client->id());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("Client %u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            // Handle incoming WebSocket data if needed
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void WebServer::broadcastLastSample() {
    if (!ws || ws->count() == 0) {
        return;
    }

    // Throttle to ~30 Hz
    unsigned long now = millis();
    if (now - lastBroadcastMs < WebServerConfig::kWebSocketUpdateIntervalMs) {
        return;
    }
    lastBroadcastMs = now;

    // Get last sample and format as JSON
    EcgSample sample = WebState::getLastSample();
    DynamicJsonDocument doc(256);
    doc["timestamp_ms"] = sample.timestampUnixMs;
    doc["elapsed_ms"] = sample.elapsedTimeMs;
    doc["ecg_raw"] = sample.ecgRaw;
    doc["ecg_proc"] = sample.ecgProcessed;
    doc["lo_pos"] = sample.loPos;
    doc["lo_neg"] = sample.loNeg;

    String jsonStr;
    serializeJson(doc, jsonStr);
    ws->textAll(jsonStr);
}

String WebServer::buildStatusJson() {
    DynamicJsonDocument doc(512);
    
    doc["recording"] = WebState::isRecording();
    doc["time_synced"] = WebState::isTimeSynced();
    
    // Add last sample
    EcgSample sample = WebState::getLastSample();
    doc["last_sample"]["timestamp_ms"] = sample.timestampUnixMs;
    doc["last_sample"]["ecg_raw"] = sample.ecgRaw;
    doc["last_sample"]["ecg_proc"] = sample.ecgProcessed;
    
    // Add metadata
    RecordingMetadata md = WebState::getMetadata();
    doc["metadata"]["dog_id"] = md.dogId;
    doc["metadata"]["experiment_id"] = md.experimentId;
    
    char currentFile[128];
    WebState::getCurrentFilename(currentFile, sizeof(currentFile));
    doc["current_filename"] = currentFile;

    String jsonStr;
    serializeJson(doc, jsonStr);
    return jsonStr;
}

void WebServer::sendFilesList(AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(1024);
    JsonArray files = doc.createNestedArray("files");

    // Ensure SD is initialized for directory listing even before first recording.
    if (!SD.begin(Pins::kSdCsPin)) {
        String jsonStr;
        serializeJson(doc, jsonStr);
        request->send(200, "application/json", jsonStr);
        return;
    }

    if (!SD.exists(WebServerConfig::kRecordingsPath)) {
        SD.mkdir(WebServerConfig::kRecordingsPath);
    }

    File root = SD.open(WebServerConfig::kRecordingsPath);
    if (!root || !root.isDirectory()) {
        String jsonStr;
        serializeJson(doc, jsonStr);
        request->send(200, "application/json", jsonStr);
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory() && String(file.name()).endsWith(".csv")) {
            JsonObject fileObj = files.createNestedObject();
            fileObj["name"] = file.name();
            fileObj["size"] = file.size();
            fileObj["time"] = file.getLastWrite();
        }
        file = root.openNextFile();
    }

    String jsonStr;
    serializeJson(doc, jsonStr);
    request->send(200, "application/json", jsonStr);
}
