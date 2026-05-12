#!/usr/bin/env python3
import os

# This script prepares the data files for SPIFFS upload
def prepare_spiffs_files(source, dest):
    """Copy data files to build directory for SPIFFS"""
    if not os.path.exists(source):
        print(f"Source directory {source} does not exist")
        return
    
    os.makedirs(dest, exist_ok=True)
    
    for file in os.listdir(source):
        source_file = os.path.join(source, file)
        dest_file = os.path.join(dest, file)
        
        if os.path.isfile(source_file):
            with open(source_file, 'rb') as f_in:
                with open(dest_file, 'wb') as f_out:
                    f_out.write(f_in.read())
            print(f"Prepared: {file}")

# Prepare files during build
if __name__ == "__main__":
    prepare_spiffs_files("data", ".pio/build/arduino_nano_esp32/spiffs")
    print("SPIFFS files prepared")
