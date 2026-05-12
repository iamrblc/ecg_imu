In this project I'm building a portable ECG monitor that consists of an arduino nano esp32 board, an SD card module and an AD8232 ECG module.

I tested each module individually, they are wired up properly.

# Wiring
## ECG (AD8232) - ESP32
GND -> GND
3v3 -> 3v3
Output -> A0
LO- -> D3
LO+ -> D2

## SD Card Module - ESP32
3v3 -> 3v3
CS -> D10
MOSI -> D11
CLK -> D13
MISO -> D12
GND -> GND

# Module tests
I tested each module and some functionality separately in the /test directory.
- rgb_led_test.cpp: connection to the board works
- ecg_test.cpp: the ecg module works
- sd_inicialization_test.cpp: the sd card module works
- csv_write_test.cpp: I could write a simple csv file.