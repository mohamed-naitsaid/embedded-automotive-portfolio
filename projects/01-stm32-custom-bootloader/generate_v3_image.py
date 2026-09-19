from intelhex import IntelHex
import struct
import zlib

BOOTLOADER_HEX = "bootloader/Debug/Bootloader.hex"
APPLICATION_HEX = "application/Debug/Application.hex"
OUTPUT_HEX = "Bootloader_Application_V3.hex"

APP_HEADER_ADDRESS = 0x08004000
APP_START_ADDRESS = 0x08004400
APP_END_ADDRESS = 0x08010000

FW_MAGIC_NUMBER = 0xB00710AD
FW_VERSION = 0x00010000


# ------------------------------------------------------------
# Load Bootloader and Application HEX files
# ------------------------------------------------------------

bootloader = IntelHex(BOOTLOADER_HEX)
application = IntelHex(APPLICATION_HEX)

# Remove Intel HEX execution metadata before merging
bootloader.start_addr = None
application.start_addr = None

# Empty Flash bytes are considered 0xFF
application.padding = 0xFF


# ------------------------------------------------------------
# Verify Application location
# ------------------------------------------------------------

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
        f"outside allowed Flash region"
    )


# ------------------------------------------------------------
# Calculate real firmware size
# ------------------------------------------------------------

firmware_size = app_max - APP_START_ADDRESS + 1

max_firmware_size = APP_END_ADDRESS - APP_START_ADDRESS

if firmware_size <= 0:
    raise ValueError("Firmware size is invalid")

if firmware_size > max_firmware_size:
    raise ValueError("Firmware does not fit inside Application region")


# ------------------------------------------------------------
# Build exact firmware byte sequence
# ------------------------------------------------------------

firmware_data = bytes(
    application[address]
    for address in range(
        APP_START_ADDRESS,
        APP_START_ADDRESS + firmware_size
    )
)


# ------------------------------------------------------------
# Calculate CRC32
# ------------------------------------------------------------

firmware_crc32 = zlib.crc32(firmware_data) & 0xFFFFFFFF


# ------------------------------------------------------------
# Build Firmware Header
#
# 0x08004000 -> Magic Number
# 0x08004004 -> Firmware Size
# 0x08004008 -> CRC32
# 0x0800400C -> Version
# ------------------------------------------------------------

header = struct.pack(
    "<IIII",
    FW_MAGIC_NUMBER,
    firmware_size,
    firmware_crc32,
    FW_VERSION
)


# ------------------------------------------------------------
# Create final Flash image
# ------------------------------------------------------------

final_image = IntelHex()

final_image.merge(
    bootloader,
    overlap="error"
)

final_image.puts(
    APP_HEADER_ADDRESS,
    header
)

final_image.merge(
    application,
    overlap="error"
)

final_image.start_addr = None

final_image.write_hex_file(
    OUTPUT_HEX
)


# ------------------------------------------------------------
# Display generated information
# ------------------------------------------------------------

print()
print("========================================")
print(" STM32 CUSTOM BOOTLOADER - V3 IMAGE")
print("========================================")
print()
print(f"Magic Number    : 0x{FW_MAGIC_NUMBER:08X}")
print(f"Firmware Size   : {firmware_size} bytes")
print(f"CRC32           : 0x{firmware_crc32:08X}")
print(f"Version         : 0x{FW_VERSION:08X}")
print()
print(f"Header Address  : 0x{APP_HEADER_ADDRESS:08X}")
print(f"Application     : 0x{APP_START_ADDRESS:08X}")
print(f"Application End : 0x{app_max:08X}")
print()
print(f"Output          : {OUTPUT_HEX}")
print()
print("V3 firmware image generated successfully.")