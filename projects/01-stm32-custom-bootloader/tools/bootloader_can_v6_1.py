#!/usr/bin/env python3
"""STM32F103 Custom Bootloader V6 — CAN firmware sender via PCAN-USB.

Commands (Windows):
    py -m pip install python-can
    py bootloader_can_v6.py plan "Application(1).bin"
    py bootloader_can_v6.py ping
    py bootloader_can_v6_1.py flash "Application_V6_1.bin"

'plan' never opens the CAN interface.
'ping' does not modify Flash.
'flash' asks confirmation, then RESET, then live PING and START during 3-second boot window.

Protocol corresponds to the user's Bootloader V6 main.c (Oct 2026).
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
import sys
import time
import zlib

CAN_COMMAND = 0x600
CAN_DATA = 0x601
CAN_HEADER = 0x602
CAN_RESPONSE = 0x650
APP_START = 0x08004400
APP_END = 0x08010000
SRAM_START = 0x20000000
SRAM_END = 0x20005000
HEADER_START = 0x08004000
FW_MAGIC = 0xB00710AD
FW_VERSION = 0x00010000
PAYLOAD_SIZE = 6
ACK = ord("A")
NACK = ord("N")
ERRORS = {
    0: "Aucune erreur",
    1: "Etat inattendu (STATE)",
    2: "Longueur incorrecte (LENGTH)",
    3: "Sequence incorrecte (SEQUENCE)",
    4: "Erreur Flash (FLASH)",
    5: "En-tete invalide (HEADER)",
    6: "CRC32 incorrect (CRC)",
    7: "Adresse ou taille hors limites (RANGE)",
}


class ProtocolError(RuntimeError):
    pass


def check_firmware(raw: bytes) -> tuple[int, int, bytes]:
    """Check enough of the vector table to catch a wrong .bin or wrong linker."""
    size = len(raw)
    if not (8 <= size <= APP_END - APP_START):
        raise ValueError(f"Taille invalide: {size} octets (max {APP_END - APP_START}).")
    msp, reset_handler = struct.unpack_from("<II", raw)
    reset_address = reset_handler & ~1
    if not (SRAM_START < msp <= SRAM_END) or (msp & 0x7):
        raise ValueError(f"MSP invalide: 0x{msp:08X} (SRAM/alignment).")
    if not (reset_handler & 1) or not (APP_START <= reset_address < APP_START + size):
        raise ValueError(
            f"Reset_Handler invalide: 0x{reset_handler:08X}. "
            "L'application doit etre liee a 0x08004400."
        )
    crc = zlib.crc32(raw) & 0xFFFFFFFF
    header = struct.pack("<IIII", FW_MAGIC, size, crc, FW_VERSION)
    return msp, reset_handler, header


def frames_for_blob(can_id: int, blob: bytes):
    """Yield (arbitration_id, sequence, payload) using 2-byte LE sequence."""
    for seq, offset in enumerate(range(0, len(blob), PAYLOAD_SIZE)):
        if seq > 65535:
            raise ValueError("Numero de sequence depassant uint16.")
        data = struct.pack("<H", seq) + blob[offset : offset + PAYLOAD_SIZE]
        yield (can_id, seq, data)


def print_plan(raw: bytes) -> bytes:
    msp, reset_handler, header = check_firmware(raw)
    crc = struct.unpack_from("<I", header, 8)[0]
    data_count = (len(raw) + PAYLOAD_SIZE - 1) // PAYLOAD_SIZE
    print("\n=== PLAN DE PROGRAMMATION V6 (sans effacement) ===")
    print(f"Firmware: {len(raw)} octets ({len(raw) / 1024:.2f} Kio)")
    print(f"SHA256:  {hashlib.sha256(raw).hexdigest()}")
    print(f"MSP:     0x{msp:08X}")
    print(f"Reset:   0x{reset_handler:08X}")
    print(f"CRC32:   0x{crc:08X} (CRC-32 standard)")
    print(f"Flash application: 0x{APP_START:08X} a 0x{APP_START + len(raw) - 1:08X}")
    print(f"Header a 0x{HEADER_START:08X}: {header.hex(' ').upper()}")
    print(f"Trames DATA: {data_count} sur ID 0x{CAN_DATA:03X}")
    print(f"Trames HEADER: 3 sur ID 0x{CAN_HEADER:03X}")
    print(f"Commande START: 0x{CAN_COMMAND:03X} {bytes([ord('S')]) .hex().upper()} {struct.pack('<I', len(raw)).hex(' ').upper()}")
    print("Effacement prevu uniquement en mode 'flash': 48 pages de 1 Kio")
    print("Zone effacee: 0x08004000 a 0x0800FFFF (header + application)")
    return header


class CANClient:
    def __init__(self, bus, can_module):
        self.bus = bus
        self.can = can_module

    def exchange(
        self,
        can_id: int,
        payload: bytes,
        context: str,
        seq: int = 0,
        timeout: float = 2.0,
        retries: int = 3,
    ) -> None:
        if len(payload) > 8:
            raise ValueError("CAN classique: 8 octets maximum")
        if len(context) != 1:
            raise ValueError("Contexte attendu: caractere unique")
        message = self.can.Message(
            arbitration_id=can_id,
            is_extended_id=False,
            data=payload,
        )
        for attempt in range(1, retries + 1):
            try:
                self.bus.send(message, timeout=1.0)
            except Exception as exc:
                raise ProtocolError(f"Echec emission ID 0x{can_id:03X}: {exc}") from exc
            deadline = time.monotonic() + timeout
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                try:
                    reply = self.bus.recv(timeout=remaining)
                except Exception as exc:
                    raise ProtocolError(f"Echec reception CAN: {exc}") from exc
                if reply is None:
                    break
                if (
                    reply.arbitration_id != CAN_RESPONSE
                    or reply.is_extended_id
                    or getattr(reply, "is_remote_frame", False)
                    or len(reply.data) < 5
                ):
                    continue
                response, command, lo, hi, error = reply.data[:5]
                received_seq = lo | (hi << 8)
                if command != ord(context) or received_seq != seq:
                    continue  # Late ACK of a preceding frame or unrelated message
                if response == ACK and error == 0:
                    return
                if response == NACK:
                    reason = ERRORS.get(error, f"Code 0x{error:02X}")
                    raise ProtocolError(
                        f"NACK contexte {context}, seq={seq}: {reason}"
                    )
                raise ProtocolError(
                    f"Reponse anormale: {bytes(reply.data[:5]).hex(' ')}"
                )
            if attempt < retries:
                print(f"  Timeout ACK {context}, seq={seq}: nouvel essai {attempt + 1}/{retries}")
        raise ProtocolError(
            f"Pas de reponse a {context}, sequence {seq} apres {retries} essai(s)."
        )

    def ping(self) -> None:
        self.exchange(CAN_COMMAND, b"P", "P", timeout=2.0, retries=3)
        print("PING reussi: ACK 0x650 / 41 50 00 00 00")

    def flash(self, raw: bytes, header: bytes) -> None:
        # The app is already valid: the bootloader only listens for ~3 seconds
        # after hardware RESET. Ask permission BEFORE that window opens.
        # No destructive command is transmitted without explicit confirmation.
        print("\nATTENTION: ERASE effacera 0x08004000..0x0800FFFF.")
        print("Le bootloader 0x08000000..0x08003FFF n'est pas vise.")
        confirm = input("Pour autoriser l'effacement, tape exactement EFFACER : ").strip()
        if confirm != "EFFACER":
            print("Annule: aucune commande START/ERASE/DATA transmise.")
            return

        print("\nIMPORTANT: Le bootloader attend ~3 s apres RESET.")
        print("Le CANable doit etre connecte, et PCAN-View deconnecte.")
        input("Appuie sur RESET de la STM32, puis IMMEDIATEMENT sur Entree ici : ")

        # PING and START follow immediately, before the bootloader jumps to app.
        print("PING prealable...")
        self.ping()

        # The firmware size is four little-endian bytes after ASCII 'S'.
        self.exchange(
            CAN_COMMAND, b"S" + struct.pack("<I", len(raw)), "S",
            timeout=2.0, retries=2,
        )
        print("START UPDATE: ACK")
        # An erase acknowledgement may take seconds (48 Flash pages).
        # Never retry ERASE automatically: a lost ACK must be diagnosed.
        self.exchange(CAN_COMMAND, b"R", "R", timeout=20.0, retries=1)
        print("ERASE: ACK. Transfert des octets...")

        total = (len(raw) + PAYLOAD_SIZE - 1) // PAYLOAD_SIZE
        for i, (can_id, seq, payload) in enumerate(frames_for_blob(CAN_DATA, raw), 1):
            self.exchange(can_id, payload, "D", seq, timeout=2.0, retries=3)
            if i % 50 == 0 or i == total:
                print(f"  DATA: {i}/{total} trames acquittees")

        for can_id, seq, payload in frames_for_blob(CAN_HEADER, header):
            self.exchange(can_id, payload, "H", seq, timeout=4.0, retries=3)
        print("HEADER: 3/3 trames acquittees")

        self.exchange(CAN_COMMAND, b"V", "V", timeout=10.0, retries=2)
        print("VERIFY: ACK (controle bootloader de la Flash et du CRC32)")

        # END resets the MCU. Do not repeat this command automatically.
        self.exchange(CAN_COMMAND, b"E", "E", timeout=3.0, retries=1)
        print("END: ACK. La STM32 redemarre; verifier l'application sur la carte.")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="STM32 Bootloader V6 : plan, PING non destructif, transfert CAN."
    )
    parser.add_argument("command", choices=("plan", "ping", "flash"))
    parser.add_argument("firmware", nargs="?", help="Chemin du fichier Application.bin")
    parser.add_argument("--channel", default="PCAN_USBBUS1")
    parser.add_argument("--bitrate", default=500000, type=int)
    args = parser.parse_args()
    if args.command in ("plan", "flash") and not args.firmware:
        parser.error("Le chemin Application.bin est obligatoire pour plan et flash.")
    if args.command == "ping" and args.firmware:
        parser.error("La commande ping ne prend pas de fichier.")

    raw = b""
    header = b""
    if args.command in ("plan", "flash"):
        path = Path(args.firmware)
        if not path.is_file():
            parser.error(f"Fichier introuvable : {path}")
        raw = path.read_bytes()
        header = print_plan(raw)
        if args.command == "plan":
            print("\nPLAN termine: pas d'acces CAN, aucune ecriture Flash.")
            return 0

    try:
        import can  # Install: py -m pip install python-can
    except ImportError:
        print("Module python-can absent. Installer avec: py -m pip install python-can")
        return 2
    try:
        # PCAN-View must be disconnected from this channel first.
        with can.Bus(
            interface="pcan", channel=args.channel, bitrate=args.bitrate
        ) as bus:
            client = CANClient(bus, can)
            if args.command == "ping":
                client.ping()
            else:
                client.flash(raw, header)
    except (ProtocolError, OSError, ValueError, RuntimeError) as exc:
        print(f"\nECHEC: {exc}", file=sys.stderr)
        print(
            "Verifie BOOT0=0, PCAN-View deconnecte, CAN a 500 kbit/s, "
            "alimentation STM32, CANH/CANL/GND et terminaisons.",
            file=sys.stderr,
        )
        if args.command == "flash":
            print(
                "En cas d'interruption apres ERASE, une application incomplete "
                "ne doit pas demarrer (Header/CRC). Conserver le ST-Link disponible.",
                file=sys.stderr,
            )
        return 1
    except KeyboardInterrupt:
        print("\nInterrompu par l'utilisateur. Refaire PING avant reprise.")
        return 130
    return 0


if __name__ == "__main__":
    sys.exit(main())
