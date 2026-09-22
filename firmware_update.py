import struct
import time
import zlib

import serial
from intelhex import IntelHex


APPLICATION_HEX = (
    "projects/01-stm32-custom-bootloader/"
    "application/Debug/Application.hex"
)

PORT = "COM15"
BAUDRATE = 115200

APP_HEADER_ADDRESS = 0x08004000
APP_START_ADDRESS = 0x08004400
APP_END_ADDRESS = 0x08010000

FW_MAGIC_NUMBER = 0xB00710AD
FW_VERSION = 0x00010000

DATA_BLOCK_SIZE = 8


# Bootloader commands
CMD_START_UPDATE = b'S'
CMD_ERASE = b'R'
CMD_DATA = b'D'
CMD_HEADER = b'H'
CMD_VERIFY = b'V'
CMD_END_UPDATE = b'E'


# Bootloader responses
BL_ACK = b'A'
BL_NACK = b'N'
BL_READY = b'Y'
BL_BYTE_RECEIVED = b'K'
BL_BUFFER_RECEIVED = b'B'


# Load application
application = IntelHex(APPLICATION_HEX)
application.start_addr = None
application.padding = 0xFF

app_min = application.minaddr()
app_max = application.maxaddr()

print(f"Application start : 0x{app_min:08X}")
print(f"Application end   : 0x{app_max:08X}")


if app_min != APP_START_ADDRESS:
    raise ValueError(
        f"Application starts at 0x{app_min:08X}, "
        f"expected 0x{APP_START_ADDRESS:08X}"
    )

if app_max >= APP_END_ADDRESS:
    raise ValueError(
        f"Application ends at 0x{app_max:08X}, "
        "outside application Flash region"
    )


firmware_size = app_max - APP_START_ADDRESS + 1

firmware_data = bytes(
    application[address]
    for address in range(
        APP_START_ADDRESS,
        APP_START_ADDRESS + firmware_size
    )
)

firmware_crc32 = zlib.crc32(firmware_data) & 0xFFFFFFFF


firmware_header = struct.pack(
    "<IIII",
    FW_MAGIC_NUMBER,
    firmware_size,
    firmware_crc32,
    FW_VERSION
)


number_of_blocks = (
    firmware_size + DATA_BLOCK_SIZE - 1
) // DATA_BLOCK_SIZE


print(f"Firmware size     : {firmware_size} bytes")

print("\nFirmware Header")
print("----------------------------")
print(f"Magic        : 0x{FW_MAGIC_NUMBER:08X}")
print(f"Size         : {firmware_size} bytes")
print(f"CRC32        : 0x{firmware_crc32:08X}")
print(f"Version      : 0x{FW_VERSION:08X}")
print(f"Header size  : {len(firmware_header)} bytes")
print("Header bytes :", firmware_header.hex(" ").upper())

print(f"Block size        : {DATA_BLOCK_SIZE} bytes")
print(f"Number of blocks  : {number_of_blocks}")
print("Application HEX loaded successfully.")


ser = serial.Serial(
    port=PORT,
    baudrate=BAUDRATE,
    bytesize=8,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    timeout=2
)

time.sleep(0.5)

ser.reset_input_buffer()
ser.reset_output_buffer()

print("\nSerial port opened:", PORT)


def close_and_exit(message):
    print(message)

    if ser.is_open:
        ser.close()

    raise SystemExit(1)


def send_command(command):
    ser.write(command)
    ser.flush()

    return ser.read(1)


def send_application_block(block_index, block):
    address = APP_START_ADDRESS + block_index * DATA_BLOCK_SIZE

    print(f"\nBlock {block_index} @ 0x{address:08X}")
    print("Data:", block.hex(" ").upper())

    ser.write(CMD_DATA)
    ser.flush()

    response = ser.read(1)

    if response != BL_READY:
        close_and_exit(
            f"STM32 not ready for block {block_index}. "
            f"Response: {response}"
        )

    for byte_index, byte in enumerate(block):
        ser.write(bytes([byte]))
        ser.flush()

        response = ser.read(1)

        if response != BL_BYTE_RECEIVED:
            close_and_exit(
                f"Block {block_index}, byte {byte_index} failed. "
                f"Response: {response}"
            )

    response = ser.read(1)

    if response != BL_BUFFER_RECEIVED:
        close_and_exit(
            f"Buffer confirmation failed for block {block_index}. "
            f"Response: {response}"
        )

    response = ser.read(1)

    if response != BL_ACK:
        close_and_exit(
            f"Flash programming failed for block {block_index}. "
            f"Response: {response}"
        )

    print("Block programmed -> ACK")


def send_firmware_header():
    print("\nProgramming firmware header...")

    ser.write(CMD_HEADER)
    ser.flush()

    response = ser.read(1)

    print("HEADER command ->", response)

    if response != BL_READY:
        close_and_exit(
            f"STM32 not ready for Header. "
            f"Response: {response}"
        )

    for index, byte in enumerate(firmware_header):
        ser.write(bytes([byte]))
        ser.flush()

        response = ser.read(1)

        print(
            f"Header byte {index}: "
            f"0x{byte:02X} -> {response}"
        )

        if response != BL_BYTE_RECEIVED:
            close_and_exit(
                f"Header byte {index} failed. "
                f"Response: {response}"
            )

    response = ser.read(1)

    print("Header buffer status ->", response)

    if response != BL_BUFFER_RECEIVED:
        close_and_exit(
            f"Header buffer not confirmed. "
            f"Response: {response}"
        )

    response = ser.read(1)

    if response != BL_ACK:
        close_and_exit(
            f"Header programming failed. "
            f"Response: {response}"
        )

    print("Firmware Header programmed -> ACK")


def verify_firmware():
    print("\nVerifying downloaded firmware...")

    ser.write(CMD_VERIFY)
    ser.flush()

    old_timeout = ser.timeout
    ser.timeout = 15

    response = ser.read(1)

    ser.timeout = old_timeout

    print("VERIFY response ->", response)

    if response == BL_ACK:
        print("Firmware verification -> ACK")
        print("Header validation      -> OK")
        print("Application validation -> OK")
        print("CRC32 verification     -> OK")
        return

    if response == BL_NACK:
        close_and_exit("Firmware verification -> NACK")

    close_and_exit(
        f"No valid response during firmware verification. "
        f"Response: {response}"
    )


def end_update():
    print("\nEnding firmware update...")

    ser.write(CMD_END_UPDATE)
    ser.flush()

    # The STM32 checks the CRC again before resetting.
    old_timeout = ser.timeout
    ser.timeout = 15

    response = ser.read(1)

    ser.timeout = old_timeout

    print("END UPDATE response ->", response)

    if response == BL_ACK:
        print("END UPDATE -> ACK")
        print("STM32 reset requested")
        return

    if response == BL_NACK:
        close_and_exit("END UPDATE -> NACK")

    close_and_exit(
        f"No valid response during END UPDATE. "
        f"Response: {response}"
    )


print("\nEntering UPDATE MODE...")

response = send_command(CMD_START_UPDATE)

if response != BL_ACK:
    close_and_exit(
        f"START UPDATE failed. "
        f"Response: {response}"
    )

print("START UPDATE -> ACK")


print("\nErasing firmware area...")

response = send_command(CMD_ERASE)

if response != BL_ACK:
    close_and_exit(
        f"ERASE failed. "
        f"Response: {response}"
    )

print("ERASE -> ACK")


print("\nProgramming application...")

for block_index in range(number_of_blocks):
    start = block_index * DATA_BLOCK_SIZE
    end = start + DATA_BLOCK_SIZE

    block = firmware_data[start:end]

    if len(block) < DATA_BLOCK_SIZE:
        block += bytes(
            [0xFF] * (DATA_BLOCK_SIZE - len(block))
        )

    send_application_block(block_index, block)


print(
    f"\n{number_of_blocks} firmware blocks "
    "programmed successfully."
)


send_firmware_header()

verify_firmware()

end_update()


ser.close()


print("\n======================================")
print("Firmware update completed.")
print("======================================")

print(
    f"Application : "
    f"0x{APP_START_ADDRESS:08X} -> "
    f"0x{app_max:08X}"
)

print(
    f"Header      : "
    f"0x{APP_HEADER_ADDRESS:08X} -> "
    f"0x{APP_HEADER_ADDRESS + 15:08X}"
)

print(f"Firmware size : {firmware_size} bytes")
print(f"Blocks        : {number_of_blocks}")
print(f"CRC32         : 0x{firmware_crc32:08X}")
print(f"Version       : 0x{FW_VERSION:08X}")

print("======================================")
print("UPDATE SUCCESSFUL")
print("======================================")