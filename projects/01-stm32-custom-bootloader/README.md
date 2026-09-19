# STM32 Custom Bootloader

Custom bootloader implementation for an ARM Cortex-M based STM32 microcontroller.

The objective of this project is to understand and implement the complete boot process of an embedded system, including memory organization, vector table relocation, application validation and transfer of execution from the bootloader to the main application.

## Project Objectives

- Understand the STM32 boot sequence
- Understand Flash and RAM memory organization
- Configure a dedicated bootloader memory region
- Configure a separate application memory region
- Understand the Cortex-M vector table
- Read the application stack pointer
- Read the application Reset Handler
- Transfer execution from bootloader to application
- Validate application firmware
- Add CRC integrity verification
- Implement firmware update over UART
- Extend firmware update to CAN
- Explore automotive diagnostic bootloader concepts

## Planned Development

### ✅ V1 — Bootloader to Application Jump — Completed

### ✅ V2 — Firmware Validation — Completed

The bootloader validates the application before transferring execution.

Validation checks:

- Application vector table is not empty
- Initial MSP belongs to STM32 SRAM
- Reset Handler has the Cortex-M Thumb bit set
- Reset Handler belongs to the Application Flash region

#### Validation Tests

| Test | Expected Behavior | Result |
|---|---|---|
| Valid application | Jump to application | PASS |
| Missing application | Stay in bootloader | PASS |
| Invalid MSP | Stay in bootloader | PASS |
| Reset Handler outside application region | Stay in bootloader | PASS |
| Invalid Thumb bit | Stay in bootloader | PASS |

A PC13 LED blinking test confirms successful execution of a valid application.

### ✅ V3 — Firmware Integrity — Completed

V3 introduces firmware metadata and CRC32 integrity verification before the application is executed.

#### Memory Layout

```text
0x08000000
+-----------------------------+
| Bootloader - 16 KB          |
+-----------------------------+

0x08004000
+-----------------------------+
| Firmware Header - 1 KB      |
|                             |
| Magic Number                |
| Firmware Size               |
| CRC32                       |
| Firmware Version            |
+-----------------------------+

0x08004400
+-----------------------------+
| Application - 47 KB         |
|                             |
| Vector Table                |
| Reset_Handler               |
| Application Code            |
+-----------------------------+

0x08010000

### V4 — UART Firmware Update

- Firmware reception
- Flash erase
- Flash programming
- Application verification

### V5 — CAN Firmware Update

- CAN transport
- Firmware transfer
- Bootloader commands

### V6 — Automotive Diagnostics

Future extension toward UDS-based firmware download mechanisms.