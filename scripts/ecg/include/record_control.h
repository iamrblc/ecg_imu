#pragma once

class RecordControl {
public:
    enum class Event {
        None,
        StartRequested,
        StopRequested
    };

    void begin();
    Event pollEvent(unsigned long nowMs);

    void setRecording(bool isRecording);
    bool isRecording() const;
    bool isLatchPressed() const;

    void blinkStopPattern();

private:
    void setColor(bool redOn, bool greenOn, bool blueOn);
    void setRecordingIndicator(bool on);

    bool lastRawPressed = false;
    bool stablePressed = false;
    bool recording = false;
    unsigned long lastChangeMs = 0;
};