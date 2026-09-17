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
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
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
/* USER CODE END PTD */


/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APP_START_ADDRESS   0x08004000U
#define APP_END_ADDRESS     0x08010000U

#define SRAM_START_ADDRESS  0x20000000U
#define SRAM_END_ADDRESS    0x20005000U


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
uint8_t Bootloader_IsApplicationValid(void);
void Jump_To_Application(void);
/* USER CODE END PFP */
/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint8_t Bootloader_IsApplicationValid(void)
{
    uint32_t appStack;
    uint32_t appResetHandler;
    uint32_t appResetAddress;

    /* Read initial MSP */
    appStack =
        *(volatile uint32_t *)APP_START_ADDRESS;

    /* Read application Reset_Handler */
    appResetHandler =
        *(volatile uint32_t *)(APP_START_ADDRESS + 4U);

    /* Check if application Flash area is empty */
    if ((appStack == 0xFFFFFFFFU) ||
        (appResetHandler == 0xFFFFFFFFU))
    {
        return 0U;
    }

    /* Check MSP inside SRAM */
    if ((appStack < SRAM_START_ADDRESS) ||
        (appStack > SRAM_END_ADDRESS))
    {
        return 0U;
    }

    /* Cortex-M handler must have Thumb bit set */
    if ((appResetHandler & 0x1U) == 0U)
    {
        return 0U;
    }

    /* Remove Thumb bit for address validation */
    appResetAddress = appResetHandler & ~0x1U;

    /* Reset_Handler must belong to Application Flash */
    if ((appResetAddress < APP_START_ADDRESS) ||
        (appResetAddress >= APP_END_ADDRESS))
    {
        return 0U;
    }

    return 1U;
}


void Jump_To_Application(void)
{
    uint32_t appStack;
    uint32_t appResetHandler;
    pFunction appEntry;

    /* Read application vector table */
    appStack =
        *(volatile uint32_t *)APP_START_ADDRESS;

    appResetHandler =
        *(volatile uint32_t *)(APP_START_ADDRESS + 4U);

    /* Check MSP */
    if ((appStack >= SRAM_START_ADDRESS) &&
        (appStack <= SRAM_END_ADDRESS))
    {
        appEntry = (pFunction)appResetHandler;

        /* Disable bootloader interrupts */
        __disable_irq();

        /* Stop SysTick */
        SysTick->CTRL = 0U;
        SysTick->LOAD = 0U;
        SysTick->VAL  = 0U;

        /* Relocate vector table */
        SCB->VTOR = APP_START_ADDRESS;

        /* Load application MSP */
        __set_MSP(appStack);

        /* Jump to application Reset_Handler */
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

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  if (Bootloader_IsApplicationValid() == 1U)
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
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
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
