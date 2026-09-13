from pathlib import Path

bootloader = Path("bootloader/Debug/Bootloader.hex")
application = Path("application/Debug/Application.hex")
output = Path("STM32_Bootloader_Application.hex")

def read_hex_without_eof(path):
    lines = path.read_text().splitlines()
    return [line for line in lines if not line.startswith(":00000001")]

merged = []
merged.extend(read_hex_without_eof(bootloader))
merged.extend(read_hex_without_eof(application))
merged.append(":00000001FF")

output.write_text("\n".join(merged) + "\n")

print(f"Created: {output}")