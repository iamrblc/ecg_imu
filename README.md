
# Canine ECG / HR Measurement solution for behavior experiments

This project is being developed to provide an easy to use method for collecting ECG data and Heart Rate measurement through R-peak detection.

The main workflow uses a custom built portable ECG device (AD8232) module. But it also contains solutions for adapting a commercially available Polar H10 pulse monitoring belt.

## AD8232 on ESP32 (Arduino Nano remake)
### Features

- [x] A cross-platform user interface is available to set up
    - [x] recording metadata (eg. file name, subject name, test condition)
    - [x] optional quasi-live streaming of data for monitoring purposes (the device can run without this)
    - [x] system monitoring is on display (is time synced through wifi, recording started, etc)
- [x] After boot it checks if wifi is available.
    - [x] It checks if a wifi.json is added. If so, it connects to that and updates ip address in the file.
    - [x] If wifi.json is not added or cannot connect, it falls back to default wifi.
    - [x] If so, it does NTP (Network Time Protocol) syncing (UNIX time)
    - [x] If not, it just measures elapsed time.
    - [ ] RTC (Real-Time Clock) module is added so sync accuracy stays for long time even without wifi (found workaround to avoid size increase)
- [x] ECG module (AD8232) collects ECG data 
- [x] Standby indicator shows when recording can start (green LED)
- [x] On board button triggers the start and end of ECG data recording
    - [x] Recording starts: indicated by continuous blue LED
    - [x] Recording stops: indicated by 3 blinks of the blue LED
- [x] SD card is inicialized 
    - [x] CSV file name is generated from user input (or UNIX time if input is missing)
    - [x] CSV file is created with the following columns:
        - [x] timestamp (UNIX time)
        - [x] time (elapsed time in ms from the beginning of the recording (@200Hz))
        - [x] ecg_raw (raw ECG signal)
        - [x] ecg_proc (processed ECG signal - removed 50 Hz hum, artifacts, etc)
        - [ ] added error handling for signal loss, power outage, armageddon
    - [ ] other processed data is recorded in an appropriate format
        - [ ] HR (heart rate) based on physiozoo
        - [ ] EDR (ECG-derived respiration)
- [x] Interface can be accessed from any browser through `caninecg.local` (or board IP address)
    - [x] Recording can be started and stopped from the interface as well.
    - [x] Recordings from the SD card can be viewed and downloaded to the computer through the interface.
    - [x] Time sync indicator is present
    - [x] Recording status is shown.
    - [ ] Battery status can be viewed through the interface
    - [x] Users can add file name, and all sorts of experiment metadata
        - [x] metadata are stored as json
        - [x] filenames are automatically expanded with timestamp to avoid accidental overwrites and to distinguish different measurements with the same subject in the same experimental setup
        - [x] fallback is added so if by accident there is no user input, or recording started fromt he board, the timestamp itself is the filename.
- [ ] Board is powered by a chargable Li-ion battery pack (ordered)
- [ ] Overall power switch is added (ordered)
- [ ] everything is soldered together into a neat little case on a PCB (board ordered)

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

### Wifi connection
- currently wifi network and password is hard coded and to change it, it requires programming.
- to overcome this, a simple `wifi.json` file can be added to the SD card with exactly this content:

```json
{"ssid":"the-name-of-the-network",
"password":"the-password-to-the-network",
"ip":""}
```
> Note, the IP address is autmatically generated by the program, you need to fill in only the ssid and the password. This json file can be written in any kind of text editor (not Word or other word processors!)


## Known issues

### The interface
It still needs fintuning, but works well.

### Timestamps
They are currently in UTC and not CET. 

### RAW ECG = PROC ECG
The processing part is a next step, so at the moment processing is simply duplicating the raw signal.
(This also involved physical tweaks, so this is a next big step.)

### User derived metadata
Currently there is file name, dog id and experiment id for placeholders. These can change.

### File downloads
Currently only the recordings are listed, the corresponding jsons with metadata are not. 

### SD card is not accessible through usb as remote storage
This is intended to avoid conflicts.

### Windows may not open caninecg.localhost
It's a known issue on Win.
- Workaround 1: Use your mobile phone.
- Workaround 2: Turn on the board, wait till it connects to wifi, remove the card, check `wifi.json` and use the presented ip address to acecss the interface. (You may to this only rarely as the IP address is not supposed to change too frequently.)


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

