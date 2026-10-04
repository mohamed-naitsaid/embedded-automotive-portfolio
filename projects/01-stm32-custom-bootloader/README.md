# STM32F103 Custom Bootloader

Custom bootloader developed for the **STM32F103C8T6 (ARM Cortex-M3)**.

The project is built incrementally, starting from a basic Bootloader-to-Application jump and progressively adding firmware validation, metadata, CRC32 integrity checking, UART firmware update, CAN-oriented firmware update, and finally a UDS-based automotive reprogramming flow.

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
- Implement UDS diagnostic services for firmware reprogramming
- Build a complete automotive-style ECU update sequence

---

# Development Status

| Version | Description | Status |
|---|---|---|
| V1 | Bootloader to Application Jump | ✅ Completed |
| V2 | Application Firmware Validation | ✅ Completed |
| V3 | Firmware Header + CRC32 Integrity | ✅ Completed |
| V4 | UART Firmware Update | ✅ Completed |
| V5 | CAN Firmware Update | ✅ Completed |
| V6 | UDS Diagnostic Firmware Update | ✅ Completed |

---

# ✅ V1 — Bootloader to Application Jump

V1 implements the transfer of execution from the Bootloader to a separate Application.

## Features

- Bootloader at `0x08000000`
- Application initially at `0x08004000`
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

---

# ✅ V2 — Firmware Validation

V2 validates the Application before transferring execution.

## Validation Checks

- Application Vector Table is not empty
- Initial MSP belongs to SRAM
- Reset Handler has the Cortex-M Thumb bit set
- Reset Handler belongs to the Application Flash region

```text
SRAM              : 0x20000000 - 0x20004FFF
Initial Stack Top : 0x20005000
```

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

## Current Memory Layout

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

Current Application:

```text
Firmware Size   : 4608 bytes
CRC32           : 0xB226C8B2
Version         : 0x00010000
Header Address  : 0x08004000
Application     : 0x08004400
Application End : 0x080055FF
```

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

## V4 Final Validation

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

## CAN IDs

| CAN ID | Direction | Purpose |
|---|---|---|
| `0x600` | Tester → Bootloader | Commands |
| `0x601` | Tester → Bootloader | Application DATA |
| `0x602` | Tester → Bootloader | Firmware Header |
| `0x650` | Bootloader → Tester | Responses |

## CAN DATA

```text
Byte 0 : Sequence LSB
Byte 1 : Sequence MSB
Byte 2..7 : 6 firmware bytes
```

For the current firmware:

```text
4608 / 6 = 768 CAN DATA frames
```

Duplicate-frame handling prevents the same Flash block from being programmed twice when an ACK is lost.

## V5 Final Validation

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

V5 CAN UPDATE SUCCESSFUL
```

---

# Proteus bxCAN Limitation

V5 and V6 were developed with **Proteus 9.0 SP2**.

A minimal bxCAN test showed that the STM32F103 Proteus model stops on direct CAN register access. Therefore the physical bxCAN peripheral could not be runtime-validated reliably in this Proteus version.

The real bxCAN path remains in the firmware and is selected with:

```c
#define PROTEUS_SIMULATION 0U
```

Proteus simulation uses:

```c
#define PROTEUS_SIMULATION 1U
```

---

# Proteus CAN Simulation Shim

To validate the CAN and UDS protocol logic in Proteus, a simulation-only UART/COMPIM transport shim is used.

```text
Python tester
      |
      v
COMPIM / UART
      |
      v
Simulated CAN frame
      |
      v
Same CAN / UDS handlers
      |
      v
Flash / Header / CRC32 / Reset
```

Simulation frame format:

```text
C5 | ID_L | ID_H | DLC | DATA...
```

This shim is only a Proteus workaround and is not part of real CAN timing.

---

# ✅ V6 — UDS Diagnostic Firmware Update

V6 extends the Bootloader with a **UDS-based automotive firmware reprogramming flow**.

The UDS layer is implemented in:

```text
bootloader/Core/Inc/uds.h
bootloader/Core/Src/uds.c
```

## UDS CAN IDs

```text
Tester -> ECU : 0x7E0
ECU    -> Tester : 0x7E8
```

## Implemented UDS Services

| SID | Service | Status |
|---|---|---|
| `0x10` | DiagnosticSessionControl | ✅ Validated |
| `0x11` | ECUReset | ✅ Validated |
| `0x27` | SecurityAccess | ✅ Validated |
| `0x34` | RequestDownload | ✅ Validated |
| `0x36` | TransferData | ✅ Validated |
| `0x37` | RequestTransferExit | ✅ Validated |

---

## V6.1 — DiagnosticSessionControl `0x10`

Supported sessions:

```text
0x01 - Default Session
0x02 - Programming Session
0x03 - Extended Diagnostic Session
```

Programming Session request:

```text
02 10 02
```

Positive response:

```text
06 50 02 00 32 01 F4
```

---

## V6.2 — ECUReset `0x11`

Supported reset:

```text
0x01 - Hard Reset
```

Request:

```text
02 11 01
```

Positive response:

```text
02 51 01
```

Then:

```text
NVIC_SystemReset()
```

---

## V6.3 — SecurityAccess `0x27`

Validated example:

```text
Seed      : 0x12345678
Valid Key : 0xB791F3DD
```

Flow:

```text
27 01 -> Request Seed
67 01 -> Seed response
27 02 -> Send Key
67 02 -> Access granted
```

The current Seed/Key algorithm is educational and is not intended as production automotive security.

---

## V6.4 — RequestDownload `0x34`

RequestDownload is accepted only when:

```text
Programming Session active
            +
SecurityAccess unlocked
```

Current compact request format:

```text
07 34 00 22 AA AA SS SS
```

where:

```text
AA AA = offset from 0x08004400
SS SS = firmware size
```

For the current full Application:

```text
Address : 0x08004400
Size    : 4608 bytes = 0x1200
```

Positive response:

```text
03 74 10 07
```

The Bootloader then erases/prepares the firmware Flash area.

---

## V6.5 — TransferData `0x36`

TransferData uses a BlockSequenceCounter.

Format:

```text
Length | 36 | BlockSequenceCounter | Firmware DATA...
```

The full-update Python tool transfers up to **5 firmware bytes per UDS TransferData request**.

Example:

```text
06 36 01 00 50 00 20
```

Positive response:

```text
02 76 01
```

Validated cases:

- correct sequence
- wrong BlockSequenceCounter
- duplicate block
- final block
- extra block after completion
- retry without double Flash programming

---

## V6.6 — RequestTransferExit `0x37`

The base TransferExit request closes the transfer sequence only after all announced bytes are received.

Base request:

```text
01 37
```

Positive response:

```text
01 77
```

An early TransferExit returns a sequence error.

---

## V6.7 — Full UDS Firmware Update and Activation

PC tool:

```text
uds_full_firmware_update_proteus.py
```

Full sequence:

```text
0x10 DiagnosticSessionControl
          |
          v
Programming Session
          |
          v
0x27 SecurityAccess
          |
          v
0x34 RequestDownload
          |
          v
Flash erase
          |
          v
0x36 TransferData
          |
          v
4608 Application bytes programmed
          |
          v
0x37 RequestTransferExit + CRC32
          |
          v
CRC32 calculated from STM32 Flash
          |
          v
Application Vector Table validated
          |
          v
Firmware Header written LAST
          |
          v
Header + Application + CRC32 validated
          |
          v
0x77 Positive Response
          |
          v
0x11 ECUReset
          |
          v
NVIC_SystemReset()
          |
          v
Bootloader starts again
          |
          v
Header + Vector Table + CRC32 valid
          |
          v
Jump_To_Application()
```

### TransferExit with CRC32

The final full-update request uses:

```text
05 37 CRC3 CRC2 CRC1 CRC0
```

For the current Application:

```text
CRC32 = 0xB226C8B2
```

The Firmware Header is written only after:

```text
All firmware bytes received      ✅
Application Vector Table valid   ✅
Calculated CRC32 == expected CRC ✅
```

---

# V6 Full Update Validation

Validated firmware:

```text
Application : 0x08004400 -> 0x080055FF
Firmware    : 4608 bytes
CRC32       : 0xB226C8B2
Version     : 0x00010000
```

Transfer:

```text
TransferData blocks : 922
Transferred bytes   : 4608 / 4608
```

Final validated output:

```text
[4] RequestDownload
    Flash erased -> ACK

[5] TransferData
    Block 1/922   -> ACK
    ...
    Block 922/922 -> ACK

Transferred 4608/4608 bytes

[6] RequestTransferExit + CRC32
    CRC32 verification     -> OK
    Application validation -> OK
    Firmware Header        -> WRITTEN
    Final validation       -> OK

[7] ECU Reset
    Positive response -> OK
    STM32 reset requested

V6.7 FULL UDS FIRMWARE UPDATE SUCCESSFUL
```

---

# CRC32 Build Note

The Bootloader remains inside its reserved **16 KB Flash region**, so the project is built globally with:

```text
-Os
```

During V6 validation in the current STM32CubeIDE / Proteus configuration, the CRC32 path produced an incorrect runtime result when the CRC function used the global size optimization.

The validated configuration keeps the project at `-Os` while compiling only the CRC function without optimization:

```c
__attribute__((noinline, optimize("O0")))
static uint32_t Bootloader_CalculateCRC32(...)
```

This configuration produced the expected CRC32 and allowed the complete UDS firmware update to pass while preserving the 16 KB Bootloader partition.

---

# UDS Negative Responses Used

| NRC | Meaning |
|---|---|
| `0x11` | ServiceNotSupported |
| `0x12` | SubFunctionNotSupported |
| `0x13` | IncorrectMessageLengthOrInvalidFormat |
| `0x22` | ConditionsNotCorrect |
| `0x24` | RequestSequenceError |
| `0x31` | RequestOutOfRange |
| `0x33` | SecurityAccessDenied |
| `0x35` | InvalidKey |
| `0x72` | GeneralProgrammingFailure |
| `0x73` | WrongBlockSequenceCounter |

---

# Current Boot Sequence

```text
RESET
  |
  v
Bootloader initialization
  |
  v
Update / diagnostic window
  |
  +--> UART V4 request?
  |
  +--> CAN V5 request?
  |
  +--> UDS V6 request?
  |
  `--> No update request
           |
           v
      Header valid?
           |
           v
      Application valid?
           |
           v
      CRC32 valid?
        /            NO       YES
      |         |
      v         v
Stay in BL   Jump_To_Application()
                  |
                  v
             Application
```

---

# Repository Structure

```text
embedded-automotive-portfolio/
|
|-- firmware_update.py
|-- uds_full_firmware_update_proteus.py
|
`-- projects/
    `-- 01-stm32-custom-bootloader/
        |
        |-- application/
        |
        |-- bootloader/
        |   |-- Core/
        |   |   |-- Inc/
        |   |   |   `-- uds.h
        |   |   `-- Src/
        |   |       |-- main.c
        |   |       `-- uds.c
        |   |
        |   |-- Bootloader.ioc
        |   `-- STM32F103C8TX_FLASH.ld
        |
        |-- proteus/
        |   `-- STM32_Bootloader_Test.pdsprj
        |
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
- UDS
- ISO-TP single-frame concepts
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
- Cortex-M Vector Table
- Main Stack Pointer
- Reset Handler
- VTOR Relocation
- Firmware Metadata
- CRC32
- Flash Erase / Programming / Read-Back
- UART Firmware Update
- CAN Firmware Update
- Sequence Numbers
- ACK / NACK
- Retransmission
- Duplicate Frame Handling
- UDS DiagnosticSessionControl
- UDS ECUReset
- UDS SecurityAccess
- UDS RequestDownload
- UDS TransferData
- UDS RequestTransferExit
- UDS Negative Response Codes
- Programming Session
- Seed / Key access
- Automotive ECU reprogramming flow
- Firmware activation
- MCU Software Reset
- Python test tooling
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
| V6 | UDS Diagnostic Firmware Update | Completed |

Git tags:

```text
v1.0-bootloader-jump
v2.0-firmware-validation
v2.0.1-firmware-validation
v3.0-firmware-integrity
v3.0.1-firmware-integrity
v4.0-uart-firmware-update
v5.0-can-firmware-update
v6.0-uds-bootloader
```

---

# Roadmap Completed

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
      |
      v
Full UDS Firmware Reprogramming
```

---

# Validation Scope

The protocol logic, Flash programming, CRC32 verification, firmware activation, UDS services and complete update state machine were validated in **Proteus 9.0 SP2** using the UART/COMPIM CAN simulation shim.

Because of the STM32F103 bxCAN peripheral limitation observed in this Proteus model, the physical bxCAN peripheral itself has not yet been runtime-validated on real hardware.

A future hardware validation can use the native STM32 bxCAN peripheral with an external CAN transceiver.

---

# Author

Embedded Systems / Automotive Engineering Portfolio Project

GitHub repository:

`embedded-automotive-portfolio`
