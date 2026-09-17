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

A PC13 LED blinking test is used to confirm successful execution of a valid application.