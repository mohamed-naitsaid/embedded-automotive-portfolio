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

CAN_SIM_FRAME_START = 0xC5
UDS_REQUEST_ID = 0x7E0
UDS_RESPONSE_ID = 0x7E8

APP_START_ADDRESS = 0x08004400
APP_END_ADDRESS = 0x08010000

SEED = 0x12345678
KEY_MASK = 0xA5A5A5A5

# Proteus COMPIM is much slower and less reliable than real CAN.
# 100 ms/byte is intentionally conservative: earlier V5/V6 tests
# showed that faster pacing can desynchronize the UART CAN shim.
# Real bxCAN does not need this artificial delay.
UART_BYTE_DELAY = 0.10
SERIAL_TIMEOUT = 10
MAX_RETRIES = 5

TRANSFER_DATA_BYTES = 5


def load_application():
    app = IntelHex(APPLICATION_HEX)
    app.start_addr = None
    app.padding = 0xFF

    app_min = app.minaddr()
    app_max = app.maxaddr()

    if app_min != APP_START_ADDRESS:
        raise ValueError(
            f"Application starts at 0x{app_min:08X}, "
            f"expected 0x{APP_START_ADDRESS:08X}"
        )

    if app_max >= APP_END_ADDRESS:
        raise ValueError("Application exceeds allowed Flash region")

    size = app_max - APP_START_ADDRESS + 1

    data = bytes(
        app[address]
        for address in range(
            APP_START_ADDRESS,
            APP_START_ADDRESS + size
        )
    )

    crc = zlib.crc32(data) & 0xFFFFFFFF

    return data, size, crc


def build_frame(can_id, data):
    if len(data) > 8:
        raise ValueError("Classic CAN DLC cannot exceed 8")

    return (
        bytes([CAN_SIM_FRAME_START])
        + can_id.to_bytes(2, "little")
        + bytes([len(data)])
        + bytes(data)
    )


def send_frame(ser, can_id, data, byte_delay=UART_BYTE_DELAY):
    frame = build_frame(can_id, data)

    for byte in frame:
        ser.write(bytes([byte]))
        ser.flush()
        time.sleep(byte_delay)


def read_exact(ser, length):
    data = ser.read(length)

    if len(data) != length:
        raise TimeoutError(
            f"Expected {length} bytes, got {len(data)}"
        )

    return data


def read_frame(ser):
    while True:
        marker = ser.read(1)

        if not marker:
            raise TimeoutError("No response from STM32")

        if marker[0] == CAN_SIM_FRAME_START:
            break

    header = read_exact(ser, 3)

    can_id = int.from_bytes(header[0:2], "little")
    dlc = header[2]

    if dlc > 8:
        raise RuntimeError(f"Invalid DLC: {dlc}")

    data = read_exact(ser, dlc)

    return can_id, data


def exchange(ser, request, expected, label, retries=MAX_RETRIES):
    for attempt in range(1, retries + 1):
        try:
            send_frame(ser, UDS_REQUEST_ID, request)
            can_id, data = read_frame(ser)

            if can_id != UDS_RESPONSE_ID:
                raise RuntimeError(
                    f"Unexpected CAN ID 0x{can_id:03X}"
                )

            if data != bytes(expected):
                raise RuntimeError(
                    f"Unexpected response: "
                    f"{data.hex(' ').upper()}"
                )

            return

        except (TimeoutError, RuntimeError) as error:
            if attempt >= retries:
                raise

            print(
                f"{label}: {error}, retry "
                f"{attempt}/{retries}"
            )

            ser.reset_input_buffer()
            time.sleep(0.2)


def main():
    firmware, firmware_size, firmware_crc = load_application()
    valid_key = SEED ^ KEY_MASK

    if firmware_size > 0xFFFF:
        raise ValueError(
            "Current compact RequestDownload supports "
            "firmware size up to 65535 bytes"
        )

    print("==============================================")
    print("V6.7 FULL UDS FIRMWARE UPDATE - PROTEUS")
    print("==============================================")
    print()
    print(
        f"Application : 0x{APP_START_ADDRESS:08X} "
        f"-> 0x{APP_START_ADDRESS + firmware_size - 1:08X}"
    )
    print(f"Size        : {firmware_size} bytes")
    print(f"CRC32       : 0x{firmware_crc:08X}")
    print(
        f"Data blocks : "
        f"{(firmware_size + TRANSFER_DATA_BYTES - 1) // TRANSFER_DATA_BYTES}"
    )
    print()

    ser = serial.Serial(
        PORT,
        BAUDRATE,
        bytesize=8,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=SERIAL_TIMEOUT,
    )

    try:
        time.sleep(0.2)
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        print("[1] Programming Session")
        exchange(
            ser,
            [0x02, 0x10, 0x02],
            [0x06, 0x50, 0x02, 0x00, 0x32, 0x01, 0xF4],
            "Programming Session",
        )
        print("    PASS")

        print("[2] SecurityAccess - Request Seed")
        exchange(
            ser,
            [0x02, 0x27, 0x01],
            [0x06, 0x67, 0x01]
            + list(SEED.to_bytes(4, "big")),
            "Request Seed",
        )
        print("    PASS")

        print("[3] SecurityAccess - Send Key")
        exchange(
            ser,
            [0x06, 0x27, 0x02]
            + list(valid_key.to_bytes(4, "big")),
            [0x02, 0x67, 0x02],
            "Send Key",
        )
        print("    PASS")

        print("[4] RequestDownload")
        size_bytes = firmware_size.to_bytes(2, "big")

        exchange(
            ser,
            [
                0x07,
                0x34,
                0x00,
                0x22,
                0x00,
                0x00,
                size_bytes[0],
                size_bytes[1],
            ],
            [0x03, 0x74, 0x10, 0x07],
            "RequestDownload",
        )
        print("    Flash erased -> ACK")
        print()

        print("[5] TransferData")

        total_blocks = (
            firmware_size + TRANSFER_DATA_BYTES - 1
        ) // TRANSFER_DATA_BYTES

        offset = 0
        block_index = 0
        sequence = 1

        while offset < firmware_size:
            chunk = firmware[
                offset:offset + TRANSFER_DATA_BYTES
            ]

            uds_length = 2 + len(chunk)

            request = [
                uds_length,
                0x36,
                sequence,
            ] + list(chunk)

            expected = [
                0x02,
                0x76,
                sequence,
            ]

            exchange(
                ser,
                request,
                expected,
                f"Block {block_index + 1}",
            )

            offset += len(chunk)
            block_index += 1

            if (
                block_index == 1
                or block_index % 50 == 0
                or block_index == total_blocks
            ):
                print(
                    f"    Block {block_index}/{total_blocks} "
                    f"-> ACK"
                )

            sequence = (sequence + 1) & 0xFF

        print()
        print(
            f"    Transferred {offset}/{firmware_size} bytes"
        )
        print()

        print("[6] RequestTransferExit + CRC32")

        crc_bytes = list(
            firmware_crc.to_bytes(4, "big")
        )

        exchange(
            ser,
            [0x05, 0x37] + crc_bytes,
            [0x01, 0x77],
            "RequestTransferExit",
        )

        print("    CRC32 verification    -> OK")
        print("    Application validation -> OK")
        print("    Firmware Header        -> WRITTEN")
        print("    Final validation       -> OK")
        print()

        print("[7] ECU Reset")
        exchange(
            ser,
            [0x02, 0x11, 0x01],
            [0x02, 0x51, 0x01],
            "ECU Reset",
        )

        print("    Positive response -> OK")
        print("    STM32 reset requested")
        print()
        print("==============================================")
        print("V6.7 FULL UDS FIRMWARE UPDATE SUCCESSFUL")
        print("==============================================")
        print()
        print("After reset:")
        print("  - wait about 15 seconds in Proteus")
        print("  - Bootloader validates Header + Vector Table + CRC32")
        print("  - Application should start")
        print("  - PC13 LED should blink")

    finally:
        if ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()
