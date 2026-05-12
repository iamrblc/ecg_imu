#include <Arduino.h>

#define LED_RED   14
#define LED_GREEN 15
#define LED_BLUE  16

void setColor(bool r, bool g, bool b) {
  digitalWrite(LED_RED,   r ? LOW : HIGH);
  digitalWrite(LED_GREEN, g ? LOW : HIGH);
  digitalWrite(LED_BLUE,  b ? LOW : HIGH);
}

void setup() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
}

void loop() {
  setColor(1,0,0); // red
  delay(500);

  setColor(0,1,0); // green
  delay(500);

  setColor(0,0,1); // blue
  delay(500);

  setColor(1,1,1); // white
  delay(500);

  setColor(0,0,0); // off
  delay(500);
}