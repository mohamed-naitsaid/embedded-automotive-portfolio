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

# Proteus COMPIM needs paced bytes. A burst can lose the simulated CAN frame.
UART_BYTE_DELAY = 0.10
UART_FRAME_GAP = 0.02
SERIAL_TIMEOUT = 30
MAX_RETRIES = 5
RETRY_DELAY = 0.5

APP_HEADER_ADDRESS = 0x08004000
APP_START_ADDRESS = 0x08004400
APP_END_ADDRESS = 0x08010000

FW_MAGIC_NUMBER = 0xB00710AD
FW_VERSION = 0x00010000

CAN_SIM_FRAME_START = 0xC5

CAN_ID_COMMAND = 0x600
CAN_ID_DATA = 0x601
CAN_ID_HEADER = 0x602
CAN_ID_RESPONSE = 0x650

CAN_DATA_PAYLOAD_SIZE = 6

CMD_PING = ord("P")
CMD_START_UPDATE = ord("S")
CMD_ERASE = ord("R")
CMD_DATA = ord("D")
CMD_HEADER = ord("H")
CMD_VERIFY = ord("V")
CMD_END_UPDATE = ord("E")

BL_ACK = ord("A")
BL_NACK = ord("N")

ERROR_NAMES = {
    0x00: "NONE",
    0x01: "STATE",
    0x02: "LENGTH",
    0x03: "SEQUENCE",
    0x04: "FLASH",
    0x05: "HEADER",
    0x06: "CRC",
    0x07: "RANGE",
}


def load_firmware():
    application = IntelHex(APPLICATION_HEX)
    application.start_addr = None
    application.padding = 0xFF

    app_min = application.minaddr()
    app_max = application.maxaddr()

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

    return (
        app_min,
        app_max,
        firmware_size,
        firmware_data,
        firmware_crc32,
        firmware_header,
    )


def build_frame(can_id, data):
    if can_id > 0x7FF:
        raise ValueError("Only standard 11-bit CAN IDs are supported.")

    if len(data) > 8:
        raise ValueError("CAN payload cannot exceed 8 bytes.")

    return (
        bytes([CAN_SIM_FRAME_START])
        + struct.pack("<H", can_id)
        + bytes([len(data)])
        + data
    )


def read_exact(ser, length):
    data = ser.read(length)

    if len(data) != length:
        raise TimeoutError(
            f"Expected {length} bytes, received {len(data)}."
        )

    return data


def read_frame(ser):
    while True:
        marker = ser.read(1)

        if not marker:
            raise TimeoutError("No response from STM32.")

        if marker[0] == CAN_SIM_FRAME_START:
            break

    header = read_exact(ser, 3)
    can_id = header[0] | (header[1] << 8)
    dlc = header[2]

    if dlc > 8:
        raise RuntimeError(f"Invalid response DLC: {dlc}")

    data = read_exact(ser, dlc)
    return can_id, data


def send_frame(ser, can_id, data):
    frame = build_frame(can_id, data)

    # Send one byte at a time for reliable COMPIM timing in Proteus.
    for byte in frame:
        ser.write(bytes([byte]))
        ser.flush()
        time.sleep(UART_BYTE_DELAY)

    time.sleep(UART_FRAME_GAP)


def expect_response(ser, context, sequence=0):
    can_id, data = read_frame(ser)

    if can_id != CAN_ID_RESPONSE:
        raise RuntimeError(
            f"Unexpected response ID 0x{can_id:03X}"
        )

    if len(data) != 5:
        raise RuntimeError(
            f"Unexpected response DLC {len(data)}"
        )

    response = data[0]
    rx_context = data[1]
    rx_sequence = data[2] | (data[3] << 8)
    error = data[4]

    if rx_context != context:
        raise RuntimeError(
            f"Wrong context: 0x{rx_context:02X}, "
            f"expected 0x{context:02X}"
        )

    if rx_sequence != sequence:
        raise RuntimeError(
            f"Wrong sequence: {rx_sequence}, "
            f"expected {sequence}"
        )

    if response != BL_ACK:
        error_name = ERROR_NAMES.get(error, "UNKNOWN")
        raise RuntimeError(
            f"NACK: context=0x{context:02X}, "
            f"sequence={sequence}, "
            f"error=0x{error:02X} ({error_name})"
        )

    return error


def send_command(ser, command, extra=b""):
    send_frame(
        ser,
        CAN_ID_COMMAND,
        bytes([command]) + extra
    )

    expect_response(
        ser,
        command,
        0
    )



def send_and_expect(ser, can_id, data, context, sequence=0, label="frame"):
    last_error = None

    for attempt in range(1, MAX_RETRIES + 1):
        try:
            send_frame(ser, can_id, data)
            expect_response(ser, context, sequence)
            return
        except TimeoutError as error:
            last_error = error
            print(
                f"    {label}: timeout, retry "
                f"{attempt}/{MAX_RETRIES}"
            )
            time.sleep(RETRY_DELAY)

    raise TimeoutError(
        f"{label}: no response after {MAX_RETRIES} retries. "
        f"Last error: {last_error}"
    )


def send_command_with_retry(ser, command, extra=b"", label="command"):
    send_and_expect(
        ser,
        CAN_ID_COMMAND,
        bytes([command]) + extra,
        command,
        0,
        label
    )


def main():
    (
        app_min,
        app_max,
        firmware_size,
        firmware_data,
        firmware_crc32,
        firmware_header,
    ) = load_firmware()

    number_of_data_frames = (
        firmware_size + CAN_DATA_PAYLOAD_SIZE - 1
    ) // CAN_DATA_PAYLOAD_SIZE

    print("V5 CAN Firmware Update - Proteus Simulation")
    print("==========================================")
    print(f"Application : 0x{app_min:08X} -> 0x{app_max:08X}")
    print(f"Size        : {firmware_size} bytes")
    print(f"CRC32       : 0x{firmware_crc32:08X}")
    print(f"Version     : 0x{FW_VERSION:08X}")
    print(f"CAN frames  : {number_of_data_frames}")
    print()

    ser = serial.Serial(
        port=PORT,
        baudrate=BAUDRATE,
        bytesize=8,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=SERIAL_TIMEOUT
    )

    try:
        time.sleep(0.2)
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        print(f"Serial port opened: {PORT}")
        print()

        print(f"COMPIM pacing : {UART_BYTE_DELAY * 1000:.0f} ms/byte")
        print(f"Serial timeout: {SERIAL_TIMEOUT} s")
        print(f"Retries       : {MAX_RETRIES}")
        print()
        print("[1] CAN PING")
        send_command_with_retry(ser, CMD_PING, label="PING")
        print("    ID 0x600 / P -> ACK on ID 0x650")

        print("[2] START UPDATE")
        send_command_with_retry(
            ser,
            CMD_START_UPDATE,
            struct.pack("<I", firmware_size),
            label="START"
        )
        print("    START -> ACK")

        print("[3] ERASE")
        send_command_with_retry(ser, CMD_ERASE, label="ERASE")
        print("    ERASE -> ACK")

        print("[4] PROGRAM APPLICATION")
        for sequence in range(number_of_data_frames):
            start = sequence * CAN_DATA_PAYLOAD_SIZE
            end = min(
                start + CAN_DATA_PAYLOAD_SIZE,
                firmware_size
            )

            payload = firmware_data[start:end]

            frame_data = (
                struct.pack("<H", sequence)
                + payload
            )

            send_and_expect(
                ser,
                CAN_ID_DATA,
                frame_data,
                CMD_DATA,
                sequence,
                label=f"DATA sequence {sequence}"
            )

            if (
                sequence == 0
                or (sequence + 1) % 50 == 0
                or sequence == number_of_data_frames - 1
            ):
                print(
                    f"    Frame {sequence + 1}/"
                    f"{number_of_data_frames} -> ACK"
                )

        print("[5] PROGRAM HEADER")
        header_sequence = 0
        offset = 0

        while offset < len(firmware_header):
            payload = firmware_header[
                offset:offset + CAN_DATA_PAYLOAD_SIZE
            ]

            frame_data = (
                struct.pack("<H", header_sequence)
                + payload
            )

            send_and_expect(
                ser,
                CAN_ID_HEADER,
                frame_data,
                CMD_HEADER,
                header_sequence,
                label=f"HEADER sequence {header_sequence}"
            )

            print(
                f"    Header frame {header_sequence} -> ACK"
            )

            offset += len(payload)
            header_sequence += 1

        print("[6] VERIFY")
        send_command_with_retry(ser, CMD_VERIFY, label="VERIFY")
        print("    Header validation      -> OK")
        print("    Application validation -> OK")
        print("    CRC32 verification     -> OK")

        print("[7] END UPDATE")
        # Allow extra time because the simulated STM32 resets after ACK.
        # END is intentionally sent once: after a successful END the MCU resets.
        ser.timeout = 30
        send_frame(
            ser,
            CAN_ID_COMMAND,
            bytes([CMD_END_UPDATE])
        )
        expect_response(
            ser,
            CMD_END_UPDATE,
            0
        )
        print("    END UPDATE -> ACK")
        print("    STM32 reset requested")

        print()
        print("==========================================")
        print("V5 CAN UPDATE SUCCESSFUL")
        print("==========================================")
        print(f"Application : 0x{APP_START_ADDRESS:08X} -> 0x{app_max:08X}")
        print(f"Header      : 0x{APP_HEADER_ADDRESS:08X} -> 0x{APP_HEADER_ADDRESS + 15:08X}")
        print(f"Firmware    : {firmware_size} bytes")
        print(f"CRC32       : 0x{firmware_crc32:08X}")
        print(f"Version     : 0x{FW_VERSION:08X}")

    finally:
        if ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()
