/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32 custom bootloader
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
    BOOT_MODE_NORMAL = 0U,
    BOOT_MODE_UPDATE
} BootloaderMode_t;

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

#define APP_HEADER_ADDRESS    0x08004000U
#define APP_START_ADDRESS     0x08004400U
#define APP_END_ADDRESS       0x08010000U

#define SRAM_START_ADDRESS    0x20000000U
#define SRAM_END_ADDRESS      0x20005000U

#define FW_MAGIC_NUMBER       0xB00710ADU
#define CRC32_POLYNOMIAL      0xEDB88320U

#define CMD_PING              'P'
#define CMD_START_UPDATE      'S'
#define CMD_ERASE             'R'
#define CMD_WRITE_TEST        'W'
#define CMD_DATA              'D'
#define CMD_HEADER            'H'
#define CMD_VERIFY            'V'
#define CMD_END_UPDATE        'E'

#define BL_ACK                'A'
#define BL_NACK               'N'
#define BL_READY              'Y'
#define BL_BYTE_RECEIVED      'K'
#define BL_BUFFER_RECEIVED    'B'

#define UART_RX_TIMEOUT       10000U
#define UART_TX_TIMEOUT       100U

#define FLASH_PAGE_SIZE       0x400U
#define UPDATE_START_ADDRESS  APP_HEADER_ADDRESS
#define UPDATE_PAGE_COUNT     48U

#define DATA_BLOCK_SIZE       8U
#define HEADER_DATA_SIZE      16U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

static uint8_t dataBuffer[DATA_BLOCK_SIZE];
static uint8_t headerBuffer[HEADER_DATA_SIZE];

static uint32_t currentWriteAddress = APP_START_ADDRESS;

static uint8_t uartRxByte;
static uint8_t uartTxByte;

static BootloaderMode_t bootMode = BOOT_MODE_NORMAL;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

/* USER CODE BEGIN PFP */

static void Bootloader_ProcessCommand(uint8_t command);

static uint8_t Bootloader_EraseFirmwareArea(void);

static uint8_t Bootloader_ProgramHalfWord(uint32_t address,
                                          uint16_t data);

static uint8_t Bootloader_ProgramBuffer(uint32_t address,
                                        const uint8_t *data,
                                        uint32_t length);

static uint8_t Bootloader_IsHeaderValid(void);
static uint8_t Bootloader_IsApplicationValid(void);
static uint8_t Bootloader_IsFirmwareIntegrityValid(void);

static uint32_t Bootloader_CalculateCRC32(uint32_t startAddress,
                                           uint32_t length);

static void Jump_To_Application(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static uint8_t Bootloader_ProgramBuffer(uint32_t address,
                                        const uint8_t *data,
                                        uint32_t length)
{
    uint32_t i;
    uint16_t halfWord;

    if ((data == NULL) || (length == 0U))
        return 0U;

    /* Flash is programmed here using 16-bit half-words. */
    if ((length & 0x1U) != 0U)
        return 0U;

    if ((address < UPDATE_START_ADDRESS) ||
        (address >= APP_END_ADDRESS))
        return 0U;

    if ((address + length) > APP_END_ADDRESS)
        return 0U;

    if ((address & 0x1U) != 0U)
        return 0U;

    HAL_FLASH_Unlock();

    for (i = 0U; i < length; i += 2U)
    {
        halfWord = (uint16_t)data[i] |
                   ((uint16_t)data[i + 1U] << 8U);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              address + i,
                              halfWord) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return 0U;
        }

        /* Verify what was actually written in Flash. */
        if (*(volatile uint16_t *)(address + i) != halfWord)
        {
            HAL_FLASH_Lock();
            return 0U;
        }
    }

    HAL_FLASH_Lock();

    return 1U;
}


static uint8_t Bootloader_ProgramHalfWord(uint32_t address,
                                          uint16_t data)
{
    if ((address < UPDATE_START_ADDRESS) ||
        (address >= APP_END_ADDRESS))
        return 0U;

    if ((address & 0x1U) != 0U)
        return 0U;

    HAL_FLASH_Unlock();

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                          address,
                          data) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return 0U;
    }

    HAL_FLASH_Lock();

    if (*(volatile uint16_t *)address != data)
        return 0U;

    return 1U;
}


static uint8_t Bootloader_EraseFirmwareArea(void)
{
    FLASH_EraseInitTypeDef eraseInit = {0};
    uint32_t pageError;

    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = UPDATE_START_ADDRESS;
    eraseInit.NbPages = UPDATE_PAGE_COUNT;

    HAL_FLASH_Unlock();

    if (HAL_FLASHEx_Erase(&eraseInit, &pageError) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return 0U;
    }

    HAL_FLASH_Lock();

    return 1U;
}


static uint32_t Bootloader_CalculateCRC32(uint32_t startAddress,
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
                crc = (crc >> 1U) ^ CRC32_POLYNOMIAL;
            else
                crc >>= 1U;
        }
    }

    return crc ^ 0xFFFFFFFFU;
}


static uint8_t Bootloader_IsHeaderValid(void)
{
    const volatile FirmwareHeader_t *header;
    uint32_t maxFirmwareSize;

    header =
        (const volatile FirmwareHeader_t *)APP_HEADER_ADDRESS;

    if (header->magic != FW_MAGIC_NUMBER)
        return 0U;

    maxFirmwareSize =
        APP_END_ADDRESS - APP_START_ADDRESS;

    if (header->firmware_size == 0U)
        return 0U;

    if (header->firmware_size > maxFirmwareSize)
        return 0U;

    return 1U;
}


static uint8_t Bootloader_IsApplicationValid(void)
{
    uint32_t appStack;
    uint32_t appResetHandler;
    uint32_t appResetAddress;

    appStack =
        *(volatile uint32_t *)APP_START_ADDRESS;

    appResetHandler =
        *(volatile uint32_t *)(APP_START_ADDRESS + 4U);

    if ((appStack == 0xFFFFFFFFU) ||
        (appResetHandler == 0xFFFFFFFFU))
        return 0U;

    /*
     * The initial MSP can be equal to the top of SRAM because
     * the stack grows downward.
     */
    if ((appStack < SRAM_START_ADDRESS) ||
        (appStack > SRAM_END_ADDRESS))
        return 0U;

    /* Cortex-M handler addresses must have the Thumb bit set. */
    if ((appResetHandler & 0x1U) == 0U)
        return 0U;

    appResetAddress =
        appResetHandler & ~0x1U;

    if ((appResetAddress < APP_START_ADDRESS) ||
        (appResetAddress >= APP_END_ADDRESS))
        return 0U;

    return 1U;
}


static uint8_t Bootloader_IsFirmwareIntegrityValid(void)
{
    const volatile FirmwareHeader_t *header;
    uint32_t calculatedCRC;

    header =
        (const volatile FirmwareHeader_t *)APP_HEADER_ADDRESS;

    calculatedCRC =
        Bootloader_CalculateCRC32(APP_START_ADDRESS,
                                  header->firmware_size);

    if (calculatedCRC != header->crc32)
        return 0U;

    return 1U;
}


static void Jump_To_Application(void)
{
    uint32_t appStack;
    uint32_t appResetHandler;
    pFunction appEntry;

    appStack =
        *(volatile uint32_t *)APP_START_ADDRESS;

    appResetHandler =
        *(volatile uint32_t *)(APP_START_ADDRESS + 4U);

    if ((appStack < SRAM_START_ADDRESS) ||
        (appStack > SRAM_END_ADDRESS))
        return;

    appEntry = (pFunction)appResetHandler;

    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    SCB->VTOR = APP_START_ADDRESS;

    __set_MSP(appStack);

    appEntry();
}


static void Bootloader_ProcessCommand(uint8_t command)
{
    switch (command)
    {
        case CMD_PING:
        {
            uartTxByte = BL_ACK;

            HAL_UART_Transmit(&huart1,
                              &uartTxByte,
                              1U,
                              UART_TX_TIMEOUT);
            break;
        }


        case CMD_START_UPDATE:
        {
            bootMode = BOOT_MODE_UPDATE;

            uartTxByte = BL_ACK;

            HAL_UART_Transmit(&huart1,
                              &uartTxByte,
                              1U,
                              UART_TX_TIMEOUT);
            break;
        }


        case CMD_ERASE:
        {
            if (bootMode != BOOT_MODE_UPDATE)
            {
                uartTxByte = BL_NACK;
            }
            else if (Bootloader_EraseFirmwareArea() == 1U)
            {
                currentWriteAddress = APP_START_ADDRESS;
                uartTxByte = BL_ACK;
            }
            else
            {
                uartTxByte = BL_NACK;
            }

            HAL_UART_Transmit(&huart1,
                              &uartTxByte,
                              1U,
                              UART_TX_TIMEOUT);
            break;
        }


        case CMD_WRITE_TEST:
        {
            if ((bootMode == BOOT_MODE_UPDATE) &&
                (Bootloader_ProgramHalfWord(APP_START_ADDRESS,
                                            0x1234U) == 1U))
            {
                uartTxByte = BL_ACK;
            }
            else
            {
                uartTxByte = BL_NACK;
            }

            HAL_UART_Transmit(&huart1,
                              &uartTxByte,
                              1U,
                              UART_TX_TIMEOUT);
            break;
        }


        case CMD_DATA:
        {
            uint32_t i;
            uint8_t receiveOk = 1U;

            if (bootMode != BOOT_MODE_UPDATE)
            {
                uartTxByte = BL_NACK;

                HAL_UART_Transmit(&huart1,
                                  &uartTxByte,
                                  1U,
                                  UART_TX_TIMEOUT);
                break;
            }

            /* Tell the PC that the STM32 is ready for the block. */
            uartTxByte = BL_READY;

            HAL_UART_Transmit(&huart1,
                              &uartTxByte,
                              1U,
                              UART_TX_TIMEOUT);

            for (i = 0U; i < DATA_BLOCK_SIZE; i++)
            {
                if (HAL_UART_Receive(&huart1,
                                     &dataBuffer[i],
                                     1U,
                                     HAL_MAX_DELAY) != HAL_OK)
                {
                    receiveOk = 0U;
                    break;
                }

                uartTxByte = BL_BYTE_RECEIVED;

                HAL_UART_Transmit(&huart1,
                                  &uartTxByte,
                                  1U,
                                  UART_TX_TIMEOUT);
            }

            if (receiveOk == 0U)
            {
                uartTxByte = BL_NACK;

                HAL_UART_Transmit(&huart1,
                                  &uartTxByte,
                                  1U,
                                  UART_TX_TIMEOUT);
                break;
            }

            /* Complete block is now available in RAM. */
            uartTxByte = BL_BUFFER_RECEIVED;

            HAL_UART_Transmit(&huart1,
                              &uartTxByte,
                              1U,
                              UART_TX_TIMEOUT);

            if (Bootloader_ProgramBuffer(currentWriteAddress,
                                         dataBuffer,
                                         DATA_BLOCK_SIZE) == 1U)
            {
                currentWriteAddress += DATA_BLOCK_SIZE;
                uartTxByte = BL_ACK;
            }
            else
            {
                uartTxByte = BL_NACK;
            }

            HAL_UART_Transmit(&huart1,
                              &uartTxByte,
                              1U,
                              UART_TX_TIMEOUT);

            break;
        }


        case CMD_HEADER:
        {
            uint32_t i;
            uint8_t receiveOk = 1U;

            if (bootMode != BOOT_MODE_UPDATE)
            {
                uartTxByte = BL_NACK;

                HAL_UART_Transmit(&huart1,
                                  &uartTxByte,
                                  1U,
                                  UART_TX_TIMEOUT);
                break;
            }

            uartTxByte = BL_READY;

            HAL_UART_Transmit(&huart1,
                              &uartTxByte,
                              1U,
                              UART_TX_TIMEOUT);

            for (i = 0U; i < HEADER_DATA_SIZE; i++)
            {
                if (HAL_UART_Receive(&huart1,
                                     &headerBuffer[i],
                                     1U,
                                     HAL_MAX_DELAY) != HAL_OK)
                {
                    receiveOk = 0U;
                    break;
                }

                uartTxByte = BL_BYTE_RECEIVED;

                HAL_UART_Transmit(&huart1,
                                  &uartTxByte,
                                  1U,
                                  UART_TX_TIMEOUT);
            }

            if (receiveOk == 0U)
            {
                uartTxByte = BL_NACK;

                HAL_UART_Transmit(&huart1,
                                  &uartTxByte,
                                  1U,
                                  UART_TX_TIMEOUT);
                break;
            }

            uartTxByte = BL_BUFFER_RECEIVED;

            HAL_UART_Transmit(&huart1,
                              &uartTxByte,
                              1U,
                              UART_TX_TIMEOUT);

            if (Bootloader_ProgramBuffer(APP_HEADER_ADDRESS,
                                         headerBuffer,
                                         HEADER_DATA_SIZE) == 1U)
            {
                uartTxByte = BL_ACK;
            }
            else
            {
                uartTxByte = BL_NACK;
            }

            HAL_UART_Transmit(&huart1,
                              &uartTxByte,
                              1U,
                              UART_TX_TIMEOUT);

            break;
        }


        case CMD_VERIFY:
        {
            if ((bootMode == BOOT_MODE_UPDATE) &&
                (Bootloader_IsHeaderValid() == 1U) &&
                (Bootloader_IsApplicationValid() == 1U) &&
                (Bootloader_IsFirmwareIntegrityValid() == 1U))
            {
                uartTxByte = BL_ACK;
            }
            else
            {
                uartTxByte = BL_NACK;
            }

            HAL_UART_Transmit(&huart1,
                              &uartTxByte,
                              1U,
                              UART_TX_TIMEOUT);

            break;
        }


        case CMD_END_UPDATE:
        {
            if ((bootMode == BOOT_MODE_UPDATE) &&
                (Bootloader_IsHeaderValid() == 1U) &&
                (Bootloader_IsApplicationValid() == 1U) &&
                (Bootloader_IsFirmwareIntegrityValid() == 1U))
            {
                uartTxByte = BL_ACK;

                /*
                 * HAL_MAX_DELAY is intentional here.
                 * We want the ACK to leave USART before resetting.
                 */
                HAL_UART_Transmit(&huart1,
                                  &uartTxByte,
                                  1U,
                                  HAL_MAX_DELAY);

                /*
                 * Proteus + COMPIM needs some time to propagate
                 * the last character before the reset.
                 */
                HAL_Delay(1000U);

                NVIC_SystemReset();
            }
            else
            {
                uartTxByte = BL_NACK;

                HAL_UART_Transmit(&huart1,
                                  &uartTxByte,
                                  1U,
                                  UART_TX_TIMEOUT);
            }

            break;
        }


        default:
        {
            uartTxByte = BL_NACK;

            HAL_UART_Transmit(&huart1,
                              &uartTxByte,
                              1U,
                              UART_TX_TIMEOUT);
            break;
        }
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

    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    MX_GPIO_Init();
    MX_USART1_UART_Init();

    /* USER CODE BEGIN 2 */

    /*
     * Give the PC a short window to request update mode.
     * Sending 'S' keeps execution inside the bootloader.
     */
    if (HAL_UART_Receive(&huart1,
                         &uartRxByte,
                         1U,
                         UART_RX_TIMEOUT) == HAL_OK)
    {
        Bootloader_ProcessCommand(uartRxByte);
    }

    if (bootMode == BOOT_MODE_UPDATE)
    {
        while (1)
        {
            if (HAL_UART_Receive(&huart1,
                                 &uartRxByte,
                                 1U,
                                 HAL_MAX_DELAY) == HAL_OK)
            {
                Bootloader_ProcessCommand(uartRxByte);
            }
        }
    }

    /*
     * No update request received.
     * Try to start the installed firmware.
     */
    if ((Bootloader_IsHeaderValid() == 1U) &&
        (Bootloader_IsApplicationValid() == 1U) &&
        (Bootloader_IsFirmwareIntegrityValid() == 1U))
    {
        Jump_To_Application();
    }

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */

    while (1)
    {
        /* Stay in the bootloader if no valid application exists. */

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


/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
    /* USER CODE BEGIN USART1_Init 0 */

    /* USER CODE END USART1_Init 0 */

    /* USER CODE BEGIN USART1_Init 1 */

    /* USER CODE END USART1_Init 1 */

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN USART1_Init 2 */

    /* USER CODE END USART1_Init 2 */
}


/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    /* USER CODE BEGIN MX_GPIO_Init_1 */

    /* USER CODE END MX_GPIO_Init_1 */

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* USER CODE BEGIN MX_GPIO_Init_2 */

    /* USER CODE END MX_GPIO_Init_2 */
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
  * @brief Reports the name of the source file and the source line number
  * where the assert_param error has occurred.
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */

    /* USER CODE END 6 */
}

#endif /* USE_FULL_ASSERT */
