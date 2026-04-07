import asyncio
import time
from bleak import BleakClient

DEVICE_ADDRESS = "FFDB0E1C-0262-9016-D154-4562DABCBE43"

PMD_CONTROL = "fb005c81-02e7-f387-1cad-8acd2d8df0c8"
PMD_DATA    = "fb005c82-02e7-f387-1cad-8acd2d8df0c8"

STREAM_SECONDS = 10

# PMD measurement type for ACC is 0x02
ACC_MEASUREMENT_TYPE = 0x02

# Step 1: ask device which ACC settings it supports
ACC_GET_SETTINGS = bytearray([0x01, ACC_MEASUREMENT_TYPE])

# Step 2: start ACC with one concrete setting set.
# This commonly seen example means:
#   op=0x02 (start)
#   type=0x02 (ACC)
#   sample rate = 0x00C8 = 200 Hz
#   resolution  = 0x0010 = 16 bit
#   range       = 0x0008 = 8 g
ACC_START = bytearray([
    0x02, 0x02,
    0x00, 0x01, 0xC8, 0x00,
    0x01, 0x01, 0x10, 0x00,
    0x02, 0x01, 0x08, 0x00
])

# Stop ACC stream
ACC_STOP = bytearray([0x03, ACC_MEASUREMENT_TYPE])


async def main():
    def handle_pmd_control(sender, data: bytearray):
        print("PMD control:", data.hex(" "))

    def handle_pmd_data(sender, data: bytearray):
        t = time.time()                     # THIS WAY IT'S NOT POSIX BUT ACTUAL TIME WHEN PACKAGE ARRIVES
        print(f"ACC raw packet @ {t}: {data.hex(' ')}")

    async with BleakClient(DEVICE_ADDRESS) as client:
        print("Connected")

        await client.start_notify(PMD_CONTROL, handle_pmd_control)
        await client.start_notify(PMD_DATA, handle_pmd_data)

        print("Requesting ACC settings...")
        await client.write_gatt_char(PMD_CONTROL, ACC_GET_SETTINGS, response=True)
        await asyncio.sleep(1.0)

        print("Starting ACC stream...")
        await client.write_gatt_char(PMD_CONTROL, ACC_START, response=True)

        await asyncio.sleep(STREAM_SECONDS)

        print("Stopping ACC stream...")
        await client.write_gatt_char(PMD_CONTROL, ACC_STOP, response=True)

        await client.stop_notify(PMD_DATA)
        await client.stop_notify(PMD_CONTROL)


if __name__ == "__main__":
    asyncio.run(main())