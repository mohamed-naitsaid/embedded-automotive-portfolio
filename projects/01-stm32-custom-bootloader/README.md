# STM32F103 Custom Bootloader

Custom bootloader developed for the **STM32F103C8T6 (ARM Cortex-M3)**.

The goal of this project is to understand and implement the embedded boot process step by step, starting from a simple Bootloader-to-Application jump and progressively adding firmware validation, integrity checking and firmware update mechanisms.

The project is developed incrementally, with each version adding a new bootloader capability.

---

## Project Objectives

- Understand the STM32 boot sequence
- Understand Flash and SRAM memory organization
- Separate Bootloader and Application memory regions
- Work with the Cortex-M Vector Table
- Understand MSP and Reset Handler behavior
- Relocate the Application Vector Table
- Validate an Application before execution
- Add firmware metadata
- Verify firmware integrity using CRC32
- Receive and program firmware through UART
- Extend the architecture later toward CAN and UDS

---

# Development Status

## ✅ V1 — Bootloader to Application Jump

The first version implements the basic transfer of execution from the Bootloader to a separate Application.

### Features

- Bootloader located at `0x08000000`
- Application located at `0x08004000`
- Separate Bootloader and Application linker configurations
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

## ✅ V2 — Firmware Validation

V2 adds validation of the Application before transferring execution.

The Bootloader checks the Application Vector Table and verifies that its main startup values are coherent.

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

Successful Application execution is confirmed by the PC13 LED blinking.

---

## ✅ V3 — Firmware Header and CRC32 Integrity

V3 introduces a dedicated **Firmware Header** and a **CRC32 integrity check**.

The Application is moved from `0x08004000` to `0x08004400`.

The region starting at `0x08004000` is reserved for firmware metadata.

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
| Application code            |
|                             |
| 0x08004400 - 0x0800FFFF     |
+-----------------------------+

0x08010000
```

### Firmware Header

The header starts at:

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

The header currently uses 16 bytes.

A 1 KB region is reserved so more metadata can be added later if needed.

### Application Vector Table

The Application now starts at:

```text
0x08004400
```

Therefore:

```text
0x08004400 -> Initial MSP
0x08004404 -> Reset_Handler
```

### Header Validation

Before executing the Application, the Bootloader checks:

```text
Magic Number valid?
        |
        v
Firmware Size != 0?
        |
        v
Firmware Size inside Application region?
        |
        v
Continue validation
```

An invalid Firmware Header prevents the Application from starting.

---

## CRC32 Integrity Verification

The Bootloader calculates CRC32 over the Application stored in Flash.

Configuration:

```text
Initial Value : 0xFFFFFFFF
Polynomial    : 0xEDB88320
Final XOR     : 0xFFFFFFFF
```

The implementation is compatible with:

```python
zlib.crc32()
```

### V3 Firmware Image Generation

The script:

```text
generate_v3_image.py
```

builds the V3 combined firmware image by:

1. Loading the Bootloader HEX
2. Loading the Application HEX
3. Checking the Application start address
4. Determining the real Application size
5. Extracting the Application bytes
6. Calculating CRC32
7. Building the Firmware Header
8. Writing the Header at `0x08004000`
9. Writing the Application at `0x08004400`
10. Generating the final Intel HEX image

Output:

```text
Bootloader_Application_V3.hex
```

Example from the current Application build:

```text
Magic Number    : 0xB00710AD
Firmware Size   : 4608 bytes
CRC32           : 0xB226C8B2
Version         : 0x00010000
Header Address  : 0x08004000
Application     : 0x08004400
Application End : 0x080055FF
```

The CRC32 changes when the Application binary changes.

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
Calculate CRC32
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

### V3 Validation Tests

| Test | Expected Behavior | Result |
|---|---|---|
| Valid Header and Application | Jump to Application | PASS |
| Invalid Magic Number | Stay in Bootloader | PASS |
| Invalid Firmware Size | Stay in Bootloader | PASS |
| Valid CRC32 | Jump to Application | PASS |
| Corrupted Application byte | CRC mismatch / Stay in Bootloader | PASS |

A corruption test was performed by modifying one Application byte without updating the CRC stored in the Header.

The Bootloader detected the mismatch and refused to execute the corrupted Application.

---

# ✅ V4 — UART Firmware Update

V4 adds a complete firmware update mechanism over **USART1**.

Instead of loading a combined Bootloader/Application image directly into Flash, the STM32 can now receive the Application from a PC while the Bootloader is running.

The PC side is handled by:

```text
firmware_update.py
```

The update process was validated in Proteus using a serial connection between Python and the simulated STM32.

### UART Configuration

```text
USART       : USART1
Baud rate   : 115200
Data bits   : 8
Parity      : None
Stop bits   : 1
Flow control: None
```

STM32 pins:

```text
PA9  -> USART1_TX
PA10 -> USART1_RX
```

---

## V4 Update Protocol

The Bootloader uses simple one-byte commands.

| Command | Character | Purpose |
|---|---:|---|
| Start Update | `S` | Enter firmware update mode |
| Erase | `R` | Erase Header and Application Flash area |
| Data | `D` | Receive one Application data block |
| Header | `H` | Receive and program the Firmware Header |
| Verify | `V` | Validate Header, Vector Table and CRC32 |
| End Update | `E` | Finish update and reset the STM32 |

Bootloader responses:

| Response | Character | Meaning |
|---|---:|---|
| ACK | `A` | Command completed successfully |
| NACK | `N` | Command failed |
| Ready | `Y` | STM32 ready to receive data |
| Byte Received | `K` | One byte received correctly |
| Buffer Received | `B` | Complete RAM buffer received |

The current implementation uses an 8-byte Application data block.

```text
DATA_BLOCK_SIZE = 8 bytes
```

The small block size and byte-by-byte acknowledgement were useful for validating the complete UART → RAM → Flash path in Proteus.

---

## Application Data Transfer

For each Application block, the sequence is:

```text
PC                                  STM32

D ---------------------------------->
                               Ready

<--------------------------------- Y

Byte 0 ---------------------------->
<--------------------------------- K

Byte 1 ---------------------------->
<--------------------------------- K

...

Byte 7 ---------------------------->
<--------------------------------- K

                               Buffer complete
<--------------------------------- B

                               Program Flash
<--------------------------------- A
```

After the ACK, the next block is sent.

For the current 4608-byte Application:

```text
Firmware size : 4608 bytes
Block size    : 8 bytes
Blocks        : 576
```

---

## Firmware Header Transfer

The Header is sent **after the complete Application**.

This is intentional.

The Application is programmed first:

```text
0x08004400 ...
```

and only when the firmware transfer is complete is the Header written at:

```text
0x08004000
```

This reduces the risk of considering an interrupted update as valid.

Header transfer:

```text
H
|
v
STM32 Ready
|
v
16 Header bytes
|
v
Header stored in RAM
|
v
Program Header in Flash
|
v
ACK
```

Current Header example:

```text
AD 10 07 B0
00 12 00 00
B2 C8 26 B2
00 00 01 00
```

which represents:

```text
Magic   : 0xB00710AD
Size    : 4608
CRC32   : 0xB226C8B2
Version : 0x00010000
```

---

## Firmware Verification

After programming the Header, Python sends:

```text
V
```

The Bootloader verifies:

```text
Firmware Header
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
      +--> MSP
      +--> Reset Handler
      +--> Thumb bit
      +--> Flash address
      |
      v
Calculate CRC32
      |
      v
Stored CRC == Calculated CRC?
      |
      +---- NO ----> NACK
      |
     YES
      |
      v
     ACK
```

The final test returned:

```text
VERIFY response -> A

Header validation      -> OK
Application validation -> OK
CRC32 verification     -> OK
```

---

## End Update and Reset

After successful verification, Python sends:

```text
E
```

The Bootloader performs one final validation before resetting the MCU.

```text
E
|
v
Header valid?
|
v
Application valid?
|
v
CRC32 valid?
|
+---- NO ----> NACK
|
YES
|
v
ACK
|
v
NVIC_SystemReset()
```

After reset, the Bootloader starts again.

If no new update request is received, it validates the installed firmware and jumps to the Application.

```text
System Reset
     |
     v
Bootloader starts
     |
     v
UART update request?
   /     \
 YES      NO
  |        |
  v        v
Update   Validate Header
Mode        |
            v
       Validate Application
            |
            v
         CRC32
            |
            v
    Jump_To_Application()
            |
            v
       PC13 blinking
```

---

## Complete V4 Update Flow

```text
Python firmware_update.py
          |
          v
Open serial port
          |
          v
S - Start Update
          |
          v
R - Erase old firmware
          |
          v
D - Send Application blocks
          |
          v
576 blocks programmed
          |
          v
H - Program Firmware Header
          |
          v
V - Verify firmware
          |
          v
Header OK
Vector Table OK
CRC32 OK
          |
          v
E - End Update
          |
          v
ACK
          |
          v
NVIC_SystemReset()
          |
          v
Bootloader starts again
          |
          v
Validate installed firmware
          |
          v
Jump to Application
          |
          v
PC13 LED blinking
```

---

## V4 Final Validation

The complete update was successfully tested with the following firmware:

```text
Application : 0x08004400 -> 0x080055FF
Header      : 0x08004000 -> 0x0800400F

Firmware size : 4608 bytes
Blocks        : 576
CRC32         : 0xB226C8B2
Version       : 0x00010000
```

Python output:

```text
Firmware verification -> ACK

Header validation      -> OK
Application validation -> OK
CRC32 verification     -> OK

END UPDATE -> ACK
STM32 reset requested

UPDATE SUCCESSFUL
```

After the STM32 reset, the Bootloader validated the firmware and transferred execution to the Application.

The PC13 LED started blinking successfully.

This validates the complete chain:

```text
PC
 |
 v
UART
 |
 v
Bootloader
 |
 v
Flash erase
 |
 v
Application programming
 |
 v
Header programming
 |
 v
CRC verification
 |
 v
MCU reset
 |
 v
Firmware validation
 |
 v
Application execution
```

---

# Current Memory Map

```text
STM32F103C8T6 Flash — 64 KB

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
| 0x08004000 - 0x080043FF     |
+-----------------------------+

0x08004400
+-----------------------------+
| Application                 |
| 47 KB                       |
|                             |
| 0x08004400 - 0x0800FFFF     |
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

# Repository Structure

```text
embedded-automotive-portfolio/
|
|-- firmware_update.py
|
`-- projects/
    `-- 01-stm32-custom-bootloader/
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
- USART / UART
- Intel HEX
- Python
- PySerial
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
- Main Stack Pointer
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
- Flash Erase
- Flash Half-Word Programming
- Flash Read-Back Verification
- UART Communication
- Bootloader Command Protocol
- Firmware Transfer
- Firmware Activation
- MCU Software Reset
- Python Serial Communication
- Intel HEX Manipulation
- Bootloader/Application Separation

---

# Planned Development

## ✅ V1 — Bootloader to Application Jump

Completed.

## ✅ V2 — Firmware Validation

Completed.

## ✅ V3 — Firmware Integrity

Completed.

## ✅ V4 — UART Firmware Update

Completed.

Implemented features:

- USART1 communication
- Update mode
- Bootloader command parser
- Application Flash erase
- UART firmware reception
- RAM buffering
- Flash programming
- Flash read-back verification
- Firmware Header transfer
- CRC32 verification
- Final update validation
- MCU reset
- Automatic Application boot after update
- Python firmware update tool

---

## 🔜 V5 — CAN Firmware Update

Planned features:

- CAN communication
- Firmware transfer over CAN
- CAN-oriented Bootloader command protocol
- Flash programming
- CRC verification
- Firmware activation
- ECU-oriented communication architecture

---

## 🔜 V6 — Automotive Diagnostics / UDS

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
| V4 | UART Firmware Update | Completed |
| V5 | CAN Firmware Update | Planned |
| V6 | Automotive Diagnostics / UDS | Planned |

Git tag for the UART update version:

```text
v4.0-uart-firmware-update
```

---

# Validation Method

The project is tested using a simulated STM32F103C8 environment in Proteus.

The Application toggles PC13:

```c
while (1)
{
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    HAL_Delay(500);
}
```

Successful LED blinking confirms that the Bootloader has accepted the firmware and transferred execution to the Application.

For V4, the complete test also verifies that the Application was first transferred through UART, written into Flash, checked using CRC32, followed by an MCU reset and a new Bootloader-to-Application jump.

---

# Current Boot Sequence

```text
RESET
  |
  v
Bootloader initialization
  |
  v
UART update request?
   / \
 YES  NO
  |    |
  |    v
  |  Firmware Header valid?
  |    |
  |    v
  |  Application Vector Table valid?
  |    |
  |    v
  |  CRC32 valid?
  |    |
  |    +---- NO ----> Stay in Bootloader
  |    |
  |   YES
  |    |
  |    v
  |  Jump to Application
  |    |
  |    v
  |  PC13 Blink
  |
  v
UART Update Mode
  |
  +--> Erase
  |
  +--> Program Application
  |
  +--> Program Header
  |
  +--> Verify CRC32
  |
  +--> End Update
  |
  v
System Reset
```

---

# Roadmap

The long-term goal is to evolve the project toward an automotive-oriented firmware update architecture:

```text
Bootloader Jump
      |
      v
Firmware Validation
      |
      v
Firmware Integrity
      |
      v
UART Firmware Update
      |
      v
CAN Firmware Update
      |
      v
UDS Diagnostic Bootloader
```

---

## Author

Embedded Systems / Automotive Engineering Portfolio Project

GitHub repository:

`embedded-automotive-portfolio`