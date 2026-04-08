import asyncio
import csv
import time
from bleak import BleakClient

DEVICE_ADDRESS = "A0:9E:1A:E6:B0:5E"

PMD_CONTROL = "fb005c81-02e7-f387-1cad-8acd2d8df0c8"
PMD_DATA = "fb005c82-02e7-f387-1cad-8acd2d8df0c8"

STREAM_SECONDS = 10

FS_ACC = 200
DT_ACC = 1 / FS_ACC
FS_ECG = 130
DT_ECG = 1 / FS_ECG

ACC_OUTPUT_FILE = "acc_data.csv"
ECG_OUTPUT_FILE = "ecg_data.csv"

ACC_MEASUREMENT_TYPE = 0x02
ECG_MEASUREMENT_TYPE = 0x00

ACC_GET_SETTINGS = bytearray([0x01, ACC_MEASUREMENT_TYPE])
ECG_GET_SETTINGS = bytearray([0x01, ECG_MEASUREMENT_TYPE])

ACC_START = bytearray([
	0x02, 0x02,
	0x00, 0x01, 0xC8, 0x00,
	0x01, 0x01, 0x10, 0x00,
	0x02, 0x01, 0x08, 0x00
])

ECG_START = bytearray([
	0x02, 0x00,
	0x00, 0x01, 0x82, 0x00,
	0x01, 0x01, 0x0E, 0x00
])

ACC_STOP = bytearray([0x03, ACC_MEASUREMENT_TYPE])
ECG_STOP = bytearray([0x03, ECG_MEASUREMENT_TYPE])


async def main():
	t0_device_acc = None
	t0_device_ecg = None

	acc_buffer = []
	ecg_buffer = []

	with (
		open(ACC_OUTPUT_FILE, "w", newline="") as acc_csv_file,
		open(ECG_OUTPUT_FILE, "w", newline="") as ecg_csv_file,
	):
		acc_writer = csv.writer(acc_csv_file)
		ecg_writer = csv.writer(ecg_csv_file)
		acc_writer.writerow(["time", "t_wall", "t_device_ns", "x", "y", "z"])
		ecg_writer.writerow(["time", "t_wall", "t_device_ns", "ecg"])

		def handle_pmd_control(sender, data: bytearray):
			print("PMD control:", data.hex(" "))

		def handle_pmd_data(sender, data: bytearray):
			nonlocal t0_device_acc, t0_device_ecg, acc_buffer, ecg_buffer
			t_wall = time.time()

			measurement_type = data[0]

			if measurement_type == ACC_MEASUREMENT_TYPE:
				ts_ns = int.from_bytes(data[1:9], byteorder="little", signed=False)
				frame_type = data[9]
				if frame_type != 0x01:
					print(f"Unsupported ACC frame type: {frame_type:#04x}")
					return

				acc_buffer.clear()
				offset = 10
				while offset + 6 <= len(data):
					x = int.from_bytes(data[offset:offset + 2], byteorder="little", signed=True)
					y = int.from_bytes(data[offset + 2:offset + 4], byteorder="little", signed=True)
					z = int.from_bytes(data[offset + 4:offset + 6], byteorder="little", signed=True)
					acc_buffer.append((x, y, z))
					offset += 6

				T = ts_ns / 1e9
				N = len(acc_buffer)
				if N == 0:
					return

				if t0_device_acc is None:
					t0_device_acc = T - (N - 1) * DT_ACC

				for i, (x, y, z) in enumerate(acc_buffer):
					t_sample = T - (N - 1 - i) * DT_ACC - t0_device_acc
					acc_writer.writerow([f"{t_sample:.6f}", f"{t_wall:.6f}", ts_ns, x, y, z])
					print(
						f"ACC t={t_sample:.6f}  t_wall={t_wall:.3f}  ts_ns={ts_ns}"
						f"  x={x:6d}  y={y:6d}  z={z:6d}"
					)
				return

			if measurement_type == ECG_MEASUREMENT_TYPE:
				ts_ns = int.from_bytes(data[1:9], byteorder="little", signed=False)
				frame_type = data[9]
				if frame_type != 0x00:
					print(f"Unsupported ECG frame type: {frame_type:#04x}")
					return

				ecg_buffer.clear()
				offset = 10
				while offset + 2 <= len(data):
					ecg = int.from_bytes(data[offset:offset + 2], byteorder="little", signed=True)
					ecg_buffer.append(ecg)
					offset += 2

				T = ts_ns / 1e9
				N = len(ecg_buffer)
				if N == 0:
					return

				if t0_device_ecg is None:
					t0_device_ecg = T - (N - 1) * DT_ECG

				for i, ecg in enumerate(ecg_buffer):
					t_sample = T - (N - 1 - i) * DT_ECG - t0_device_ecg
					ecg_writer.writerow([f"{t_sample:.6f}", f"{t_wall:.6f}", ts_ns, ecg])
					print(f"ECG t={t_sample:.6f}  t_wall={t_wall:.3f}  ts_ns={ts_ns}  ecg={ecg:6d}")

		async with BleakClient(DEVICE_ADDRESS) as client:
			print("Connected")

			await client.start_notify(PMD_CONTROL, handle_pmd_control)
			await client.start_notify(PMD_DATA, handle_pmd_data)

			print("Requesting ACC settings...")
			await client.write_gatt_char(PMD_CONTROL, ACC_GET_SETTINGS, response=True)
			await asyncio.sleep(1.0)

			print("Requesting ECG settings...")
			await client.write_gatt_char(PMD_CONTROL, ECG_GET_SETTINGS, response=True)
			await asyncio.sleep(1.0)

			print("Starting ACC stream...")
			await client.write_gatt_char(PMD_CONTROL, ACC_START, response=True)
			await asyncio.sleep(0.2)

			print("Starting ECG stream...")
			await client.write_gatt_char(PMD_CONTROL, ECG_START, response=True)

			await asyncio.sleep(STREAM_SECONDS)

			print("Stopping ACC stream...")
			await client.write_gatt_char(PMD_CONTROL, ACC_STOP, response=True)
			await asyncio.sleep(0.2)

			print("Stopping ECG stream...")
			await client.write_gatt_char(PMD_CONTROL, ECG_STOP, response=True)

			await client.stop_notify(PMD_DATA)
			await client.stop_notify(PMD_CONTROL)

	print(f"Saved ACC to {ACC_OUTPUT_FILE}")
	print(f"Saved ECG to {ECG_OUTPUT_FILE}")


if __name__ == "__main__":
	asyncio.run(main())


