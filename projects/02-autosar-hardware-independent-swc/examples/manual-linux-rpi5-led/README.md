# Raspberry Pi 5 Linux ARM64 — Layered LED Control

Educational Linux ARM64 example developed on a **Raspberry Pi 5** to understand object-file compilation, dynamic linking, GPIO control with `libgpiod`, and hardware-independent application design.

This example reuses the same layered software idea developed for the STM32 bare-metal example.

> This is an educational demonstrator inspired by AUTOSAR separation principles.  
> It is not a complete AUTOSAR Classic stack or production RTE implementation.

---

## Objective

The objective is to demonstrate how application logic can remain independent from the target hardware.

The software chain is:

```text
Application
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
libgpiod
    |
    v
GPIO17
    |
    v
Physical LED
```

The application logic does not directly access Raspberry Pi GPIO APIs.

---

## Target Platform

```text
Board        : Raspberry Pi 5
Architecture : ARM64 / aarch64
OS           : Raspberry Pi OS / Debian
GPIO         : GPIO17
GPIO chip    : gpiochip0
GPIO API     : libgpiod 2.2.1
```

Physical output:
```text
GPIO17 / Physical Pin 11
        |
       330 Ω
        |
       LED
       GND / Physical Pin 6
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
libgpiod
  |
/dev/gpiochip0
offset 17
  |
  v
GPIO17
  |
  v
LED
```

---

## Application Layer

The application is implemented in:

```text
app/LedControl.c
```

It maintains the previous LED state and toggles it on each periodic execution.

Conceptually:

```text
Read previous state
        |
        v
Send current state
        |
        v
Invert state
        |
        v
Store for next execution
```

The application layer does not know whether the final target is an STM32, Raspberry Pi, or another platform.

---

## RTE-like Adapter

The lightweight interface is implemented in:

```text
app/Rte_LedControl.c
```

It provides:

```c
void Rte_Write_LedCommand(bool value);
```

and forwards the command to:

```c
Led_Hw_Write(value);
```

Flow:

```text
LedControl
    |
    v
Rte_Write_LedCommand()
    |
    v
Led_Hw_Write()
```

This adapter is for educational purposes and is not a production AUTOSAR RTE.

---

## Hardware Abstraction

The common interface is defined in:

```text
platform/Led_Hw.h
```

with:

```c
void Led_Hw_Init(void);
void Led_Hw_Write(bool state);
```

The Raspberry Pi-specific implementation is:

```text
platform/Led_Hw_RaspberryPi.c
```

Only this file knows about:

```text
libgpiod
/dev/gpiochip0
GPIO17
```

---

## GPIO Control with libgpiod

The GPIO implementation uses `libgpiod 2.2.1`.

The application requests:

```text
gpiochip0
offset 17
```

which corresponds to:

```text
GPIO17
```

The line is configured as an output and controlled using the libgpiod v2 API.

Simplified flow:

```text
gpiod_chip_open()
        |
        v
configure GPIO line
        |
        v
request GPIO17
        |
        v
gpiod_line_request_set_value()
```

---

## Project Structure

```text
manual-linux-rpi5-led/
|
├── app/
│   ├── LedControl.c
│   ├── LedControl.h
│   ├── Rte_LedControl.c
│   └── Rte_LedControl.h
│
├── platform/
│   ├── Led_Hw.h
│   └── Led_Hw_RaspberryPi.c
│
├── main.c
├── .gitignore
└── README.md
```

Generated object files and executables are excluded from version control.

---

## Manual Compilation

Each source file was compiled independently.

### Application

```bash
gcc -c app/LedControl.c \
-std=c11 \
-Iapp \
-Wall \
-Wextra \
-Werror \
-ffunction-sections \
-fdata-sections \
-o build/LedControl.o
```

### RTE-like Adapter

```bash
gcc -c app/Rte_LedControl.c \
-std=c11 \
-Iapp \
-Iplatform \
-Wall \
-Wextra \
-Werror \
-ffunction-sections \
-fdata-sections \
-o build/Rte_LedControl.o
```

### Raspberry Pi Hardware Layer

```bash
gcc -c platform/Led_Hw_RaspberryPi.c \
-std=c11 \
-Iplatform \
$(pkg-config --cflags libgpiod) \
-Wall \
-Wextra \
-Werror \
-ffunction-sections \
-fdata-sections \
-o build/Led_Hw_RaspberryPi.o
```

### Main

```bash
gcc -c main.c \
-std=c11 \
-Iapp \
-Iplatform \
-Wall \
-Wextra \
-Werror \
-ffunction-sections \
-fdata-sections \
-o build/main.o
```

---

## Linking

The final executable is produced by linking all object files with `libgpiod`:

```bash
gcc \
build/main.o \
build/LedControl.o \
build/Rte_LedControl.o \
build/Led_Hw_RaspberryPi.o \
$(pkg-config --libs libgpiod) \
-Wl,--gc-sections \
-o build/autosar_led
```

The result is:

```text
ELF 64-bit LSB PIE executable
ARM aarch64
dynamically linked
```

---

## ELF Inspection

The executable was inspected using:

```bash
file build/autosar_led
```

Result:

```text
ELF 64-bit LSB pie executable
ARM aarch64
dynamically linked
```

Size inspection:

```bash
size build/autosar_led
```

Example result:

```text
text    data    bss     dec
4500     776     32    5308
```

Symbols can be inspected using:

```bash
nm build/autosar_led
```

The executable contains:

```text
main
LedControl_Init
LedControl_Step
Rte_Write_LedCommand
Led_Hw_Init
Led_Hw_Write
```


## Execution

Run the program with:
```bash
sudo ./build/autosar_led
Initialization output:

```

The application then periodically toggles GPIO17.
---

## STM32 vs Raspberry Pi 5


ARM Cortex-M3                 ARM64 / aarch64
Bare-metal                    Linux
No operating system           Raspberry Pi OS
```text
Application
    |
RTE-like interface
    |
    v
Hardware abstraction
    v
Target-specific implementation
---

## Relation to AUTOSAR Concepts


Simplified educational architecture:
```text
Application
    |
    v
RTE-like Adapter
    |
    v
Hardware Abstraction
    v
Linux / libgpiod
    |
    v
Hardware
```

A production AUTOSAR Classic system would use a more complete architecture:
```text
Application SWC
      |
      v
      |
      v
Basic Software
      |
      v
IoHwAb
      |
      v
MCAL
      |
      v
Hardware
```

---

## Concepts Practiced

- Embedded C
- Linux ARM64
- Raspberry Pi 5
- GCC
- Object files
- Linking
- ELF executables
- Dynamic libraries
- `libgpiod`
- Linux GPIO character-device interface
- Hardware abstraction
- Application / hardware separation
- RTE-like communication
- Physical hardware validation

---

## Status

**✅ Completed — Manual compilation, object-file linking, ARM64 ELF generation, libgpiod integration and physical GPIO17 validation achieved on Raspberry Pi 5.**     RTE

    |

The example demonstrates the architectural principle of separating application functionality from hardware-specific code.
```

    |
    v

Despite these differences, the software architecture remains:

```
Direct GPIO registers         libgpiod
PC13                          GPIO17
firmware.elf                  Linux ELF executable
ST-Link flashing              Execute as a process
arm-none-eabi-gcc             gcc
Startup assembly              Linux runtime
MCU linker script             Linux linker
-------------------------------------------------------
STM32F103                     Raspberry Pi 5
```text
This example complements the STM32 bare-metal implementation.
The LED connected to GPIO17 physically changes state.


**Result: ✅ GPIO17 physical execution validated on Raspberry Pi 5.**

```text
GPIO17 initialized
```


Dynamic libraries can be inspected using:
---


```bash
ldd build/autosar_led
```

This demonstrates the difference between a Linux executable and a bare-metal STM32 firmware.
  v
        |


