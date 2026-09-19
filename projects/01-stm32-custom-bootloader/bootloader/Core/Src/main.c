/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef void (*pFunction)(void);

typedef struct
{
    uint32_t magic;
    uint32_t firmware_size;
    uint32_t crc32;
    uint32_t version;

} FirmwareHeader_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define APP_HEADER_ADDRESS   0x08004000U
#define APP_HEADER_SIZE      0x00000400U

#define APP_START_ADDRESS    0x08004400U
#define APP_END_ADDRESS      0x08010000U

#define SRAM_START_ADDRESS   0x20000000U
#define SRAM_END_ADDRESS     0x20005000U

#define FW_MAGIC_NUMBER      0xB00710ADU

/* CRC-32 reflected polynomial */
#define CRC32_POLYNOMIAL     0xEDB88320U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

void SystemClock_Config(void);

uint8_t Bootloader_IsHeaderValid(void);

uint8_t Bootloader_IsApplicationValid(void);

uint32_t Bootloader_CalculateCRC32(uint32_t startAddress,
                                   uint32_t length);

uint8_t Bootloader_IsFirmwareIntegrityValid(void);

void Jump_To_Application(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief Calculate CRC32 over a Flash memory region.
 *
 * CRC configuration:
 * - Initial value : 0xFFFFFFFF
 * - Polynomial    : 0xEDB88320
 * - Final XOR     : 0xFFFFFFFF
 *
 * This corresponds to the standard reflected CRC-32
 * used by Python zlib.crc32().
 */
uint32_t Bootloader_CalculateCRC32(uint32_t startAddress,
                                   uint32_t length)
{
    const volatile uint8_t *data;
    uint32_t crc;
    uint32_t i;
    uint8_t bit;

    data = (const volatile uint8_t *)startAddress;

    crc = 0xFFFFFFFFU;

    for (i = 0U; i < length; i++)
    {
        crc ^= (uint32_t)data[i];

        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x1U) != 0U)
            {
                crc = (crc >> 1U) ^ CRC32_POLYNOMIAL;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc ^ 0xFFFFFFFFU;
}


/**
 * @brief Validate firmware header.
 */
uint8_t Bootloader_IsHeaderValid(void)
{
    const volatile FirmwareHeader_t *header;
    uint32_t maxFirmwareSize;

    header =
        (const volatile FirmwareHeader_t *)APP_HEADER_ADDRESS;

    /*
     * Check Magic Number.
     */
    if (header->magic != FW_MAGIC_NUMBER)
    {
        return 0U;
    }

    /*
     * Maximum available Application Flash size:
     *
     * 0x08010000 - 0x08004400
     * = 0xBC00
     * = 48128 bytes
     */
    maxFirmwareSize =
        APP_END_ADDRESS - APP_START_ADDRESS;

    /*
     * Firmware cannot have a zero size.
     */
    if (header->firmware_size == 0U)
    {
        return 0U;
    }

    /*
     * Firmware must fit inside the Application region.
     */
    if (header->firmware_size > maxFirmwareSize)
    {
        return 0U;
    }

    return 1U;
}


/**
 * @brief Validate Application Vector Table.
 */
uint8_t Bootloader_IsApplicationValid(void)
{
    uint32_t appStack;
    uint32_t appResetHandler;
    uint32_t appResetAddress;

    /*
     * First Application Vector Table entry:
     * Initial MSP.
     */
    appStack =
        *(volatile uint32_t *)APP_START_ADDRESS;

    /*
     * Second Application Vector Table entry:
     * Reset_Handler.
     */
    appResetHandler =
        *(volatile uint32_t *)(APP_START_ADDRESS + 4U);

    /*
     * Check if Application Flash appears empty.
     */
    if ((appStack == 0xFFFFFFFFU) ||
        (appResetHandler == 0xFFFFFFFFU))
    {
        return 0U;
    }

    /*
     * Initial MSP must belong to SRAM.
     *
     * SRAM:
     * 0x20000000 -> 0x20005000
     */
    if ((appStack < SRAM_START_ADDRESS) ||
        (appStack > SRAM_END_ADDRESS))
    {
        return 0U;
    }

    /*
     * Cortex-M exception handler addresses
     * must have Thumb bit (bit 0) set.
     */
    if ((appResetHandler & 0x1U) == 0U)
    {
        return 0U;
    }

    /*
     * Remove Thumb bit before checking
     * physical Flash address.
     */
    appResetAddress =
        appResetHandler & ~0x1U;

    /*
     * Reset_Handler must belong to the
     * Application Flash region.
     */
    if ((appResetAddress < APP_START_ADDRESS) ||
        (appResetAddress >= APP_END_ADDRESS))
    {
        return 0U;
    }

    return 1U;
}


/**
 * @brief Validate Application firmware CRC32.
 */
uint8_t Bootloader_IsFirmwareIntegrityValid(void)
{
    const volatile FirmwareHeader_t *header;
    uint32_t calculatedCRC;

    header =
        (const volatile FirmwareHeader_t *)APP_HEADER_ADDRESS;

    /*
     * Calculate CRC over the real Application firmware.
     */
    calculatedCRC =
        Bootloader_CalculateCRC32(
            APP_START_ADDRESS,
            header->firmware_size
        );

    /*
     * Compare calculated CRC with the CRC
     * stored inside the Firmware Header.
     */
    if (calculatedCRC != header->crc32)
    {
        return 0U;
    }

    return 1U;
}


/**
 * @brief Transfer execution from Bootloader to Application.
 */
void Jump_To_Application(void)
{
    uint32_t appStack;
    uint32_t appResetHandler;
    pFunction appEntry;

    /*
     * Read Application Vector Table.
     */
    appStack =
        *(volatile uint32_t *)APP_START_ADDRESS;

    appResetHandler =
        *(volatile uint32_t *)(APP_START_ADDRESS + 4U);

    /*
     * Final MSP safety check.
     */
    if ((appStack >= SRAM_START_ADDRESS) &&
        (appStack <= SRAM_END_ADDRESS))
    {
        /*
         * Convert Reset_Handler address
         * into a callable function pointer.
         */
        appEntry =
            (pFunction)appResetHandler;

        /*
         * Disable Bootloader interrupts
         * during the transition.
         */
        __disable_irq();

        /*
         * Stop Bootloader SysTick.
         */
        SysTick->CTRL = 0U;
        SysTick->LOAD = 0U;
        SysTick->VAL  = 0U;

        /*
         * Relocate Vector Table to Application.
         */
        SCB->VTOR = APP_START_ADDRESS;

        /*
         * Load Application Main Stack Pointer.
         */
        __set_MSP(appStack);

        /*
         * Jump to Application Reset_Handler.
         */
        appEntry();
    }
}

/* USER CODE END 0 */


/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /*
     * Reset of all peripherals,
     * Initializes the Flash interface and SysTick.
     */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /*
     * Configure the system clock.
     */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /*
     * V3 Boot validation sequence:
     *
     * 1. Header validation
     * 2. Vector Table validation
     * 3. Firmware CRC32 validation
     * 4. Jump to Application
     */
    if ((Bootloader_IsHeaderValid() == 1U) &&
        (Bootloader_IsApplicationValid() == 1U) &&
        (Bootloader_IsFirmwareIntegrityValid() == 1U))
    {
        Jump_To_Application();
    }

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */

    /* USER CODE BEGIN 2 */

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */

    while (1)
    {
        /*
         * If we reach this point,
         * the firmware was not considered valid.
         */

        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }

    /* USER CODE END 3 */
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /**
     * Initializes the RCC Oscillators
     * according to the specified parameters.
     */
    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;

    RCC_OscInitStruct.HSIState =
        RCC_HSI_ON;

    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /**
     * Initializes CPU, AHB and APB clocks.
     */
    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_HSI;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV1;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct,
                            FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}


/* USER CODE BEGIN 4 */

/* USER CODE END 4 */


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */

    __disable_irq();

    while (1)
    {
    }

    /* USER CODE END Error_Handler_Debug */
}


#ifdef USE_FULL_ASSERT

/**
  * @brief Reports the name of the source file and
  *        the source line number where assert_param
  *        error has occurred.
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */

    /*
     * User can add implementation here.
     */

    /* USER CODE END 6 */
}

#endif /* USE_FULL_ASSERT */
