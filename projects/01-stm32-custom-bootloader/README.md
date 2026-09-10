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

### V1 — Bootloader to Application Jump

- Flash memory partition
- Separate bootloader and application
- Vector table relocation
- MSP configuration
- Jump to application

### V2 — Firmware Validation

- Application address validation
- Stack pointer validation
- Reset Handler validation

### V3 — Firmware Integrity

- Firmware metadata
- Magic number
- Firmware size
- CRC verification

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