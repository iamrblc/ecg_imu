# POLAR DATA STREAM

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

