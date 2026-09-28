import time
import serial

PORT = "COM15"
BAUDRATE = 115200

CAN_SIM_FRAME_START = 0xC5
UDS_REQUEST_ID = 0x7E0
UDS_RESPONSE_ID = 0x7E8

UART_BYTE_DELAY = 0.10
SERIAL_TIMEOUT = 10

SEED = 0x12345678
KEY_MASK = 0xA5A5A5A5
VALID_KEY = SEED ^ KEY_MASK


def build_frame(can_id, data):
    return (
        bytes([CAN_SIM_FRAME_START])
        + can_id.to_bytes(2, "little")
        + bytes([len(data)])
        + bytes(data)
    )


def send_frame(ser, can_id, data):
    frame = build_frame(can_id, data)

    for byte in frame:
        ser.write(bytes([byte]))
        ser.flush()
        time.sleep(UART_BYTE_DELAY)


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


def run_test(ser, name, request, expected):
    print(name)
    print(
        "    TX 0x7E0 : "
        + " ".join(f"{b:02X}" for b in request)
    )

    send_frame(ser, UDS_REQUEST_ID, request)
    can_id, data = read_frame(ser)

    print(
        f"    RX 0x{can_id:03X} : "
        + " ".join(f"{b:02X}" for b in data)
    )

    if can_id != UDS_RESPONSE_ID:
        raise RuntimeError(
            f"Expected CAN ID 0x{UDS_RESPONSE_ID:03X}, "
            f"got 0x{can_id:03X}"
        )

    if data != bytes(expected):
        raise RuntimeError(
            "Unexpected UDS response\n"
            f"Expected: {bytes(expected).hex(' ').upper()}\n"
            f"Received: {data.hex(' ').upper()}"
        )

    print("    PASS")
    print()


def u32_be(value):
    return list(value.to_bytes(4, "big"))


def main():
    print("==========================================")
    print("V6.6 UDS RequestTransferExit - Proteus")
    print("==========================================")
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

        run_test(
            ser,
            "[1] RequestTransferExit before download",
            [0x01, 0x37],
            [0x03, 0x7F, 0x37, 0x24],
        )

        run_test(
            ser,
            "[2] Enter Programming Session",
            [0x02, 0x10, 0x02],
            [0x06, 0x50, 0x02, 0x00, 0x32, 0x01, 0xF4],
        )

        run_test(
            ser,
            "[3] SecurityAccess - Request Seed",
            [0x02, 0x27, 0x01],
            [0x06, 0x67, 0x01] + u32_be(SEED),
        )

        run_test(
            ser,
            "[4] SecurityAccess - Send Key",
            [0x06, 0x27, 0x02] + u32_be(VALID_KEY),
            [0x02, 0x67, 0x02],
        )

        # 8 bytes total, programmed in two 4-byte blocks.
        run_test(
            ser,
            "[5] RequestDownload - 8 bytes",
            [0x07, 0x34, 0x00, 0x22, 0x00, 0x00, 0x00, 0x08],
            [0x03, 0x74, 0x10, 0x07],
        )

        run_test(
            ser,
            "[6] TransferData block 1",
            [0x06, 0x36, 0x01, 0x11, 0x22, 0x33, 0x44],
            [0x02, 0x76, 0x01],
        )

        # Only 4/8 bytes received -> TransferExit must fail.
        run_test(
            ser,
            "[7] RequestTransferExit before all bytes",
            [0x01, 0x37],
            [0x03, 0x7F, 0x37, 0x24],
        )

        run_test(
            ser,
            "[8] TransferData block 2 - final",
            [0x06, 0x36, 0x02, 0x55, 0x66, 0x77, 0x88],
            [0x02, 0x76, 0x02],
        )

        run_test(
            ser,
            "[9] Valid RequestTransferExit",
            [0x01, 0x37],
            [0x01, 0x77],
        )

        # The transfer has been closed. More data must be rejected.
        run_test(
            ser,
            "[10] TransferData after TransferExit",
            [0x06, 0x36, 0x03, 0x99, 0xAA, 0xBB, 0xCC],
            [0x03, 0x7F, 0x36, 0x24],
        )

        run_test(
            ser,
            "[11] Second RequestTransferExit",
            [0x01, 0x37],
            [0x03, 0x7F, 0x37, 0x24],
        )

        print("Download address : 0x08004400")
        print("Download size    : 8 bytes")
        print("Transferred      : 8 bytes")
        print("Transfer status  : CLOSED")
        print()
        print("==========================================")
        print("V6.6 UDS REQUEST TRANSFER EXIT SUCCESSFUL")
        print("==========================================")

    finally:
        if ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()
