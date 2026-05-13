
# Canine ECG / HR Measurement solution for behavior experiments

This project is being developed to provide an easy to use method for collecting ECG data and Heart Rate measurement through R-peak detection.

The main workflow uses a custom built portable ECG device (AD8232) module. But it also contains solutions for adapting a commercially available Polar H10 pulse monitoring belt.

## AD8232 on ESP32 (Arduino Nano remake)
### Features

- [ ] A cross-platform user interface is available to set up
    - [ ] recording metadata (eg. file name, subject name, test condition)
    - [ ] optional quasi-live streaming of data for monitoring purposes (the device can run without this)
- [x] On board button triggers the start and end of ECG data recording (ON / OFF)
- [ ] Board clock syncs with lab wifi before the experiment starts (UNIX time)
- [x] ECG module (AD8232) collects ECG data 
- [x] Data is written to an SD card 
- [ ] Board is powered by a chargable Li-ion battery pack

### Wiring
#### ECG module
|AD8232 |ESP32  |Comment                    | 
|-------|-------|---------------------------|
|GND    |GND    |ground                     |
|3v3    |3v3    |kraft                      |
|OUTPUT |A0     |analog                     |
|LO-    |D3     |Lead Off detection         |
|LO+    |D2     |Lead Off detection         |
|SDN    |-      |unconnected                |

#### SD card module
|SD     |ESP32  |Comment                                    | 
|-------|-------|-------------------------------------------|
|3v3    |3v3    |kraft                                      |
|CS     |D10    |Chip Select / fro SPI                      |
|MOSI   |D11    |ESP32 > SD card data                       |
|CLK    |D13    |Clock / timing signal for data transfer    |
|MISO   |D12    |SD card data  > ESP32                      |
|GND    |GND    |ground                                     |

> Note: AFAIK the digital pins are not arbitrary, although they can be modified later.
> Note2: OMG, I don't believe that MOSI and MISO were not cancelled yet.

#### SD card module
|SD     |ESP32  |Comment                                    | 
|-------|-------|-------------------------------------------|
|3v3    |3v3    |kraft                                      |
|CS     |D10    |Chip Select / fro SPI                      |
|MOSI   |D11    |ESP32 > SD card data                       |

#### Push Button Latch
|button |ESP32  |Comment                                    | 
|-------|-------|-------------------------------------------|
|NO     |D4     |Normally Open                              |
|COM    |GND    |Middle pin                                 |
|NC     |       |Normally Closed (unconnected               |

#### ESP32 board pins (note for self)
|PIN    |Stands for     |Comment                    | 
|-------|---------------|---------------------------|
|TX1    |transmit       |UART transmission          | 
|RX0    |receive        |UART reception             | 
|RST    |reset          |                           | 
|GND    |ground         |                           | 
|D2     |digital        |general (LO-)              |   
|D3     |digital        |general (LO+)              |
|D4     |digital        |general (push button latch)|
|D5     |digital        |general                    |
|D6     |digital        |general                    |
|D7     |digital        |general                    |
|D8     |digital        |general                    |
|D9     |digital        |general                    |
|D10    |digital        |reserved for CHIP SELECT   |
|D11    |digital        |reserved for MOSI          |
|D12    |digital        |reserved for MISO          |
|D13    |digital        |reserved for CLOCK         |
|3V3    |kraft          |3.3V                       |
|B0     |boot           |                           |
|A0     |analog         |                           |
|A1     |analog         |                           |
|A2     |analog         |                           |                           
|A4     |analog         |                           |
|A5     |analog         |                           |
|A6     |analog         |                           |
|A7     |analog         |                           |
|VBUS   |USB power      |5V                         |
|B1     |boot           |                           |
|GND    |ground         |                           |
|VIN    |voltage input  |                           |


```mermaid
flowchart TD
    A[Power On or Boot] --> B[setup in main.cpp]
    B --> C[WebState begin]
    C --> D[Load wifi.json from SD if present]
    D --> E{WiFi connected}
    E -- yes --> F[Update wifi.json with current IP]
    F --> G[Try NTP sync and set timeSynced]
    G --> H[Start web server and mDNS]
    E -- no --> I[Offline mode no NTP]

    H --> J[Init ECG sampler]
    I --> J
    J --> K[Init RecordControl button and LEDs]
    K --> L[Ensure recordings folder exists]
    L --> M{Latch already pressed}
    M -- yes --> N[startRecording]
    M -- no --> O[Enter main loop]
    N --> O

    O --> P[Check trigger sources]
    P --> P1[Button start or stop events]
    P --> P2[Web API start or stop requests]
    P --> P3[WebState recording reconciliation]
    P1 --> Q{Start or Stop}
    P2 --> Q
    P3 --> Q

    Q -- Start --> R[startRecording sequence]
    R --> R1[Init SD and create CSV path]
    R1 --> R2[Generate filename from metadata and timestamp]
    R2 --> R3[Open CSV and write header]
    R3 --> R4[Reset recording filter and counters]
    R4 --> R5[Set recording true and LED blue]
    R5 --> S[Sampling loop]

    Q -- Stop --> T[stopRecording sequence]
    T --> T1[Write metadata JSON beside CSV]
    T1 --> T2[Flush and close CSV]
    T2 --> T3[Set recording false and blink stop pattern]
    T3 --> O

    O --> U[Scheduler chooses cadence]
    S --> U
    U --> V[Read sample raw ADC and lead off pins]
    V --> W[Apply notch filter recording and live paths]
    W --> X{Recording active}
    X -- yes --> Y[Batch buffer rows and flush to CSV]
    X -- no --> Z[Skip CSV write]
    Y --> AA[Update WebState lastSample]
    Z --> AA
    AA --> AB[Broadcast websocket sample throttled]
    AB --> AC[Serve status files and download endpoints]
    AC --> O

    Y --> D1[CSV output timestamp elapsed raw proc lo_pos lo_neg]
    T1 --> D2[JSON metadata output]
    AB --> D3[Browser live chart output]
    AC --> D4[File list and download output]

    B1[Browser app init chart websocket polling] --> B2[Start button posts metadata]
    B1 --> B3[Stop button posts stop]
    B1 --> B4[Periodic status and files refresh]
    B2 --> P2
    B3 --> P2
    B4 --> AC

```




## POLAR DATA STREAM
> This part is currently not developed as the custom built solutions proved to be more suitable. 

This pipeline was developed to get ECG and accelerometer data from a Polar H10 HR belt for BARKS Lab. 

1. Change settings at `settings.yaml`
2. Run `polar_data_stream.py`.
3. Collect your output `csv` from the `outputs` directory.

Ignore `utils.py` (unless you know what you are doing).

That's it. 

## Functionality

- Gathers packages of accelerometer data (x, y, z axes) and heart rate service data. 
- It creates a new `csv` file. Then appends data at given intervals to avoid complete data loss if something goes awry. 
- It automatically stops at a set length for safety. However, it also listens to stop signal from the user.
- It has a GUI for ease of use. 

> BELOW THIS LINE EVERYTHING IS OBSOLETE

## What are the files for (polar10 directory)

### belts.yaml
This contains info about the individual belts. Later this will be used to access the selected belts.

### bleak_test.py
This is a quick test to see if the bluetooth works, what devices are found, etc. Not too important. Will be deprecated soon.

### hr_belt_access.py
Connects to a given belt and it lists all the available services.

### read_hr_stream.py
Reads the heart rate stream. It was used for testing purposes.

