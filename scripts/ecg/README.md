
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
    %% AD8232/ESP32 ECG pipeline only (scripts/ecg)

    A[Power On / Boot] --> B[setup in main.cpp]
    B --> C[WebState begin]
    C --> D[Try load wifi.json from SD]
    D --> E{WiFi connected?}
    E -- yes --> F[Save/update wifi.json with IP]
    F --> G[Try NTP sync -> set timeSynced]
    G --> H[Start web server + mDNS]
    E -- no --> I[Run offline mode\nno NTP no web server]

    H --> J[Init ECG sampler]
    I --> J
    J --> K[Init RecordControl\nbutton + LEDs]
    K --> L[Ensure /recordings exists on SD]
    L --> M{Latch already pressed?}
    M -- yes --> N[startRecording]
    M -- no --> O[Enter main loop]
    N --> O

    subgraph Triggers[Recording trigger paths]
      T1[Button debounced\nStartRequested/StopRequested]
      T2[Web API POST\nrecording start/stop]
      T3[WebState.recording flag\nreconciled each loop]
    end

    O --> T1
    O --> T2
    O --> T3

    T1 --> U{Start or Stop?}
    T2 --> U
    T3 --> U

    U -- Start --> V[startRecording]
    V --> V1[Init SD if needed]
    V1 --> V2[Generate filename from metadata + timestamp]
    V2 --> V3[Open CSV in /recordings]
    V3 --> V4[Write CSV header]
    V4 --> V5[Reset recording filter]
    V5 --> V6[Set startTime sample counters state]
    V6 --> V7[WebState.recording=true\nLED blue]
    V7 --> W[Sampling loop while active]

    U -- Stop --> X[stopRecording]
    X --> X1[Write metadata JSON next to CSV]
    X1 --> X2[Flush/close CSV]
    X2 --> X3[WebState.recording=false\nLED stop blink then standby]
    X3 --> O

    subgraph Runtime[Continuous runtime behavior]
      R1[Choose scheduler\nrecording cadence or always-on cadence]
      R2[Read ECG sample\nraw ADC + LO pins]
      R3[Apply notch filter\nrecording + live paths]
      R4{Recording active?}
      R5[Buffer rows and batch flush to CSV]
      R6[Update WebState.lastSample]
      R7[Broadcast websocket sample\n~30 Hz throttled]
      R8[HTTP poll endpoints\nstatus/files/download]
    end

    W --> R1
    O --> R1
    R1 --> R2
    R2 --> R3
    R3 --> R4
    R4 -- yes --> R5
    R4 -- no --> R6
    R5 --> R6
    R6 --> R7
    R7 --> R8
    R8 --> O

    subgraph DataOutputs[Persistent and live outputs]
      D1[CSV\ntimestamp elapsed raw proc lo_pos lo_neg]
      D2[JSON metadata\nfilename ids start end duration sample_count]
      D3[Browser chart\nraw+processed via websocket]
      D4[File management\nlist + download recordings]
    end

    R5 --> D1
    X1 --> D2
    R7 --> D3
    R8 --> D4

    subgraph BrowserUI[interface/app.js]
      B1[Page load init chart + websocket + polling]
      B2[Start button sends metadata to API]
      B3[Stop button calls API]
      B4[Periodic status and files refresh]
    end

    B1 --> B2
    B1 --> B3
    B1 --> B4
    B2 --> T2
    B3 --> T2
    B4 --> R8

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

