# STM32F103 Custom Bootloader

Custom bootloader implementation for the **STM32F103C8T6 (ARM Cortex-M3)**.

The objective of this project is to understand and implement the complete embedded boot process step by step, from a basic bootloader-to-application jump to firmware validation, integrity checking, communication-based firmware updates, and automotive diagnostic bootloader concepts.

The project is developed incrementally through multiple versions.

---

## Project Objectives

- Understand the STM32 boot sequence
- Understand Flash and SRAM memory organization
- Configure separate Bootloader and Application memory regions
- Understand the Cortex-M Vector Table
- Understand MSP and Reset Handler behavior
- Relocate the Application Vector Table
- Validate an Application before execution
- Add firmware metadata
- Verify firmware integrity using CRC32
- Implement firmware update over UART
- Extend firmware update over CAN
- Explore automotive diagnostic bootloader concepts using UDS

---

# Development Status

## ✅ V1 — Bootloader to Application Jump — Completed

The first version implements the basic transfer of execution from the Bootloader to a separate Application.

### Features

- Flash memory partition
- Separate Bootloader and Application projects
- Bootloader located at `0x08000000`
- Application located at `0x08004000`
- Separate linker configurations
- Application Vector Table relocation
- Initial MSP loading
- Application Reset Handler loading
- SysTick shutdown before jump
- VTOR relocation
- Jump from Bootloader to Application
- Intel HEX merge
- Proteus simulation
- PC13 LED blinking validation

### V1 Memory Layout

```text
0x08000000
+-----------------------------+
| Bootloader                  |
| 16 KB                       |
|                             |
| 0x08000000 - 0x08003FFF     |
+-----------------------------+

0x08004000
+-----------------------------+
| Application                 |
| 48 KB                       |
|                             |
| Vector Table                |
| Reset_Handler               |
| main()                      |
|                             |
| 0x08004000 - 0x0800FFFF     |
+-----------------------------+

0x08010000
```

### V1 Boot Flow

```text
Power ON / Reset
       |
       v
Bootloader Vector Table
       |
       v
Bootloader Reset_Handler
       |
       v
Bootloader main()
       |
       v
Read Application MSP
       |
       v
Read Application Reset_Handler
       |
       v
Stop SysTick
       |
       v
Relocate VTOR
       |
       v
Load Application MSP
       |
       v
Jump to Application Reset_Handler
       |
       v
Application main()
       |
       v
PC13 LED blinking
```

---

## ✅ V2 — Firmware Validation — Completed

V2 adds validation of the Application before transferring execution.

The Bootloader verifies that the Application Vector Table contains coherent values before performing the jump.

### Validation Checks

- Application Vector Table is not empty
- Initial MSP belongs to STM32 SRAM
- Reset Handler has the Cortex-M Thumb bit set
- Reset Handler belongs to the Application Flash region

### Memory Validation

```text
Application Flash:
0x08004000 - 0x0800FFFF

SRAM:
0x20000000 - 0x20004FFF

Initial Stack Top:
0x20005000
```

### V2 Validation Flow

```text
Bootloader
    |
    v
Read Application Vector Table
    |
    +--> Initial MSP
    |
    +--> Reset Handler
    |
    v
Application empty?
    |
    v
MSP inside SRAM?
    |
    v
Thumb bit valid?
    |
    v
Reset Handler inside Application Flash?
    |
    +---------- NO ----------> Stay in Bootloader
    |
   YES
    |
    v
Jump to Application
```

### V2 Validation Tests

| Test | Expected Behavior | Result |
|---|---|---|
| Valid Application | Jump to Application | PASS |
| Missing Application | Stay in Bootloader | PASS |
| Invalid MSP | Stay in Bootloader | PASS |
| Reset Handler outside Application region | Stay in Bootloader | PASS |
| Invalid Thumb bit | Stay in Bootloader | PASS |

A PC13 LED blinking test confirms successful execution of a valid Application.

---

## ✅ V3 — Firmware Integrity — Completed

V3 introduces a dedicated **Firmware Header** and **CRC32 integrity verification**.

The Application is moved from `0x08004000` to `0x08004400`, while the region starting at `0x08004000` is reserved for firmware metadata.

### V3 Memory Layout

```text
0x08000000
+-----------------------------+
| Bootloader                  |
| 16 KB                       |
|                             |
| 0x08000000 - 0x08003FFF     |
+-----------------------------+

0x08004000
+-----------------------------+
| Firmware Header             |
| Reserved region: 1 KB       |
|                             |
| Magic Number                |
| Firmware Size               |
| CRC32                       |
| Firmware Version            |
|                             |
| 0x08004000 - 0x080043FF     |
+-----------------------------+

0x08004400
+-----------------------------+
| Application                 |
| 47 KB                       |
|                             |
| Vector Table                |
| Reset_Handler               |
| Application Code            |
|                             |
| 0x08004400 - 0x0800FFFF     |
+-----------------------------+

0x08010000
```

### Firmware Header

The firmware header starts at:

```text
0x08004000
```

Current structure:

| Offset | Field | Size |
|---|---|---|
| `0x00` | Magic Number | 4 bytes |
| `0x04` | Firmware Size | 4 bytes |
| `0x08` | CRC32 | 4 bytes |
| `0x0C` | Firmware Version | 4 bytes |

Current configuration:

```text
Magic Number : 0xB00710AD
Version      : 0x00010000
```

The current structure uses only 16 bytes, while a 1 KB region is reserved for future firmware metadata.

### Application Start Address

The Application Vector Table is now located at:

```text
0x08004400
```

Therefore:

```text
0x08004400 -> Initial MSP
0x08004404 -> Reset_Handler
```

### Firmware Header Validation

Before checking the Application, the Bootloader verifies:

```text
Magic Number valid?
        |
        v
Firmware Size != 0?
        |
        v
Firmware Size <= Application Flash region?
        |
        v
Continue validation
```

An invalid header prevents the Application from starting.

---

## CRC32 Integrity Verification

The V3 Bootloader performs a CRC32 check before transferring execution.

CRC configuration:

```text
Initial Value : 0xFFFFFFFF
Polynomial    : 0xEDB88320
Final XOR     : 0xFFFFFFFF
```

The implementation is compatible with Python:

```python
zlib.crc32()
```

### Firmware Image Generation

The Python script:

```text
generate_v3_image.py
```

automatically:

1. Loads the Bootloader HEX
2. Loads the Application HEX
3. Verifies the Application start address
4. Determines the real firmware size
5. Builds the exact Application byte sequence
6. Calculates CRC32
7. Creates the Firmware Header
8. Inserts the Header at `0x08004000`
9. Inserts the Application at `0x08004400`
10. Generates the final combined firmware image

Final output:

```text
Bootloader_Application_V3.hex
```

### Example Generated Header

Example from the current build:

```text
Magic Number    : 0xB00710AD
Firmware Size   : 4608 bytes
CRC32           : 0xB226C8B2
Version         : 0x00010000

Header Address  : 0x08004000
Application     : 0x08004400
Application End : 0x080055FF
```

The exact CRC32 value may change whenever the Application binary changes.

### CRC Validation Flow

```text
Firmware Header
      |
      +--> Stored CRC32
      |
      +--> Firmware Size
      |
      v
Application @ 0x08004400
      |
      v
Bootloader calculates CRC32
      |
      v
Calculated CRC == Stored CRC?
        /               \
      NO                 YES
      |                   |
      v                   v
Stay in Bootloader   Continue Boot
                          |
                          v
                  Jump to Application
```

### Complete V3 Boot Flow

```text
Power ON / Reset
       |
       v
Bootloader
       |
       v
Validate Firmware Header
       |
       +--> Magic Number
       +--> Firmware Size
       |
       v
Validate Application Vector Table
       |
       +--> MSP
       +--> Reset Handler
       +--> Thumb bit
       +--> Flash address
       |
       v
Calculate Application CRC32
       |
       v
Compare with Header CRC32
       |
       +---------- INVALID ----------> Stay in Bootloader
       |
      VALID
       |
       v
Jump_To_Application()
       |
       v
Application Reset_Handler
       |
       v
Application main()
       |
       v
PC13 LED blinking
```

### V3 Validation Tests

| Test | Expected Behavior | Result |
|---|---|---|
| Valid Header and Application | Jump to Application | PASS |
| Invalid Magic Number | Stay in Bootloader | PASS |
| Invalid Firmware Size | Stay in Bootloader | PASS |
| Valid CRC32 | Jump to Application | PASS |
| Corrupted Application byte | CRC mismatch / Stay in Bootloader | PASS |

A firmware corruption test was performed by modifying a single Application byte without updating the stored CRC32.

The Bootloader detected the CRC mismatch and refused to execute the Application.

---

# Current Memory Map

```text
STM32F103C8T6 Flash — 64 KB

0x08000000
+-----------------------------+
| Bootloader                  |
| 16 KB                       |
+-----------------------------+
0x08004000
| Firmware Header             |
| Reserved: 1 KB              |
+-----------------------------+
0x08004400
| Application                 |
| 47 KB                       |
+-----------------------------+
0x08010000
```

SRAM:

```text
0x20000000
+-----------------------------+
| SRAM — 20 KB                |
+-----------------------------+
0x20005000
```

---

# Project Structure

```text
01-stm32-custom-bootloader/
|
|-- application/
|   |-- Core/
|   |-- Drivers/
|   |-- Startup/
|   |-- Application.ioc
|   `-- STM32F103C8TX_FLASH.ld
|
|-- bootloader/
|   |-- Core/
|   |-- Drivers/
|   |-- Startup/
|   |-- Bootloader.ioc
|   `-- STM32F103C8TX_FLASH.ld
|
|-- proteus/
|   `-- STM32_Bootloader_Test.pdsprj
|
|-- generate_v3_image.py
|-- Bootloader_Application_V3.hex
|-- .gitignore
`-- README.md
```

---

# Tools and Technologies

- STM32F103C8T6
- ARM Cortex-M3
- Embedded C
- STM32CubeIDE
- STM32CubeMX
- STM32 HAL
- GNU ARM Toolchain
- Linker Scripts
- Intel HEX
- Python
- IntelHex
- Python `zlib`
- Proteus 9 Professional
- Git
- GitHub

---

# Concepts Practiced

- STM32 Boot Process
- Bootloader Architecture
- Flash Memory Mapping
- SRAM Memory Mapping
- Linker Scripts
- Cortex-M Vector Table
- Main Stack Pointer (MSP)
- Program Counter
- Reset Handler
- Startup Code
- VTOR Relocation
- Function Pointers
- Thumb State
- Firmware Metadata
- Magic Numbers
- Firmware Size Validation
- CRC32
- Firmware Integrity Verification
- Intel HEX Manipulation
- Firmware Image Generation
- Bootloader/Application Separation

---

# Planned Development

## ✅ V1 — Bootloader to Application Jump

Completed.

## ✅ V2 — Firmware Validation

Completed.

## ✅ V3 — Firmware Integrity

Completed.

## 🔜 V4 — UART Firmware Update

Planned features:

- UART communication protocol
- Firmware reception
- Bootloader command handling
- Flash erase
- Flash programming
- Firmware Header reception
- CRC verification
- New firmware activation
- Recovery behavior in case of update failure

---

## 🔜 V5 — CAN Firmware Update

Planned features:

- CAN communication
- Firmware transfer over CAN
- Bootloader command protocol
- Flash programming
- CRC verification
- Firmware activation
- Multi-node / ECU-oriented architecture

---

## 🔜 V6 — Automotive Diagnostics

Future extension toward an automotive diagnostic bootloader.

Planned concepts:

- UDS over CAN
- Diagnostic Session Control
- ECU Reset
- Security Access
- Request Download
- Transfer Data
- Request Transfer Exit
- Firmware validation
- Automotive ECU reprogramming workflow

Possible UDS services:

```text
0x10 - Diagnostic Session Control
0x11 - ECU Reset
0x27 - Security Access
0x34 - Request Download
0x36 - Transfer Data
0x37 - Request Transfer Exit
```

---

# Version History

| Version | Description | Status |
|---|---|---|
| V1 | Bootloader to Application Jump | Completed |
| V2 | Application Firmware Validation | Completed |
| V3 | Firmware Header + CRC32 Integrity Verification | Completed |
| V4 | UART Firmware Update | Planned |
| V5 | CAN Firmware Update | Planned |
| V6 | Automotive Diagnostics / UDS | Planned |

---

# Validation Method

The project is validated using a simulated STM32F103C8 environment in Proteus.

PC13 is configured as GPIO output:

```c
while (1)
{
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    HAL_Delay(500);
}
```

Successful PC13 LED blinking confirms that the Bootloader has accepted the firmware and successfully transferred execution to the Application.

If firmware validation fails, execution remains inside the Bootloader and the Application LED does not start blinking.

---

# Current Bootloader Validation Sequence

```text
RESET
  |
  v
Firmware Header valid?
  |
  v
Magic Number valid?
  |
  v
Firmware Size valid?
  |
  v
Application Vector Table valid?
  |
  +--> MSP valid?
  +--> Reset Handler valid?
  +--> Thumb bit valid?
  +--> Application address valid?
  |
  v
Calculate CRC32
  |
  v
Stored CRC == Calculated CRC?
       /        \
     NO          YES
     |            |
     v            v
 Stay in      Jump to
 Bootloader   Application
                  |
                  v
             PC13 Blink
```

---

# Roadmap

The long-term objective is to evolve this project from a basic STM32 bootloader into an automotive-oriented firmware update architecture:

```text
Basic Jump
    |
    v
Firmware Validation
    |
    v
Firmware Integrity
    |
    v
UART Update
    |
    v
CAN Update
    |
    v
UDS Diagnostic Bootloader
```

---

## Author

Embedded Systems / Automotive Engineering Portfolio Project

GitHub repository:

`embedded-automotive-portfolio`