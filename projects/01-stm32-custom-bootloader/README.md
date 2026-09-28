# STM32F103 Custom Bootloader

Custom bootloader developed for the **STM32F103C8T6 (ARM Cortex-M3)**.

The project is built incrementally, starting from a basic Bootloader-to-Application jump and progressively adding firmware validation, metadata, CRC32 integrity checking, UART firmware update, CAN-oriented firmware update, and later UDS diagnostics.

---

# Project Objectives

- Understand the STM32 boot sequence
- Understand Flash and SRAM organization
- Separate Bootloader and Application memory regions
- Work with the Cortex-M Vector Table
- Understand MSP and Reset Handler behavior
- Relocate VTOR before jumping to the Application
- Validate an Application before execution
- Add firmware metadata
- Verify firmware integrity using CRC32
- Receive and program firmware through UART
- Implement a CAN-oriented firmware update protocol
- Handle sequence numbers, ACK/NACK and retransmissions
- Prepare the architecture for UDS-based automotive reprogramming

---

# Development Status

| Version | Description | Status |
|---|---|---|
| V1 | Bootloader to Application Jump | ✅ Completed |
| V2 | Application Firmware Validation | ✅ Completed |
| V3 | Firmware Header + CRC32 Integrity | ✅ Completed |
| V4 | UART Firmware Update | ✅ Completed |
| V5 | CAN Firmware Update | ✅ Completed |
| V6 | Automotive Diagnostics / UDS | 🔜 Planned |

---

# ✅ V1 — Bootloader to Application Jump

V1 implements the transfer of execution from the Bootloader to a separate Application.

## Features

- Bootloader at `0x08000000`
- Application at `0x08004000`
- Separate linker configurations
- Application Vector Table relocation
- Initial MSP loading
- Reset Handler loading
- SysTick shutdown before jump
- VTOR relocation
- Jump to the Application Reset Handler
- Proteus validation using PC13 blinking

## V1 Memory Layout

```text
0x08000000
+-----------------------------+
| Bootloader                  |
| 16 KB                       |
| 0x08000000 - 0x08003FFF     |
+-----------------------------+

0x08004000
+-----------------------------+
| Application                 |
| 48 KB                       |
| 0x08004000 - 0x0800FFFF     |
+-----------------------------+

0x08010000
```

## V1 Boot Flow

```text
Reset
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
Load MSP
  |
  v
Jump to Application
  |
  v
PC13 blinking
```

---

# ✅ V2 — Firmware Validation

V2 validates the Application before transferring execution.

## Validation Checks

- Application Vector Table is not empty
- Initial MSP belongs to SRAM
- Reset Handler has the Cortex-M Thumb bit set
- Reset Handler belongs to the Application Flash region

```text
Application Flash : 0x08004000 - 0x0800FFFF
SRAM              : 0x20000000 - 0x20004FFF
Initial Stack Top : 0x20005000
```

## V2 Validation Tests

| Test | Expected Behavior | Result |
|---|---|---|
| Valid Application | Jump to Application | PASS |
| Missing Application | Stay in Bootloader | PASS |
| Invalid MSP | Stay in Bootloader | PASS |
| Reset Handler outside Application region | Stay in Bootloader | PASS |
| Invalid Thumb bit | Stay in Bootloader | PASS |

---

# ✅ V3 — Firmware Header and CRC32 Integrity

V3 introduces a Firmware Header and CRC32 integrity verification.

The Application moves to:

```text
0x08004400
```

A 1 KB metadata region is reserved at:

```text
0x08004000 - 0x080043FF
```

## V3 Memory Layout

```text
0x08000000
+-----------------------------+
| Bootloader                  |
| 16 KB                       |
| 0x08000000 - 0x08003FFF     |
+-----------------------------+

0x08004000
+-----------------------------+
| Firmware Header             |
| Reserved region: 1 KB       |
| 0x08004000 - 0x080043FF     |
+-----------------------------+

0x08004400
+-----------------------------+
| Application                 |
| 47 KB                       |
| 0x08004400 - 0x0800FFFF     |
+-----------------------------+

0x08010000
```

## Firmware Header

| Offset | Field | Size |
|---|---|---|
| `0x00` | Magic Number | 4 bytes |
| `0x04` | Firmware Size | 4 bytes |
| `0x08` | CRC32 | 4 bytes |
| `0x0C` | Firmware Version | 4 bytes |

```text
Magic Number : 0xB00710AD
Version      : 0x00010000
Header Size  : 16 bytes
```

Application Vector Table:

```text
0x08004400 -> Initial MSP
0x08004404 -> Reset_Handler
```

## CRC32

```text
Initial Value : 0xFFFFFFFF
Polynomial    : 0xEDB88320
Final XOR     : 0xFFFFFFFF
```

Compatible with:

```python
zlib.crc32()
```

Current Application example:

```text
Firmware Size   : 4608 bytes
CRC32           : 0xB226C8B2
Version         : 0x00010000
Header Address  : 0x08004000
Application     : 0x08004400
Application End : 0x080055FF
```

## V3 Validation Tests

| Test | Expected Behavior | Result |
|---|---|---|
| Valid Header and Application | Jump to Application | PASS |
| Invalid Magic Number | Stay in Bootloader | PASS |
| Invalid Firmware Size | Stay in Bootloader | PASS |
| Valid CRC32 | Jump to Application | PASS |
| Corrupted Application byte | CRC mismatch / Stay in Bootloader | PASS |

---

# ✅ V4 — UART Firmware Update

V4 adds a complete firmware update mechanism over **USART1**.

PC tool:

```text
firmware_update.py
```

## UART Configuration

```text
USART        : USART1
Baud rate    : 115200
Data bits    : 8
Parity       : None
Stop bits    : 1
Flow control : None

PA9  -> USART1_TX
PA10 -> USART1_RX
```

## V4 Commands

| Command | Character | Purpose |
|---|---:|---|
| Ping | `P` | Test communication |
| Start Update | `S` | Enter update mode |
| Erase | `R` | Erase Header and Application area |
| Data | `D` | Receive one Application data block |
| Header | `H` | Receive and program Firmware Header |
| Verify | `V` | Validate Header, Application and CRC32 |
| End Update | `E` | Finish update and reset |

Responses:

| Response | Character | Meaning |
|---|---:|---|
| ACK | `A` | Success |
| NACK | `N` | Failure |
| Ready | `Y` | Ready to receive |
| Byte Received | `K` | Byte received |
| Buffer Received | `B` | Complete buffer received |

Current UART block size:

```text
DATA_BLOCK_SIZE = 8 bytes
```

For the current firmware:

```text
Firmware size : 4608 bytes
Block size    : 8 bytes
Blocks        : 576
```

## V4 Update Flow

```text
Python firmware_update.py
          |
          v
S - Start Update
          |
          v
R - Erase
          |
          v
D - 576 data blocks
          |
          v
H - Program Header
          |
          v
V - Verify
          |
          +--> Header OK
          +--> Vector Table OK
          +--> CRC32 OK
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
Jump to Application
```

## V4 Final Validation

```text
Application : 0x08004400 -> 0x080055FF
Header      : 0x08004000 -> 0x0800400F
Firmware    : 4608 bytes
Blocks      : 576
CRC32       : 0xB226C8B2
Version     : 0x00010000
```

Final result:

```text
Firmware verification -> ACK
Header validation      -> OK
Application validation -> OK
CRC32 verification     -> OK
END UPDATE              -> ACK
STM32 reset requested
UPDATE SUCCESSFUL
```

---

# ✅ V5 — CAN Firmware Update

V5 adds a firmware update mechanism designed around **CAN communication** while preserving the V4 UART path.

The V5 protocol includes:

- command frames
- firmware DATA frames
- Firmware Header frames
- sequence numbers
- ACK/NACK responses
- explicit error codes
- duplicate-frame handling
- retry support
- CRC32 verification
- reset and firmware activation

## CAN Configuration

```text
Peripheral : bxCAN / CAN1
CAN RX     : PA11
CAN TX     : PA12
Bit rate   : 500 kbit/s

PCLK1      : 8 MHz
Prescaler  : 1
BS1        : 13 TQ
BS2        : 2 TQ
SJW        : 1 TQ
```

Bitrate:

```text
Total TQ = 1 + 13 + 2 = 16

8 MHz / (1 × 16) = 500 kbit/s
```

## CAN IDs

| CAN ID | Direction | Purpose |
|---|---|---|
| `0x600` | Tester → Bootloader | Commands |
| `0x601` | Tester → Bootloader | Application DATA |
| `0x602` | Tester → Bootloader | Firmware Header |
| `0x650` | Bootloader → Tester | Responses |

## CAN Response Format

```text
Byte 0 : Response     ('A' or 'N')
Byte 1 : Context
Byte 2 : Sequence LSB
Byte 3 : Sequence MSB
Byte 4 : Error code
```

Error codes:

```text
0x00 - NONE
0x01 - STATE
0x02 - LENGTH
0x03 - SEQUENCE
0x04 - FLASH
0x05 - HEADER
0x06 - CRC
0x07 - RANGE
```

## START UPDATE

CAN ID:

```text
0x600
```

Payload:

```text
Byte 0    : 'S'
Byte 1..4 : Firmware size, little-endian
```

For 4608 bytes:

```text
4608 = 0x00001200
Size bytes = 00 12 00 00
```

## Application DATA Frames

CAN ID:

```text
0x601
```

Frame format:

```text
Byte 0 : Sequence LSB
Byte 1 : Sequence MSB
Byte 2 : Firmware byte 0
Byte 3 : Firmware byte 1
Byte 4 : Firmware byte 2
Byte 5 : Firmware byte 3
Byte 6 : Firmware byte 4
Byte 7 : Firmware byte 5
```

Therefore:

```text
CAN_DATA_PAYLOAD_SIZE = 6 bytes
```

For the current Application:

```text
4608 / 6 = 768 CAN DATA frames
```

## Sequence and Retry Handling

Expected sequence:

```text
0, 1, 2, 3, ...
```

If the expected sequence arrives:

```text
Program Flash
   |
   v
Increment sequence
   |
   v
ACK
```

If the previous sequence arrives again:

```text
Duplicate frame
   |
   v
Do not program again
   |
   v
ACK again
```

This allows recovery when firmware data was programmed successfully but its ACK was lost.

## Firmware Header over CAN

CAN ID:

```text
0x602
```

Header size:

```text
16 bytes
```

With 6 useful bytes per CAN frame:

```text
Header frame 0 -> 6 bytes
Header frame 1 -> 6 bytes
Header frame 2 -> 4 bytes
```

The Header is validated in RAM before being written to Flash.

Writing the Header last reduces the risk of treating an interrupted update as valid.

## Verification

The `V` command validates:

```text
Firmware Header
      |
      v
Application Vector Table
      |
      v
CRC32
      |
   +--+--+
   |     |
  NO    YES
   |     |
 NACK   ACK
```

## END UPDATE

The `E` command performs a final validation.

If everything is valid:

```text
ACK
 |
 v
NVIC_SystemReset()
```

---

# Proteus bxCAN Limitation

V5 was developed with **Proteus 9.0 SP2**.

A minimal bxCAN test showed:

```text
UART initialized       -> OK
CAN1 clock enable      -> OK
RCC->APB1ENR read      -> OK
CAN1->MCR access       -> simulation stops
```

Therefore the STM32F103 bxCAN peripheral itself could not be executed reliably with this Proteus model.

This occurs before filters, real CAN frames, CANH/CANL or the firmware update state machine are involved.

---

# Proteus CAN Simulation Shim

To validate the complete V5 protocol logic, a Proteus-only UART/COMPIM transport shim was added.

```text
Python updater
      |
      v
COMPIM / UART
      |
      v
Simulated CAN frame
      |
      v
Same V5 CAN handlers
      |
      v
Flash / Header / CRC32 / Reset
```

Simulation frame format:

```text
C5 | ID_L | ID_H | DLC | DATA...
```

Example PING request:

```text
C5 00 06 01 50
```

Meaning:

```text
CAN ID : 0x600
DLC    : 1
DATA   : 0x50 = 'P'
```

Expected response:

```text
C5 50 06 05 41 50 00 00 00
```

Meaning:

```text
CAN ID   : 0x650
DLC      : 5
Response : 'A'
Context  : 'P'
Sequence : 0
Error    : 0
```

The real bxCAN path is selected with:

```c
#define PROTEUS_SIMULATION 0U
```

Proteus simulation uses:

```c
#define PROTEUS_SIMULATION 1U
```

---

# Proteus COMPIM Pacing

Proteus COMPIM lost bytes when a complete simulated CAN frame was sent as one fast serial burst.

The final stable configuration used:

```text
UART byte delay : 100 ms
Serial timeout  : 30 s
Retries         : 5
```

This is only a **Proteus simulation workaround**. It is not real CAN timing.

---

# V5 Retry Validation

During the final complete update, one timeout occurred:

```text
DATA sequence 5: timeout, retry 1/5
```

The same DATA sequence was retransmitted.

The Bootloader recognized it as a duplicate and returned ACK without programming the same data twice.

The update then continued normally to completion.

---

# V5 Final Validation

Complete validated flow:

```text
CAN PING               -> ACK
START UPDATE           -> ACK
ERASE                  -> ACK

768 DATA frames        -> ACK

Header frame 0         -> ACK
Header frame 1         -> ACK
Header frame 2         -> ACK

Header validation      -> OK
Application validation -> OK
CRC32 verification     -> OK

END UPDATE             -> ACK
STM32 reset requested
```

Final output:

```text
V5 CAN UPDATE SUCCESSFUL

Application : 0x08004400 -> 0x080055FF
Header      : 0x08004000 -> 0x0800400F
Firmware    : 4608 bytes
CRC32       : 0xB226C8B2
Version     : 0x00010000
```

Validated V5 elements:

```text
CAN-oriented command protocol   ✅
CAN IDs                         ✅
DATA sequence management        ✅
Flash erase                     ✅
Application programming         ✅
Duplicate-frame handling        ✅
Retry mechanism                 ✅
Header transfer                 ✅
Header validation               ✅
Application validation          ✅
CRC32 verification              ✅
END command                     ✅
MCU reset                       ✅
```

> The V5 protocol logic and firmware-update state machine were validated completely in Proteus through the UART/COMPIM CAN simulation shim. The physical STM32F103 bxCAN peripheral itself was not runtime-validated in Proteus because the Proteus 9.0 SP2 STM32F103 model stops on direct bxCAN register access.

---

# Current Memory Map

```text
STM32F103C8T6 Flash — 64 KB

0x08000000
+-----------------------------+
| Bootloader                  |
| 16 KB                       |
| 0x08000000 - 0x08003FFF     |
+-----------------------------+

0x08004000
+-----------------------------+
| Firmware Header             |
| Reserved region: 1 KB       |
| Header uses first 16 bytes  |
| 0x08004000 - 0x080043FF     |
+-----------------------------+

0x08004400
+-----------------------------+
| Application                 |
| 47 KB                       |
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

# Current Boot Sequence

```text
RESET
  |
  v
Bootloader initialization
  |
  v
Boot window
  |
  +--> UART update request?
  |
  +--> CAN update request?
  |
  +--> No update request
           |
           v
      Header valid?
           |
           v
      Application valid?
           |
           v
      CRC32 valid?
        /            NO        YES
      |          |
      v          v
Stay in BL   Jump_To_Application()
                  |
                  v
             Application
```

Update modes:

```text
UART path -> V4 protocol
CAN path  -> V5 protocol
```

---

# Repository Structure

```text
embedded-automotive-portfolio/
|
|-- firmware_update.py
|-- can_firmware_update_proteus_robust.py
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
- bxCAN
- CAN protocol concepts
- Intel HEX
- Python
- PySerial
- IntelHex
- Python `zlib`
- Proteus 9 Professional
- COMPIM
- Git
- GitHub

---

# Concepts Practiced

- STM32 Boot Process
- Bootloader Architecture
- Flash and SRAM Memory Mapping
- Linker Scripts
- Cortex-M Vector Table
- Main Stack Pointer
- Reset Handler
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
- CAN-oriented Bootloader Communication
- CAN Standard Identifiers
- CAN Payload Design
- Sequence Numbers
- ACK / NACK
- Error Codes
- Retransmission
- Duplicate Frame Handling
- Firmware Transfer
- Firmware Activation
- MCU Software Reset
- Python Serial Communication
- Intel HEX Manipulation
- Bootloader/Application Separation
- Simulation vs Hardware Abstraction

---

# Version History

| Version | Description | Status |
|---|---|---|
| V1 | Bootloader to Application Jump | Completed |
| V2 | Application Firmware Validation | Completed |
| V3 | Firmware Header + CRC32 Integrity Verification | Completed |
| V4 | UART Firmware Update | Completed |
| V5 | CAN Firmware Update | Completed |
| V6 | Automotive Diagnostics / UDS | Planned |

Git tags:

```text
v1.0-bootloader-jump
v2.0-firmware-validation
v2.0.1-firmware-validation
v3.0-firmware-integrity
v3.0.1-firmware-integrity
v4.0-uart-firmware-update
v5.0-can-firmware-update
```

---

# 🔜 V6 — Automotive Diagnostics / UDS

The next version will extend the Bootloader toward an automotive diagnostic reprogramming architecture.

Planned services:

```text
0x10 - Diagnostic Session Control
0x11 - ECU Reset
0x27 - Security Access
0x34 - Request Download
0x36 - Transfer Data
0x37 - Request Transfer Exit
```

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

The goal is to reuse the Flash, CRC32 and firmware-validation foundation developed in V1–V5 and expose the update flow through UDS services.

---

# Roadmap

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

# Author

Embedded Systems / Automotive Engineering Portfolio Project

GitHub repository:

`embedded-automotive-portfolio`
