# Software Architecture

## Overview

The objective of this project is to keep the AUTOSAR Application Software Component independent from the target hardware.

```text
                     Application Layer
                  +----------------------+
                  |    SWC_LedControl    |
                  |                      |
                  | Runnable: Step()     |
                  | Period: 500 ms       |
                  +----------+-----------+
                             |
                        Rte_IWrite()
                             |
                             v
                  +----------------------+
                  |     RTE Adapter      |
                  |    Rte_Adapter.c     |
                  +----------+-----------+
                             |
                       Led_Hw_Write()
                             |
                  +----------+-----------+
                  |      Led_Hw API      |
                  +----------+-----------+
                             |
             +---------------+----------------+
             |               |                |
             v               v                v
        PC Console       STM32F103       Raspberry Pi 5
        printf()         STM32 HAL        libgpiod
                            |                |
                          PC13             GPIO17
                            |                |
                           LED              LED