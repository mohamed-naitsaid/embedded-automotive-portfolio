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
    for b in build_frame(can_id, data):
        ser.write(bytes([b]))
        ser.flush()
        time.sleep(BYTE_DELAY)


def read_exact(ser, n):
    data = ser.read(n)
    if len(data) != n:
        raise TimeoutError(f"Expected {n} bytes, got {len(data)}")
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


def exchange(ser, request, expected, label):
    print(label)
    print("  TX:", " ".join(f"{b:02X}" for b in request))
    send_frame(ser, UDS_REQUEST_ID, request)
    can_id, data = read_frame(ser)
    print(f"  RX 0x{can_id:03X}:", " ".join(f"{b:02X}" for b in data))

    if can_id != UDS_RESPONSE_ID or data != bytes(expected):
        raise RuntimeError(
            "Unexpected response: " + data.hex(" ").upper()
        )

    print("  PASS\n")


def main():
    app = IntelHex(APPLICATION_HEX)
    app.padding = 0xFF

    fw = bytes(app[a] for a in range(APP_START, APP_START + 12))
    crc = zlib.crc32(fw) & 0xFFFFFFFF

    print("==========================================")
    print("V6.7 CRC O0 CHECK - 12 BYTES")
    print("==========================================")
    print("Data :", fw.hex(" ").upper())
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
            [0x02, 0x10, 0x02],
            [0x06, 0x50, 0x02, 0x00, 0x32, 0x01, 0xF4],
            "[1] Programming Session",
        )

        exchange(
            ser,
            [0x02, 0x27, 0x01],
            [0x06, 0x67, 0x01]
            + list(SEED.to_bytes(4, "big")),
            "[2] Request Seed",
        )

        exchange(
            ser,
            [0x06, 0x27, 0x02]
            + list(VALID_KEY.to_bytes(4, "big")),
            [0x02, 0x67, 0x02],
            "[3] Send Key",
        )

        exchange(
            ser,
            [0x07, 0x34, 0x00, 0x22,
             0x00, 0x00, 0x00, 0x0C],
            [0x03, 0x74, 0x10, 0x07],
            "[4] RequestDownload",
        )

        for seq in range(1, 4):
            chunk = fw[(seq - 1) * 4:seq * 4]
            exchange(
                ser,
                [0x06, 0x36, seq] + list(chunk),
                [0x02, 0x76, seq],
                f"[5.{seq}] TransferData block {seq}",
            )

        request = [0x05, 0x37] + list(crc.to_bytes(4, "big"))

        print("[6] RequestTransferExit + CRC32")
        print("  TX:", " ".join(f"{b:02X}" for b in request))
        send_frame(ser, UDS_REQUEST_ID, request)

        can_id, data = read_frame(ser)
        print(f"  RX 0x{can_id:03X}:", " ".join(f"{b:02X}" for b in data))
        print()

        if data == bytes([0x01, 0x77]):
            print("==========================================")
            print("CRC O0 CHECK SUCCESSFUL")
            print("==========================================")
            print("The CRC path now matches the firmware CRC.")
        else:
            print("CRC O0 CHECK FAILED")
            print("Response:", data.hex(" ").upper())

    finally:
        if ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()
