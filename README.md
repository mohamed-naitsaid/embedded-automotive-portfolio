# Embedded Automotive Portfolio 🚗⚡

Welcome to my Embedded Software & Automotive Systems portfolio.

I am an engineering student in **Automobile & Digital Solutions**, with a strong interest in **embedded software, automotive ECUs, ARM microcontrollers and AUTOSAR Classic**.

This repository documents my practical learning journey through embedded projects, simulations, code generation, debugging and automotive software concepts.

The objective is not to present every technology as mastered, but to progressively build and demonstrate practical skills through working projects.

---

## 🎯 Main Focus

My current technical focus is:

- Embedded C
- STM32 microcontrollers
- ARM Cortex-M architecture
- Bootloader development
- Automotive communication fundamentals
- CAN and UDS fundamentals
- AUTOSAR Classic Application Layer
- MATLAB / Simulink model-based development
- Hardware abstraction and portability
- Embedded software architecture
- Automotive ECU concepts

---

# 🛠️ Technical Skills

## Programming

| Technology | Level / Usage |
|---|---|
| C | Practical use in embedded projects |
| Embedded C | Practical use with STM32 and generated AUTOSAR code |
| C++ | Academic / basic practical use |
| MATLAB | Practical use in engineering and Simulink projects |
| Python | Academic and engineering scripting |
| Git / GitHub | Used for project versioning and documentation |

---

## Embedded Systems

Practical experience with:

- STM32F103
- STM32F446
- ARM Cortex-M fundamentals
- GPIO
- Timers
- PWM
- External interrupts
- STM32 HAL
- STM32CubeIDE
- STM32CubeMX
- Startup code fundamentals
- Vector table
- MSP / Stack Pointer
- Reset Handler
- VTOR relocation
- Flash / SRAM memory organization
- Firmware validation
- Hardware abstraction

Working knowledge of:

- UART
- SPI
- I2C
- DMA
- Watchdog concepts
- Interrupt handling
- Memory-mapped peripherals

---

# 🚗 Automotive Embedded Systems

## Practical / Project-Based

- AUTOSAR Classic Application Software Component
- AUTOSAR Runnable configuration
- Timing Events
- Sender/Receiver communication
- P-Port configuration
- ARXML generation
- RTE data-access interfaces
- Hardware-independent Application Layer
- STM32 integration
- CAN fundamentals
- UDS fundamentals
- Automotive bootloader concepts

## Fundamentals Currently Studied

- AUTOSAR BSW architecture
- MCAL
- ECU Abstraction
- COM Stack
- COM
- PduR
- CanIf
- DCM
- DEM
- NvM
- RTOS / FreeRTOS fundamentals
- ISO 26262 fundamentals
- ASPICE fundamentals
- MISRA-C concepts

These topics are listed as learning areas and are not presented as full production-level implementations.

---

# 🧠 ARM Cortex-M Knowledge

Topics studied and applied during STM32 projects include:

- CPU and core concepts
- General-purpose registers
- Program Counter
- Link Register
- Stack Pointer
- MSP and PSP
- Exception handling
- NVIC fundamentals
- SysTick fundamentals
- Thread mode and Handler mode
- Privileged and unprivileged execution
- Fetch / Decode / Execute
- Memory map
- Flash and SRAM
- Vector table
- Startup code
- Reset sequence
- Stack initialization
- VTOR

---

# 📂 Main Projects

---

## 01 — STM32F103 Custom Bootloader

Custom bootloader developed for the **STM32F103C8T6 / ARM Cortex-M3**.

### Implemented concepts

- Bootloader and Application memory separation
- Application located at a custom Flash offset
- Reading the initial Main Stack Pointer from the application vector table
- Reading the Application Reset Handler
- MSP update before application jump
- VTOR relocation
- Jump from Bootloader to Application
- Application address validation
- Stack Pointer validation
- Reset Handler validation

### Memory architecture

```text
FLASH
0x08000000
     |
     | Bootloader
     |
0x08004000
     |
     | Application
     |
0x08010000