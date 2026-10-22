# STM32F103 Bare-Metal Manual Build — Layered LED Control

Educational bare-metal example developed to understand the complete embedded build chain on an **STM32F103C8T6 / ARM Cortex-M3**, without using STM32 HAL for GPIO control.

This example manually implements a small layered software architecture inspired by the separation used in AUTOSAR-based systems.

> This is an educational demonstrator.  
> It is not a complete AUTOSAR Classic stack and does not implement production RTE, BSW or MCAL modules.

---

## Objective

The purpose of this example is to understand how embedded C source files become an executable firmware image.

The complete flow was built manually:

```text
C / Assembly sources
        |
        v
arm-none-eabi-gcc
        |
        v
Object files (.o)
        |
        v
GNU Linker + Linker Script
        |
        v
firmware.elf
        |
        v
STM32CubeProgrammer
        |
        v
STM32F103C8T6
```

The project also demonstrates separation between:

```text
Application logic
      |
      v
RTE-like interface
      |
      v
Hardware abstraction
      |
      v
STM32 registers
```

---

## Software Architecture

```text
main()
  |
  v
LedControl_Step()
  |
  v
Rte_Write_LedCommand()
  |
  v
Led_Hw_Write()
  |
  v
STM32F103 GPIO registers
  |
  v
PC13 LED
```

### Application Layer

`LedControl.c`

Contains only the LED-control application behavior.

The application does not directly access STM32 registers.

---

### RTE-like Adapter

`Rte_LedControl.c`

Provides the interface:

```c
Rte_Write_LedCommand(...)
```

and forwards the application command to the hardware abstraction.

This lightweight adapter is used only for educational purposes and is not a production AUTOSAR RTE.

---

### Hardware Abstraction

`Led_Hw.h`

Defines a common hardware interface:

```c
void Led_Hw_Init(void);
void Led_Hw_Write(bool state);
```

The STM32 implementation is located in:

```text
platform/Led_Hw_STM32.c
```

---

## Direct STM32 Register Access

GPIO PC13 is controlled without STM32 HAL.

The GPIOC clock is enabled through:

```c
RCC->APB2ENR
```

PC13 is configured through:

```c
GPIOC->CRH
```

The output state is changed using:

```c
GPIOC->BSRR
GPIOC->BRR
```

On the STM32F103C8T6 Blue Pill, the onboard PC13 LED is active-low:

```text
PC13 = 0  -> LED ON
PC13 = 1  -> LED OFF
```

---

## Startup Sequence

The project uses an STM32 startup assembly file.

The simplified execution sequence is:

```text
Reset
  |
  v
Reset_Handler
  |
  +--> Initialize .data
  |
  +--> Clear .bss
  |
  +--> SystemInit()
  |
  v
main()
```

For this minimal bare-metal example, the C-library constructor initialization call was removed because the project does not use the standard C runtime initialization features required by it.

---

## Memory Layout

The linker script defines the STM32F103 memory organization.

```text
FLASH
0x08000000
|
+-- .isr_vector
+-- .text
+-- read-only data
+-- initial values for .data


RAM
0x20000000
|
+-- .data
+-- .bss
+-- stack
```

The linker combines the different object files and places their sections at the appropriate Flash and RAM addresses.

---

## Project Structure

```text
manual-baremetal-led/
|
├── app/
│   ├── LedControl.c
│   ├── LedControl.h
│   ├── Rte_LedControl.c
│   └── Rte_LedControl.h
│
├── platform/
│   ├── Led_Hw.h
│   └── Led_Hw_STM32.c
│
├── startup/
│   └── startup_stm32f103c8tx.s
│
├── system/
│   └── system_stm32f1xx.c
│
├── linker/
│   └── STM32F103C8TX_FLASH.ld
│
├── main.c
├── .gitignore
└── README.md
```

CMSIS Core and STM32F1 CMSIS Device headers are required to build the project but are not duplicated in this repository.

---

## Manual Compilation

Each C source file is first compiled independently.

Example:

```bash
arm-none-eabi-gcc -c app/LedControl.c \
-mcpu=cortex-m3 \
-mthumb \
-std=c11 \
-Iapp \
-Wall \
-Wextra \
-Werror \
-ffunction-sections \
-fdata-sections \
-o build/LedControl.o
```

The same process produces object files for:

```text
LedControl.c
Rte_LedControl.c
Led_Hw_STM32.c
main.c
system_stm32f1xx.c
startup_stm32f103c8tx.s
```

---

## Linking

The final ELF firmware is produced by linking all object files with the STM32 linker script:

```text
startup.o
system_stm32f1xx.o
main.o
LedControl.o
Rte_LedControl.o
Led_Hw_STM32.o
        |
        v
STM32F103C8TX_FLASH.ld
        |
        v
firmware.elf
```

Example:

```bash
arm-none-eabi-gcc \
-mcpu=cortex-m3 \
-mthumb \
-nostdlib \
build/startup.o \
build/system_stm32f1xx.o \
build/main.o \
build/LedControl.o \
build/Rte_LedControl.o \
build/Led_Hw_STM32.o \
-T linker/STM32F103C8TX_FLASH.ld \
-Wl,--gc-sections \
-Wl,-Map=build/firmware.map \
-o build/firmware.elf
```

---

## Useful Binary Inspection Commands

Firmware size:

```bash
arm-none-eabi-size build/firmware.elf
```

Symbols:

```bash
arm-none-eabi-nm build/firmware.elf
```

ELF sections:

```bash
arm-none-eabi-objdump -h build/firmware.elf
```

These tools were used to inspect:

```text
Reset_Handler
SystemInit
main
LedControl_Step
Rte_Write_LedCommand
Led_Hw_Write
```

and the `.text`, `.data`, `.bss` and interrupt-vector sections.

---

## Hardware Validation

Target:

```text
Board      : STM32F103C8T6 Blue Pill
CPU        : ARM Cortex-M3
LED        : PC13
Programmer : ST-Link V2
Flashing   : STM32CubeProgrammer
Compiler   : arm-none-eabi-gcc
```

The manually generated `firmware.elf` was programmed on the physical STM32F103C8T6.

A distinct LED timing pattern was used to differentiate the new manually built firmware from the previously flashed STM32CubeIDE version.

**Result: ✅ Manual bare-metal firmware successfully executed on physical hardware.**

---

## Relation to the AUTOSAR Project

This example was created while studying the architecture of the main hardware-independent AUTOSAR SWC project.

The simplified architecture:

```text
Application
    |
    v
RTE-like Adapter
    |
    v
Hardware Abstraction
    |
    v
STM32 Registers
```

helps illustrate the principle of separating application behavior from hardware-dependent implementation.

A production AUTOSAR Classic ECU would typically use a more complete chain such as:

```text
Application SWC
      |
      v
     RTE
      |
      v
Basic Software
      |
      v
IoHwAb
      |
      v
DIO / PORT
      |
      v
MCAL
      |
      v
Microcontroller
```

---

## Concepts Practiced

- Embedded C
- ARM Cortex-M3
- STM32F103C8T6
- CMSIS
- Direct register access
- Startup code
- Reset Handler
- Interrupt vector table
- `.text`
- `.data`
- `.bss`
- Object files
- Static linking
- Linker scripts
- ELF inspection
- Cross-compilation
- Hardware abstraction
- Layered embedded software architecture
- Physical firmware validation

---

## Status

**✅ Completed — Manual compilation, linking, ELF generation and physical STM32F103 execution validated.**