#pragma once

#include <ESPAsyncWebServer.h>
#include "web_state.h"

class WebServer {
public:
    WebServer();

    // Initialize and start web server
    void begin();

    // Check if server is running
    bool isRunning();

    // Handle incoming WebSocket message
    void broadcastLastSample();

private:
    AsyncWebServer* server;
    AsyncWebSocket* ws;
    unsigned long lastBroadcastMs;

    // Endpoint handlers
    void handleRoot(AsyncWebServerRequest* request);
    void handleStatus(AsyncWebServerRequest* request);
    void handleRecordingStart(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleRecordingStop(AsyncWebServerRequest* request);
    void handleFilesList(AsyncWebServerRequest* request);
    void handleDownload(AsyncWebServerRequest* request);
    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                              void* arg, uint8_t* data, size_t len);

    // Utility functions
    String buildStatusJson();
    void sendFilesList(AsyncWebServerRequest* request);
};

extern WebServer gWebServer;
