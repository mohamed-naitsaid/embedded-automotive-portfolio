#!/usr/bin/env python3
"""STM32F103 UDS-over-CAN firmware sender for the project's custom bootloader.

Protocol: proprietary single-frame UDS subset on classical CAN, not ISO-TP
multi-frame and not a production UDS security implementation.

Usage from the repository root (Windows / Git Bash):
  uv run --no-project python projects/01-stm32-custom-bootloader/tools/bootloader_uds_can.py plan projects/01-stm32-custom-bootloader/firmware/Application_V6_1.bin
  uv run --no-project --with python-can python projects/01-stm32-custom-bootloader/tools/bootloader_uds_can.py flash projects/01-stm32-custom-bootloader/firmware/Application_V6_1.bin

IMPORTANT: The flash subcommand ERASES the complete application/header region.
The plan subcommand does not connect to hardware and never writes Flash.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
import sys
import time
import zlib

REQUEST_ID = 0x7E0
RESPONSE_ID = 0x7E8
APP_START = 0x08004400
APP_END = 0x08010000
HEADER_START = 0x08004000
FLASH_ERASE_START = 0x08004000
FLASH_ERASE_END = 0x0800FFFF
SRAM_START = 0x20000000
SRAM_END = 0x20005000
BLOCK_PAYLOAD_BYTES = 5
SEED_KEY_XOR_MASK = 0xA5A5A5A5  # Educational only; fixed mask is insecure.

NRC = {
    0x11: "ServiceNotSupported",
    0x12: "SubFunctionNotSupported",
    0x13: "IncorrectMessageLengthOrInvalidFormat",
    0x22: "ConditionsNotCorrect",
    0x24: "RequestSequenceError",
    0x31: "RequestOutOfRange",
    0x33: "SecurityAccessDenied",
    0x35: "InvalidKey",
    0x72: "GeneralProgrammingFailure",
    0x73: "WrongBlockSequenceCounter",
}


class UpdateError(RuntimeError):
    """Protocol, transport, or verification failure."""


def check_firmware(firmware: bytes) -> tuple[int, int, int]:
    """Basic checks of Cortex-M3 vectors and expected Flash partition."""
    n = len(firmware)
    if not 8 <= n <= APP_END - APP_START:
        raise ValueError(f"Taille invalide ({n} octets); max {APP_END - APP_START}.")
    msp, reset_handler = struct.unpack_from("<II", firmware)
    if not (SRAM_START < msp <= SRAM_END) or msp & 7:
        raise ValueError(f"MSP invalide: 0x{msp:08X}")
    reset_address = reset_handler & ~1
    if not reset_handler & 1 or not APP_START <= reset_address < APP_START + n:
        raise ValueError(f"Reset_Handler invalide: 0x{reset_handler:08X}")
    crc32 = zlib.crc32(firmware) & 0xFFFFFFFF
    return msp, reset_handler, crc32


def build_data_blocks(firmware: bytes):
    """Yield 1-based block number, 8-bit BSC, data bytes, full CAN payload."""
    for block_no, offset in enumerate(range(0, len(firmware), BLOCK_PAYLOAD_BYTES), 1):
        sequence = block_no & 0xFF  # 1..255,0,1.. as in uds.c uint8_t
        chunk = firmware[offset: offset + BLOCK_PAYLOAD_BYTES]
        uds = bytes([0x36, sequence]) + chunk
        frame_data = bytes([len(uds)]) + uds  # ISO-TP single-frame PCI
        yield block_no, sequence, chunk, frame_data


def plan(firmware: bytes) -> int:
    msp, reset_handler, crc32 = check_firmware(firmware)
    n = len(firmware)
    count = (n + BLOCK_PAYLOAD_BYTES - 1) // BLOCK_PAYLOAD_BYTES
    request_download = bytes([0x07, 0x34, 0x00, 0x22, 0x00, 0x00]) + n.to_bytes(2, "big")
    exit_request = bytes([0x05, 0x37]) + crc32.to_bytes(4, "big")
    first = next(build_data_blocks(firmware))[3]
    last = None
    for _, _, _, data in build_data_blocks(firmware):
        last = data
    print("\n=== PLAN UDS-CAN — AUCUNE ECRITURE FLASH ===")
    print(f"Firmware:        {n} octets ({n / 1024:.2f} Kio)")
    print(f"SHA256:          {hashlib.sha256(firmware).hexdigest()}")
    print(f"MSP:             0x{msp:08X}")
    print(f"Reset_Handler:   0x{reset_handler:08X}")
    print(f"CRC32:           0x{crc32:08X}")
    print(f"Zone binaire:    0x{APP_START:08X}..0x{APP_START+n-1:08X}")
    print(f"Zone effacement: 0x{FLASH_ERASE_START:08X}..0x{FLASH_ERASE_END:08X} (FLASH seulement)")
    print(f"Entete ecrit a:  0x{HEADER_START:08X} (par le bootloader apres CRC)")
    print(f"Trames 0x36:     {count}, {BLOCK_PAYLOAD_BYTES} octets de donnees max par bloc")
    print(f"RequestDownload: {request_download.hex(' ').upper()} sur CAN ID 0x{REQUEST_ID:03X}")
    print(f"Premier bloc:    {first.hex(' ').upper()}")
    print(f"Dernier bloc:    {last.hex(' ').upper()}")
    print(f"TransferExit:    {exit_request.hex(' ').upper()}")
    print("Session:         02 10 02 -> 06 50 02 00 32 01 F4")
    print("Seed:            02 27 01 -> 06 67 01 [seed32]")
    print("Key:             seed XOR 0xA5A5A5A5 (demo uniquement)")
    print("RequestDownload ACK attendu: 03 74 10 07")
    print("TransferData ACK attendu:    02 76 [BSC]")
    print("TransferExit ACK attendu:    01 77")
    print("ECUReset ACK attendu:        02 51 01")
    print("\nPLAN TERMINE : aucun acces CAN, aucune commande d'effacement.\n")
    return count


class UDSCAN:
    def __init__(self, bus, can_module):
        self.bus = bus
        self.can = can_module

    def _send_once(self, uds: bytes) -> None:
        if not 1 <= len(uds) <= 7:
            raise ValueError("Ce bootloader n'accepte que les trames UDS simples (1..7 octets UDS).")
        data = bytes([len(uds)]) + uds
        message = self.can.Message(arbitration_id=REQUEST_ID, is_extended_id=False, data=data)
        try:
            self.bus.send(message, timeout=1.0)
        except Exception as exc:
            raise UpdateError(f"Erreur emission CAN: {exc}") from exc

    def exchange(self, uds: bytes, expected: bytes | None = None, *, timeout: float = 2.0,
                 retries: int = 1) -> bytes:
        """One request, one positive response. Retries only when caller explicitly permits.

        Avoid automatic retries for RequestDownload, RequestTransferExit or ECUReset.
        TransferData repetitions are supported by the bootloader's last-BSC ACK logic.
        """
        sid = uds[0]
        for attempt in range(1, retries + 1):
            self._send_once(uds)
            deadline = time.monotonic() + timeout
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                try:
                    received = self.bus.recv(timeout=remaining)
                except Exception as exc:
                    raise UpdateError(f"Erreur reception CAN: {exc}") from exc
                if received is None:
                    break
                if (received.arbitration_id != RESPONSE_ID or received.is_extended_id
                        or getattr(received, "is_remote_frame", False)):
                    continue
                payload = bytes(received.data)
                if not 2 <= len(payload) <= 8 or payload[0] != len(payload) - 1:
                    continue
                uds_reply = payload[1:]
                if (len(uds_reply) == 3 and uds_reply[0] == 0x7F and uds_reply[1] == sid):
                    code = uds_reply[2]
                    raise UpdateError(f"NRC {sid:02X}/{code:02X} ({NRC.get(code, 'inconnu')})")
                if uds_reply[0] != sid + 0x40:
                    continue  # Old/other response.
                if expected is not None and uds_reply != expected:
                    # On TransferData old acknowledgements can arrive after a retry.
                    if sid == 0x36 and uds_reply[:1] == b'\x76':
                        continue
                    raise UpdateError(
                        f"Reponse anormale pour 0x{sid:02X}: {payload.hex(' ').upper()}"
                    )
                return uds_reply
            if attempt < retries:
                print(f"  Timeout SID 0x{sid:02X}: essai {attempt + 1}/{retries}")
        raise UpdateError(f"Pas de reponse SID 0x{sid:02X} apres {retries} essai(s)")


def flash(bus, can_module, firmware: bytes) -> None:
    _, _, crc32 = check_firmware(firmware)
    client = UDSCAN(bus, can_module)
    print("\nCAN pret. Le bootloader ecoute au RESET pendant ~3 secondes.")
    input("Appuie sur RESET de la Blue Pill puis immediatement Entree ici : ")

    print("[1/6] Programming Session ...")
    client.exchange(b"\x10\x02", b"\x50\x02\x00\x32\x01\xF4", timeout=1.4, retries=2)
    print("      PASS — Bootloader reste actif")

    print("[2/6] Request Seed / Send Key ...")
    seed_reply = client.exchange(b"\x27\x01", timeout=2.0)
    if len(seed_reply) != 6 or seed_reply[:2] != b"\x67\x01":
        raise UpdateError(f"Seed anormal: {seed_reply.hex(' ').upper()}")
    seed = int.from_bytes(seed_reply[2:], 'big')
    if seed == 0:
        raise UpdateError("Seed nul inattendu apres RESET: verifier l'etat UDS")
    key = (seed ^ SEED_KEY_XOR_MASK).to_bytes(4, "big")
    client.exchange(b"\x27\x02" + key, b"\x67\x02", timeout=2.0)
    print("      PASS — acces pedagogique de programme debloque")

    print("[3/6] RequestDownload — EFFACEMENT APPLICATION en cours ...")
    request = b"\x34\x00\x22\x00\x00" + len(firmware).to_bytes(2, "big")
    client.exchange(request, b"\x74\x10\x07", timeout=25.0, retries=1)
    print("      PASS — effacement confirme par l'ECU")

    print("[4/6] TransferData ...")
    total = (len(firmware) + BLOCK_PAYLOAD_BYTES - 1) // BLOCK_PAYLOAD_BYTES
    for n, bsc, chunk, _ in build_data_blocks(firmware):
        client.exchange(bytes([0x36, bsc]) + chunk,
                        bytes([0x76, bsc]), timeout=2.0, retries=3)
        if n == 1 or n % 50 == 0 or n == total:
            print(f"      {n}/{total} blocs ACK")

    print("[5/6] RequestTransferExit + controle CRC32 ...")
    client.exchange(b"\x37" + crc32.to_bytes(4, "big"), b"\x77",
                    timeout=10.0, retries=1)
    print(f"      PASS — CRC32 0x{crc32:08X} confirme par la reponse positive ECU")

    print("[6/6] ECUReset ...")
    try:
        client.exchange(b"\x11\x01", b"\x51\x01", timeout=2.0, retries=1)
        print("      PASS — reponse reset positive")
    except UpdateError as exc:
        print(f"      ATTENTION: {exc}. Le reset peut avoir ete execute avant reception ACK.")
        print("      Ne relance pas le flash automatiquement. Controle d'abord la LED PC13.")
    print("\nFIN: attendre le redemarrage, puis verifier PC13 et l'integrite du firmware.\n")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="STM32F103 UDS CAN sender (custom single frames)")
    parser.add_argument("command", choices=["plan", "flash"])
    parser.add_argument("firmware", type=Path)
    parser.add_argument("--channel", default="PCAN_USBBUS1")
    parser.add_argument("--bitrate", type=int, default=500000)
    args = parser.parse_args(argv)

    try:
        data = args.firmware.read_bytes()
        plan(data)  # Validation is mandatory in both modes.
        if args.command == "plan":
            return 0

        print("\nDANGER: la commande flash efface la zone Flash 0x08004000..0x0800FFFF.")
        print("Elle peut rendre l'application inexecutable en cas de coupure, CAN perdu ou erreur.")
        print("Le Bootloader 0x08000000..0x08003FFF doit rester intact, mais garde ST-Link pret.")
        print("Ferme PCAN-View (deconnecte canal) et STM32CubeProgrammer; stabilise l'alimentation.")
        confirm = input("Pour autoriser explicitement l'EFFACEMENT de l'application, tape EFFACER : ").strip()
        if confirm != "EFFACER":
            print("Operation annulee avant toute connexion CAN destructive.")
            return 0
        try:
            import can
        except ImportError as exc:
            raise UpdateError("Module python-can absent. Utilise: uv run --no-project --with python-can python ...") from exc
        with can.Bus(interface="pcan", channel=args.channel, bitrate=args.bitrate) as bus:
            flash(bus, can, data)
        return 0
    except (OSError, ValueError, UpdateError) as exc:
        print(f"\nECHEC: {exc}", file=sys.stderr)
        if args.command == "flash":
            print("NE PAS reprogrammer le bootloader a l'aveugle. Si ERASE a commence,"
                  " recuperer avec ST-Link ou retransferer un firmware valide.", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\nInterrompu. Si ERASE avait commence, l'application peut etre invalide.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
