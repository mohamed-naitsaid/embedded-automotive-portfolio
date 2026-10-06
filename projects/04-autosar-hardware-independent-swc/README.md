# AUTOSAR Classic Hardware-Independent SWC

AUTOSAR Classic demonstrator focused on **Application Layer portability across different hardware platforms**.

The project implements a periodic LED-control Software Component using **MATLAB/Simulink AUTOSAR Blockset**, generates C code and ARXML, and keeps the application logic independent from the target hardware.

## Targets

- PC Console — ✅ Validated
- STM32F103C8T6 / Proteus — ✅ Validated
- Raspberry Pi 5 / Linux GPIO — ⏳ Physical validation pending

> This project demonstrates AUTOSAR Application Layer concepts and SWC portability.  
> It is not a complete production AUTOSAR Classic stack with commercial RTE, BSW and MCAL configuration.

## Project Objective

The objective is to demonstrate that an AUTOSAR Application Software Component can remain independent from the hardware platform.

Only the hardware adaptation layer changes between targets.

```text
                    SWC_LedControl
                  AUTOSAR Application
                          |
                     Rte_IWrite()
                          |
                          v
                     RTE Adapter
                          |
                    Led_Hw_Write()
                          |
             +------------+-------------+
             |            |             |
             v            v             v
         PC Console     STM32       Raspberry Pi 5
                         PC13          GPIO17
                          |             |
                         LED           LED