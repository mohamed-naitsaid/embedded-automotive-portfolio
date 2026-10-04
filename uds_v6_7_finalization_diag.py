import time
import zlib
import serial
from intelhex import IntelHex

PORT = "COM15"
BAUDRATE = 115200

CAN_SIM_FRAME_START = 0xC5
UDS_REQUEST_ID = 0x7E0
UDS_RESPONSE_ID = 0x7E8

APPLICATION_HEX = (
    "projects/01-stm32-custom-bootloader/"
    "application/Debug/Application.hex"
)

APP_START = 0x08004400

SEED = 0x12345678
KEY_MASK = 0xA5A5A5A5
VALID_KEY = SEED ^ KEY_MASK

BYTE_DELAY = 0.10
TIMEOUT = 10


def build_frame(can_id, data):
    return (
        bytes([CAN_SIM_FRAME_START])
        + can_id.to_bytes(2, "little")
        + bytes([len(data)])
        + bytes(data)
    )


def send_frame(ser, can_id, data):
    frame = build_frame(can_id, data)

    for b in frame:
        ser.write(bytes([b]))
        ser.flush()
        time.sleep(BYTE_DELAY)


def read_exact(ser, n):
    data = ser.read(n)

    if len(data) != n:
        raise TimeoutError(
            f"Expected {n} bytes, got {len(data)}"
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

    can_id = int.from_bytes(header[:2], "little")
    dlc = header[2]
    data = read_exact(ser, dlc)

    return can_id, data


def exchange(ser, label, request, expected):
    print(label)
    print(
        "  TX:",
        " ".join(f"{b:02X}" for b in request)
    )

    send_frame(ser, UDS_REQUEST_ID, request)

    can_id, data = read_frame(ser)

    print(
        f"  RX 0x{can_id:03X}:",
        " ".join(f"{b:02X}" for b in data)
    )

    if can_id != UDS_RESPONSE_ID:
        raise RuntimeError(
            f"Unexpected CAN ID 0x{can_id:03X}"
        )

    if data != bytes(expected):
        raise RuntimeError(
            "Unexpected response. "
            f"Expected {bytes(expected).hex(' ').upper()}"
        )

    print("  PASS\n")


def main():
    app = IntelHex(APPLICATION_HEX)
    app.padding = 0xFF

    firmware = bytes(
        app[address]
        for address in range(
            APP_START,
            APP_START + 12
        )
    )

    crc = zlib.crc32(firmware) & 0xFFFFFFFF

    print("========================================")
    print("V6.7 FINALIZATION DIAGNOSTIC - 12 BYTES")
    print("========================================")
    print()
    print(
        "Data :",
        firmware.hex(" ").upper()
    )
    print(f"CRC32: 0x{crc:08X}")
    print()

    ser = serial.Serial(
        PORT,
        BAUDRATE,
        bytesize=8,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=TIMEOUT,
    )

    try:
        time.sleep(0.2)
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        exchange(
            ser,
            "[1] Programming Session",
            [0x02, 0x10, 0x02],
            [0x06, 0x50, 0x02, 0x00, 0x32, 0x01, 0xF4],
        )

        exchange(
            ser,
            "[2] Request Seed",
            [0x02, 0x27, 0x01],
            [0x06, 0x67, 0x01]
            + list(SEED.to_bytes(4, "big")),
        )

        exchange(
            ser,
            "[3] Send Key",
            [0x06, 0x27, 0x02]
            + list(VALID_KEY.to_bytes(4, "big")),
            [0x02, 0x67, 0x02],
        )

        exchange(
            ser,
            "[4] RequestDownload - 12 bytes",
            [0x07, 0x34, 0x00, 0x22,
             0x00, 0x00, 0x00, 0x0C],
            [0x03, 0x74, 0x10, 0x07],
        )

        for seq in range(1, 4):
            start = (seq - 1) * 4
            chunk = firmware[start:start + 4]

            exchange(
                ser,
                f"[5.{seq}] TransferData block {seq}",
                [0x06, 0x36, seq] + list(chunk),
                [0x02, 0x76, seq],
            )

        exchange(
            ser,
            "[6] RequestTransferExit + CRC32",
            [0x05, 0x37]
            + list(crc.to_bytes(4, "big")),
            [0x01, 0x77],
        )

        print("========================================")
        print("FINALIZATION PATH SUCCESSFUL")
        print("========================================")
        print()
        print(
            "Conclusion: Header writing + CRC finalization work."
        )
        print(
            "The remaining problem is inside the long 4608-byte transfer."
        )

    finally:
        if ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()
