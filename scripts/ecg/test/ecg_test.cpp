#include <Arduino.h>

const int ECG_PIN = A0;
const int LO_PLUS = D2;
const int LO_MINUS = D3;

void setup() {
    Serial.begin(115200);

    pinMode(LO_PLUS, INPUT);
    pinMode(LO_MINUS, INPUT);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
}

void loop() {
    int loPlus = digitalRead(LO_PLUS);
    int loMinus = digitalRead(LO_MINUS);
    int ecg = analogRead(ECG_PIN);

    Serial.print("LO+=");
    Serial.print(loPlus);

    Serial.print(" LO-=");
    Serial.print(loMinus);

    Serial.print(" ECG=");
    Serial.println(ecg);

    delay(100);
}