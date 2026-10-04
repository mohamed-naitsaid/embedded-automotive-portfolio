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

    if dlc > 8:
        raise RuntimeError(f"Invalid DLC: {dlc}")

    data = read_exact(ser, dlc)
    return can_id, data


def exchange(ser, label, request, expected):
    print(label)
    print("  TX:", " ".join(f"{b:02X}" for b in request))

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
            "Unexpected response. Expected "
            + bytes(expected).hex(" ").upper()
        )

    print("  PASS\n")


def main():
    app = IntelHex(APPLICATION_HEX)
    app.padding = 0xFF

    expected = bytes(
        app[address]
        for address in range(APP_START, APP_START + 12)
    )

    expected_crc = zlib.crc32(expected) & 0xFFFFFFFF

    print("==========================================")
    print("V6.7 RAW FLASH DIAGNOSTIC - 12 BYTES")
    print("==========================================")
    print()
    print("Expected bytes :", expected.hex(" ").upper())
    print(f"Expected CRC32 : 0x{expected_crc:08X}")
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
            chunk = expected[start:start + 4]

            exchange(
                ser,
                f"[5.{seq}] TransferData block {seq}",
                [0x06, 0x36, seq] + list(chunk),
                [0x02, 0x76, seq],
            )

        request_exit = (
            [0x05, 0x37]
            + list(expected_crc.to_bytes(4, "big"))
        )

        flash_bytes = bytearray()

        for read_index in range(3):
            print(
                f"[6.{read_index + 1}] "
                "RequestTransferExit + read Flash"
            )

            print(
                "  TX:",
                " ".join(f"{b:02X}" for b in request_exit)
            )

            send_frame(
                ser,
                UDS_REQUEST_ID,
                request_exit
            )

            can_id, data = read_frame(ser)

            print(
                f"  RX 0x{can_id:03X}:",
                " ".join(f"{b:02X}" for b in data)
            )

            if (
                can_id != UDS_RESPONSE_ID
                or len(data) != 8
                or data[:4] != bytes([0x07, 0x7F, 0x37, 0x74])
            ):
                raise RuntimeError(
                    "Unexpected Flash diagnostic response: "
                    + data.hex(" ").upper()
                )

            flash_bytes.extend(data[4:8])
            print()

        actual = bytes(flash_bytes)
        actual_crc = zlib.crc32(actual) & 0xFFFFFFFF

        print("==========================================")
        print("RAW FLASH RESULT")
        print("==========================================")
        print()
        print("Expected :", expected.hex(" ").upper())
        print("Flash    :", actual.hex(" ").upper())
        print()
        print(f"Expected CRC : 0x{expected_crc:08X}")
        print(f"Flash CRC    : 0x{actual_crc:08X}")
        print()

        if actual == expected:
            print("FLASH BYTES MATCH EXACTLY")
            print(
                "The remaining issue is inside the STM32 CRC "
                "calculation path."
            )
        else:
            print("FLASH BYTES DO NOT MATCH")
            print("Different byte positions:")

            for i, (a, b) in enumerate(zip(expected, actual)):
                if a != b:
                    print(
                        f"  +0x{i:02X}: "
                        f"expected {a:02X}, Flash {b:02X}"
                    )

    finally:
        if ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()
