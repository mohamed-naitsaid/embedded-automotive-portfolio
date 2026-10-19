# Embedded Automotive Portfolio 🚗⚡

Welcome to my **Embedded Software & Automotive Systems** portfolio.

I am an engineering student in **Automobile & Digital Solutions**, focused on **embedded automotive software, ARM Cortex-M, STM32, CAN/UDS, bootloaders and AUTOSAR Classic**.

This repository documents practical projects developed through:

- Embedded C programming
- Model-Based Development
- MCU integration
- Automotive communication
- Firmware update mechanisms
- Hardware abstraction
- Debugging
- Simulation
- Physical hardware validation
- Technical documentation

The objective is to demonstrate practical engineering skills through working implementations rather than simply listing technologies.

---

# 🎯 Main Focus

My current technical focus includes:

- Embedded C
- ARM Cortex-M
- STM32 microcontrollers
- Automotive bootloaders
- Flash memory programming
- CAN communication
- UDS diagnostics
- AUTOSAR Classic Application Layer
- MATLAB / Simulink
- ARXML and RTE interfaces
- Hardware abstraction
- Embedded software portability
- Automotive ECU architecture
- Software testing and debugging

---

# 🛠️ Technical Skills

## Programming

| Area | Technologies |
|---|---|
| Embedded Programming | C, Embedded C |
| Programming | C++, Python |
| Model-Based Development | MATLAB, Simulink |
| Scripting / Test Tools | Python, `python-can` |
| Version Control | Git, GitHub |

---

## Embedded Systems

Practical experience with:

- STM32F103C8T6
- STM32F446RE
- ARM Cortex-M3 / Cortex-M4 fundamentals
- GPIO
- Timers
- PWM
- External interrupts
- STM32 HAL
- STM32CubeIDE
- STM32CubeMX
- ST-Link
- Flash memory organization
- Firmware programming
- Hardware abstraction
- Embedded debugging

ARM / MCU concepts studied and applied:

- Memory map
- Flash / SRAM
- Vector table
- Startup sequence
- Reset Handler
- Main Stack Pointer — MSP
- Process Stack Pointer — PSP
- VTOR relocation
- Exception handling
- NVIC fundamentals
- SysTick fundamentals
- Linker scripts
- ELF / BIN / MAP files
- `.text`
- `.rodata`
- `.data`
- `.bss`
- Stack and heap
- `static`, `const`, `volatile`

---

# 🚗 Automotive Embedded Systems

## Practical / Project-Based

- Custom STM32 bootloader
- Firmware/Application memory separation
- Application jump
- Vector table relocation
- Flash erase and programming
- Firmware header
- CRC32 verification
- Classical CAN communication
- CAN-based firmware transfer
- UDS-based firmware update
- UDS services and negative responses
- AUTOSAR Application SWC
- Runnable and Timing Event
- Sender/Receiver communication
- ARXML generation
- RTE data-access interface
- Hardware-independent Application Layer
- STM32 / Raspberry Pi portability

---

## Currently Developing / Studying

- Full AUTOSAR Classic BSW architecture
- MCAL
- ECU Abstraction
- IoHwAb
- COM Stack
- COM
- PduR
- CanIf
- DCM
- DEM
- NvM
- AUTOSAR OS
- RTOS / FreeRTOS
- ISO 26262 fundamentals
- ASPICE fundamentals
- MISRA-C concepts

> These topics are listed as learning areas and are not presented as complete production-level implementations unless demonstrated by a project.

---

# 📂 Main Projects

---

# 01 — STM32F103 Custom Bootloader — CAN & UDS Firmware Update

Hardware-validated custom bootloader developed for the **STM32F103C8T6 / ARM Cortex-M3**.

[View Project](projects/01-stm32-custom-bootloader)

## Project Highlights

The project progressively evolved from a basic Bootloader → Application jump into a physical **CAN/UDS firmware update demonstrator**.

Implemented concepts include:

- Bootloader / Application Flash separation
- Custom linker regions
- Application vector table relocation
- MSP validation and loading
- Reset Handler validation
- `SCB->VTOR` relocation
- Flash erase and programming
- Firmware metadata header
- CRC32 firmware verification
- Classical CAN at **500 kbit/s**
- CAN firmware transfer
- Per-block acknowledgements
- UDS programming workflow
- Physical firmware update validation

### UDS Services Implemented

- `0x10` — DiagnosticSessionControl
- `0x11` — ECUReset
- `0x27` — SecurityAccess
- `0x34` — RequestDownload
- `0x36` — TransferData
- `0x37` — RequestTransferExit

The implementation is an educational **custom single-frame UDS subset over Classical CAN** and does not claim to implement a complete ISO-TP / production ISO 14229 stack.

---

## Memory Architecture

```text
STM32F103 Flash
0x08000000
     |
     | Custom Bootloader
     | 16 KiB
     |
0x08004000
     |
     | Firmware Header Area
     |
0x08004400
     |
     | Application Firmware
     |
0x08010000
```

---

## Hardware Architecture

```text
Windows PC
    |
    | Python + python-can
    v
USB-CAN Interface
    |
    | Classical CAN
    | 500 kbit/s
    v
CAN Transceiver
    |
    | PA11 RX
    | PA12 TX
    v
STM32F103C8T6
    |
    +--> Custom Bootloader
    |
    +--> Firmware Header
    |
    +--> Application
```

---

## Hardware-Validated Firmware Update

A complete UDS firmware update was validated on physical hardware.

The bootloader received a new application through CAN and programmed it into internal Flash.

### Before update

```text
Application V6.1
PC13 LED
Toggle period: 3 s
```

### After UDS update

```text
Application V6.2
PA8 external LED
Toggle period: 1 s
```

The transfer successfully completed:

```text
924 / 924 TransferData blocks acknowledged
CRC32 accepted
ECUReset positive response
New application executed
Power-cycle persistence validated
```

The application behavior changed without reflashing the bootloader through ST-Link.

**Status:** ✅ Hardware validated

---

## Technologies

- Embedded C
- STM32F103C8T6
- ARM Cortex-M3
- STM32 HAL
- STM32CubeIDE
- STM32CubeProgrammer
- ST-Link V2
- Classical CAN
- UDS
- Python
- `python-can`
- CANable / USB-CAN
- CRC32
- Git / GitHub

---

# 02 — AUTOSAR Classic Hardware-Independent SWC

AUTOSAR Classic demonstrator focused on **Application Layer portability and hardware-independent software design**.

[View Project](projects/02-autosar-hardware-independent-swc)

## Project Objective

The project demonstrates that the same generated AUTOSAR Application Software Component can execute on different hardware platforms without changing its application logic.

The AUTOSAR SWC was developed using:

- MATLAB R2024a
- Simulink
- AUTOSAR Blockset

The model generates:

- Embedded C code
- Component ARXML
- Interface ARXML
- Datatype ARXML
- Implementation ARXML

---

## AUTOSAR Software Component

```text
Component      : SWC_LedControl
Type           : Application SWC

Runnable       : SWC_LedControl_Step
Init Runnable  : SWC_LedControl_Init

Timing Event   : 500 ms

P-Port         : LedCommand
Data Element   : LedCommand

Communication  : Sender / Receiver
Access Mode    : ImplicitSend
```

The generated SWC communicates through:

```c
Rte_IWrite_SWC_LedControl_Step_LedCommand_LedCommand(...);
```

---

## Hardware-Independent Architecture

```text
                    SAME APPLICATION SWC
                       SWC_LedControl
                             |
                        Rte_IWrite()
                             |
                        RTE Adapter
                             |
                       Led_Hw_Write()
                        /          \
                       /            \
                      v              v
                STM32F103        Raspberry Pi 5
                STM32 HAL          libgpiod
                   |                  |
                  PC13              GPIO17
                   |                  |
                 LED ✅              LED ✅
```

The following application code remains unchanged:

```text
SWC_LedControl.c
```

Only the target-specific hardware layer changes:

```text
STM32F103
└── Led_Hw_STM32.c

Raspberry Pi 5
└── Led_Hw_RaspberryPi.c
```

---

## Validation

| Target | Status |
|---|---|
| PC Console | ✅ Validated |
| STM32F103 / Proteus | ✅ Validated |
| STM32F103 Physical Hardware | ✅ Validated |
| Raspberry Pi 5 / ARM64 | ✅ Validated |
| Raspberry Pi 5 / GPIO17 | ✅ Physical validation |
| Same Application SWC across targets | ✅ Validated |

---

## STM32F103 Physical Validation

```text
SWC_LedControl
      ↓
Rte_IWrite()
      ↓
RTE Adapter
      ↓
Led_Hw_STM32
      ↓
STM32 HAL
      ↓
PC13
      ↓
LED
```

The application was programmed and executed on a physical **STM32F103C8T6 Blue Pill**.

---

## Raspberry Pi 5 Physical Validation

The same generated Application SWC was compiled as a native:

```text
ELF 64-bit ARM aarch64
```

on Raspberry Pi OS 64-bit.

Runtime architecture:

```text
SWC_LedControl
      ↓
Rte_IWrite()
      ↓
RTE Adapter
      ↓
Led_Hw_RaspberryPi
      ↓
libgpiod
      ↓
GPIO17
      ↓
External LED
```

Physical GPIO17 blinking was successfully validated.

**Status:** ✅ Completed and physically validated on both hardware platforms.

---

## Technologies

- AUTOSAR Classic
- MATLAB R2024a
- Simulink
- AUTOSAR Blockset
- Embedded C
- ARXML
- RTE interfaces
- STM32F103C8T6
- STM32 HAL
- STM32CubeIDE
- Proteus
- Raspberry Pi 5
- Linux
- GCC
- `libgpiod`
- Git / GitHub

---

# 🔌 Automotive Communication

## CAN

Practical and theoretical work includes:

- Classical CAN
- Standard 11-bit identifiers
- CAN frame structure
- Arbitration
- DLC / Data Field
- CRC
- ACK
- Bit stuffing
- CAN errors
- CAN transceiver integration
- MCU CAN peripheral
- USB-CAN interface
- CAN firmware transfer

A physical CAN bus has been used for the STM32 bootloader project.

---

## UDS

Practical implementation currently includes:

```text
DiagnosticSessionControl
ECUReset
SecurityAccess
RequestDownload
TransferData
RequestTransferExit
```

The bootloader project also implements positive and negative responses for several programming-state and parameter errors.

The current implementation remains an educational subset and does not claim full ISO 14229 / ISO-TP compliance.

---

# 🧪 Model-Based Development & Simulation

Tools used in projects and coursework include:

- MATLAB
- Simulink
- Stateflow
- AUTOSAR Blockset
- Proteus
- Webots

Areas explored include:

- Automotive control logic
- EV modeling
- State-based systems
- ABS concepts
- Vehicle dynamics
- AUTOSAR SWC modeling
- Code generation
- MIL / SIL concepts

---

# 🔧 Development Tools

| Category | Tools |
|---|---|
| MCU Development | STM32CubeIDE, STM32CubeMX |
| Programming / Flashing | STM32CubeProgrammer, ST-Link |
| Model-Based Development | MATLAB, Simulink, AUTOSAR Blockset |
| Automotive Communication | CANable, PCAN-View, `python-can` |
| Simulation | Proteus, Webots |
| Linux | Raspberry Pi OS, GCC, Make |
| Version Control | Git, GitHub |
| Editors | VS Code |
| System Modeling | Cameo / SysML |
| Mechanical CAD | CATIA V5 |

---

# 📈 Demonstrated Progress

## Embedded C / ARM

- [x] Embedded C fundamentals
- [x] Bit manipulation
- [x] Pointer fundamentals
- [x] Structures / enums
- [x] `static`, `const`, `volatile`
- [x] Flash / SRAM understanding
- [x] `.text`, `.rodata`, `.data`, `.bss`
- [x] Stack fundamentals
- [x] Linker script fundamentals
- [x] ELF / BIN / MAP understanding
- [x] ARM Cortex-M startup fundamentals
- [x] MSP
- [x] Reset Handler
- [x] Vector Table
- [x] VTOR relocation

---

## STM32

- [x] STM32F103
- [x] STM32 HAL
- [x] GPIO
- [x] Timers / PWM coursework
- [x] External interrupts
- [x] Flash programming
- [x] Bootloader → Application jump
- [x] Firmware validation
- [x] CAN peripheral integration
- [x] Physical hardware testing

---

## Automotive Communication

- [x] CAN fundamentals
- [x] CAN frame structure
- [x] Arbitration
- [x] Bit stuffing concepts
- [x] Physical CAN communication
- [x] CAN-based firmware transfer
- [x] UDS fundamentals
- [x] UDS programming workflow
- [x] Physical UDS firmware update
- [ ] Complete ISO-TP implementation
- [ ] Complete ISO 14229 diagnostic stack

---

## AUTOSAR Classic

- [x] Layered architecture fundamentals
- [x] Application Software Component
- [x] Runnable Entity
- [x] Timing Event
- [x] Sender/Receiver Interface
- [x] P-Port
- [x] ARXML generation
- [x] RTE data-access interface
- [x] Simulink AUTOSAR code generation
- [x] Hardware-independent SWC
- [x] STM32 physical integration
- [x] Raspberry Pi 5 physical integration
- [ ] Production RTE configuration
- [ ] AUTOSAR OS
- [ ] Complete BSW configuration
- [ ] COM Stack implementation
- [ ] DCM / DEM implementation
- [ ] NvM implementation
- [ ] MCAL configuration

---

## Real-Time / Software Quality

- [x] RTOS scheduling fundamentals
- [x] Task / priority concepts
- [x] Queue / semaphore / mutex concepts
- [x] ASPICE fundamentals
- [x] ISO 26262 fundamentals
- [x] MISRA-C concepts
- [ ] Complete FreeRTOS project
- [ ] Complete ASPICE project workflow
- [ ] MISRA-C compliance project

---

# 🔬 Engineering Approach

For technical projects I follow a progressive engineering workflow:

```text
Requirements / Objective
          ↓
Architecture
          ↓
Design
          ↓
Implementation
          ↓
Build
          ↓
Test
          ↓
Debug
          ↓
Hardware Validation
          ↓
Documentation
          ↓
Git Version Control
```

The objective is not only to make a system work, but also to understand:

- how the software is organized,
- how it interacts with the hardware,
- how memory is used,
- how communication protocols operate,
- how failures are detected,
- how firmware can be updated,
- how software can remain portable,
- and how the implementation can be tested and documented.

---

# 🎯 Career Direction

My objective is to develop toward an:

**Embedded Software Engineer — Automotive**

with particular interest in:

- Embedded automotive software
- ECU development
- Bootloaders
- CAN communication
- UDS diagnostics
- AUTOSAR Classic
- Model-Based Development
- Test & Validation
- Automotive software architecture
- Embedded system integration

---

# 📌 Repository Philosophy

This repository is intentionally progressive.

A technology is presented as a practical skill when it has been implemented, tested or demonstrated through a project.

Topics still under development are explicitly identified as learning areas.

```text
Learn
  ↓
Understand
  ↓
Design
  ↓
Build
  ↓
Test
  ↓
Debug
  ↓
Validate on Hardware
  ↓
Document
  ↓
Improve
```

**Embedded Software • Automotive Systems • Build → Test → Validate 🚗⚡**