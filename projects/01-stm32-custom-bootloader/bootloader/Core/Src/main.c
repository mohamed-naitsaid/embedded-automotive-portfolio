/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32 custom bootloader
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "uds.h"
/* USER CODE END Includes */
/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
    BOOT_MODE_NORMAL = 0U,
    BOOT_MODE_UPDATE
} BootloaderMode_t;
typedef enum
{
    BOOT_TRANSPORT_NONE = 0U,
    BOOT_TRANSPORT_UART,
    BOOT_TRANSPORT_CAN
} BootloaderTransport_t;
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
#define PROTEUS_SIMULATION       1U
#define CAN_ID_COMMAND           0x600U
#define CAN_ID_DATA              0x601U
#define CAN_ID_HEADER            0x602U
#define CAN_ID_RESPONSE          0x650U
#define CAN_ID_UDS_REQUEST       0x7E0U
#define CAN_ID_UDS_RESPONSE      0x7E8U
#define CAN_SEQUENCE_SIZE        2U
#define CAN_DATA_PAYLOAD_SIZE    6U
#define CAN_RESPONSE_DLC         5U
#define CAN_TX_TIMEOUT           100U
#define CAN_SIM_FRAME_START      0xC5U
#define CAN_ERROR_NONE           0x00U
#define CAN_ERROR_STATE          0x01U
#define CAN_ERROR_LENGTH         0x02U
#define CAN_ERROR_SEQUENCE       0x03U
#define CAN_ERROR_FLASH          0x04U
#define CAN_ERROR_HEADER         0x05U
#define CAN_ERROR_CRC            0x06U
#define CAN_ERROR_RANGE          0x07U
#define APP_HEADER_ADDRESS       0x08004000U
#define APP_START_ADDRESS        0x08004400U
#define APP_END_ADDRESS          0x08010000U
#define SRAM_START_ADDRESS       0x20000000U
#define SRAM_END_ADDRESS         0x20005000U
#define FW_MAGIC_NUMBER          0xB00710ADU
#define CRC32_POLYNOMIAL         0xEDB88320U
#define CMD_PING                 'P'
#define CMD_START_UPDATE         'S'
#define CMD_ERASE                'R'
#define CMD_WRITE_TEST           'W'
#define CMD_DATA                 'D'
#define CMD_HEADER               'H'
#define CMD_VERIFY               'V'
#define CMD_END_UPDATE           'E'
#define BL_ACK                   'A'
#define BL_NACK                  'N'
#define BL_READY                 'Y'
#define BL_BYTE_RECEIVED         'K'
#define BL_BUFFER_RECEIVED       'B'
#if PROTEUS_SIMULATION
#define UART_RX_TIMEOUT          60000U
#else
#define UART_RX_TIMEOUT          3000U
#endif
#define UART_TX_TIMEOUT          100U
#define FLASH_PAGE_SIZE          0x400U
#define UPDATE_START_ADDRESS     APP_HEADER_ADDRESS
#define UPDATE_PAGE_COUNT        48U
#define DATA_BLOCK_SIZE          8U
#define HEADER_DATA_SIZE         16U
/* USER CODE END PD */
/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */
/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart1;
/* USER CODE BEGIN PV */
static CAN_TxHeaderTypeDef canTxHeader;
static CAN_RxHeaderTypeDef canRxHeader;
static uint8_t canTxData[8];
static uint8_t canRxData[8];
static uint32_t canTxMailbox;
static uint32_t canFirmwareSize;
static uint32_t canFirmwareBytesReceived;
static uint16_t canExpectedDataSequence;
static uint16_t canExpectedHeaderSequence;
static uint8_t canHeaderBytesReceived;
static uint8_t canEraseDone;
static uint8_t canHeaderWritten;
static uint8_t canVerified;
static uint8_t dataBuffer[DATA_BLOCK_SIZE];
static uint8_t headerBuffer[HEADER_DATA_SIZE];
static uint32_t currentWriteAddress = APP_START_ADDRESS;
static uint8_t uartRxByte;
static uint8_t uartTxByte;
static BootloaderMode_t bootMode = BOOT_MODE_NORMAL;
static BootloaderTransport_t bootTransport = BOOT_TRANSPORT_NONE;

static uint32_t udsFlashWriteAddress = APP_START_ADDRESS;
static uint8_t udsFlashPendingByte;
static uint8_t udsFlashPendingByteValid;
/* USER CODE END PV */
/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_CAN_Init(void);
/* USER CODE BEGIN PFP */
static uint8_t Bootloader_CAN_Init(void);
static void Bootloader_CAN_ResetUpdateState(void);
static uint8_t Bootloader_CAN_SendResponse(uint8_t response,
                                           uint8_t context,
                                           uint16_t sequence,
                                           uint8_t errorCode);
static void Bootloader_CAN_Process(void);
static uint8_t Bootloader_CAN_SimReceiveFrame(void);
static uint8_t Bootloader_UDS_SendFrame(const uint8_t *data,
                                        uint8_t dlc);
static uint8_t Bootloader_UDS_ProgramTransferBlock(const uint8_t *data,
                                                   uint8_t length,
                                                   uint8_t finalBlock);
static void Bootloader_UDS_ProcessFrame(void);
static void Bootloader_CAN_ProcessCommand(void);
static void Bootloader_CAN_ProcessData(void);
static void Bootloader_CAN_ProcessHeader(void);
static uint8_t Bootloader_CAN_ValidateHeaderBuffer(uint8_t *errorCode);
static void Bootloader_WaitForUpdateRequest(void);
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
static uint32_t Bootloader_ReadUint32LE(const uint8_t *data);
static void Jump_To_Application(void);
/* USER CODE END PFP */
/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint32_t Bootloader_ReadUint32LE(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}
static uint8_t Bootloader_CAN_Init(void)
{
    CAN_FilterTypeDef filter = {0};
    /*
     * One 16-bit list filter accepts the three V5 IDs and the UDS request ID.
     * Standard identifiers are stored left-shifted by 5 bits.
     */
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDLIST;
    filter.FilterScale = CAN_FILTERSCALE_16BIT;
    filter.FilterIdHigh = (CAN_ID_COMMAND << 5U);
    filter.FilterIdLow = (CAN_ID_DATA << 5U);
    filter.FilterMaskIdHigh = (CAN_ID_HEADER << 5U);
    filter.FilterMaskIdLow = (CAN_ID_UDS_REQUEST << 5U);
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;
    if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK)
        return 0U;
    if (HAL_CAN_Start(&hcan) != HAL_OK)
        return 0U;
    return 1U;
}
static void Bootloader_CAN_ResetUpdateState(void)
{
    uint32_t i;
    currentWriteAddress = APP_START_ADDRESS;
    canFirmwareBytesReceived = 0U;
    canExpectedDataSequence = 0U;
    canExpectedHeaderSequence = 0U;
    canHeaderBytesReceived = 0U;
    canEraseDone = 0U;
    canHeaderWritten = 0U;
    canVerified = 0U;
    for (i = 0U; i < HEADER_DATA_SIZE; i++)
        headerBuffer[i] = 0U;
}
static uint8_t Bootloader_UDS_SendFrame(const uint8_t *data,
                                        uint8_t dlc)
{
    uint32_t startTick;
    uint32_t i;
    uint8_t simFrame[12];

    if ((data == NULL) || (dlc == 0U) || (dlc > 8U))
    {
        return 0U;
    }

    canTxHeader.StdId = CAN_ID_UDS_RESPONSE;
    canTxHeader.ExtId = 0U;
    canTxHeader.IDE = CAN_ID_STD;
    canTxHeader.RTR = CAN_RTR_DATA;
    canTxHeader.DLC = dlc;
    canTxHeader.TransmitGlobalTime = DISABLE;

    for (i = 0U; i < dlc; i++)
    {
        canTxData[i] = data[i];
    }

    if (PROTEUS_SIMULATION != 0U)
    {
        simFrame[0] = CAN_SIM_FRAME_START;
        simFrame[1] = (uint8_t)(CAN_ID_UDS_RESPONSE & 0xFFU);
        simFrame[2] = (uint8_t)((CAN_ID_UDS_RESPONSE >> 8U) & 0xFFU);
        simFrame[3] = dlc;

        for (i = 0U; i < dlc; i++)
        {
            simFrame[4U + i] = data[i];
        }

        if (HAL_UART_Transmit(&huart1,
                              simFrame,
                              4U + dlc,
                              HAL_MAX_DELAY) != HAL_OK)
        {
            return 0U;
        }

        return 1U;
    }

    startTick = HAL_GetTick();

    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0U)
    {
        if ((HAL_GetTick() - startTick) >= CAN_TX_TIMEOUT)
        {
            return 0U;
        }
    }

    if (HAL_CAN_AddTxMessage(&hcan,
                             &canTxHeader,
                             canTxData,
                             &canTxMailbox) != HAL_OK)
    {
        return 0U;
    }

    return 1U;
}

static uint8_t Bootloader_UDS_ProgramTransferBlock(const uint8_t *data,
                                                   uint8_t length,
                                                   uint8_t finalBlock)
{
    uint8_t index = 0U;
    uint16_t halfWord;

    if ((data == NULL) || (length == 0U))
    {
        return 0U;
    }

    /*
     * STM32F103 Flash is programmed by half-word in this project.
     * UDS TransferData may carry an odd number of bytes, so one byte
     * can be kept until the following block arrives.
     */
    if (udsFlashPendingByteValid != 0U)
    {
        halfWord = (uint16_t)udsFlashPendingByte |
                   ((uint16_t)data[0] << 8U);

        if (Bootloader_ProgramHalfWord(udsFlashWriteAddress,
                                      halfWord) == 0U)
        {
            return 0U;
        }

        udsFlashWriteAddress += 2U;
        udsFlashPendingByteValid = 0U;
        index = 1U;
    }

    while ((uint8_t)(index + 1U) < length)
    {
        halfWord = (uint16_t)data[index] |
                   ((uint16_t)data[index + 1U] << 8U);

        if (Bootloader_ProgramHalfWord(udsFlashWriteAddress,
                                      halfWord) == 0U)
        {
            return 0U;
        }

        udsFlashWriteAddress += 2U;
        index += 2U;
    }

    if (index < length)
    {
        if (finalBlock != 0U)
        {
            halfWord = (uint16_t)data[index] | 0xFF00U;

            if (Bootloader_ProgramHalfWord(udsFlashWriteAddress,
                                          halfWord) == 0U)
            {
                return 0U;
            }

            udsFlashWriteAddress += 2U;
        }
        else
        {
            udsFlashPendingByte = data[index];
            udsFlashPendingByteValid = 1U;
        }
    }

    if ((finalBlock != 0U) &&
        (udsFlashPendingByteValid != 0U))
    {
        halfWord = (uint16_t)udsFlashPendingByte | 0xFF00U;

        if (Bootloader_ProgramHalfWord(udsFlashWriteAddress,
                                      halfWord) == 0U)
        {
            return 0U;
        }

        udsFlashWriteAddress += 2U;
        udsFlashPendingByteValid = 0U;
    }

    return 1U;
}

static void Bootloader_UDS_ProcessFrame(void)
{
    uint8_t udsResponse[8];
    uint8_t udsResponseDlc = 0U;
    uint8_t responseOk = 1U;

    uint8_t transferData[5];
    uint8_t transferLength = 0U;
    uint8_t finalBlock = 0U;
    uint32_t transferAddress = 0U;
    uint8_t flashOk;

    if (UDS_ProcessSingleFrame(canRxData,
                               (uint8_t)canRxHeader.DLC,
                               udsResponse,
                               &udsResponseDlc) == 0U)
    {
        return;
    }

    if (UDS_GetSession() == UDS_SESSION_PROGRAMMING)
    {
        bootMode = BOOT_MODE_UPDATE;
        bootTransport = BOOT_TRANSPORT_CAN;
    }

    /*
     * A valid RequestDownload is accepted by the UDS layer first,
     * then the Bootloader prepares Flash before sending 0x74.
     */
    if (UDS_IsDownloadPreparationPending() != 0U)
    {
        flashOk = Bootloader_EraseFirmwareArea();

        if (flashOk != 0U)
        {
            udsFlashWriteAddress = UDS_GetDownloadAddress();
            udsFlashPendingByte = 0U;
            udsFlashPendingByteValid = 0U;
        }

        UDS_CompleteDownloadPreparation(flashOk,
                                        udsResponse,
                                        &udsResponseDlc);
    }

    /*
     * TransferData is validated by uds.c.
     * Only a validated block reaches Flash.
     */
    if (UDS_IsTransferDataPending() != 0U)
    {
        if (UDS_GetPendingTransferData(&transferAddress,
                                       transferData,
                                       &transferLength,
                                       &finalBlock) == 0U)
        {
            UDS_CompleteTransferData(0U,
                                     udsResponse,
                                     &udsResponseDlc);
        }
        else
        {
            /*
             * transferAddress is the logical address calculated by UDS.
             * The Flash stream helper handles a possible pending odd byte.
             */
            (void)transferAddress;

            flashOk = Bootloader_UDS_ProgramTransferBlock(
                transferData,
                transferLength,
                finalBlock
            );

            UDS_CompleteTransferData(flashOk,
                                     udsResponse,
                                     &udsResponseDlc);
        }
    }

    if (udsResponseDlc > 0U)
    {
        responseOk = Bootloader_UDS_SendFrame(udsResponse,
                                              udsResponseDlc);
    }

    if ((responseOk == 1U) &&
        (UDS_IsResetRequested() != 0U))
    {
        UDS_ClearResetRequest();

        if (PROTEUS_SIMULATION != 0U)
        {
            HAL_Delay(1000U);
        }
        else
        {
            HAL_Delay(10U);
        }

        NVIC_SystemReset();
    }
}
static uint8_t Bootloader_CAN_SimReceiveFrame(void)
{
    uint8_t frameHeader[3];
    uint16_t stdId;
    uint32_t i;
    /*
     * Proteus transport format:
     * C5 | ID_L | ID_H | DLC | DATA...
     *
     * The C5 marker is consumed by the caller before this function.
     */
    if (HAL_UART_Receive(&huart1,
                         frameHeader,
                         3U,
                         HAL_MAX_DELAY) != HAL_OK)
    {
        return 0U;
    }
    stdId = (uint16_t)frameHeader[0] |
            ((uint16_t)frameHeader[1] << 8U);
    if ((stdId > 0x7FFU) || (frameHeader[2] > 8U))
    {
        return 0U;
    }
    for (i = 0U; i < 8U; i++)
    {
        canRxData[i] = 0U;
    }
    if (frameHeader[2] > 0U)
    {
        if (HAL_UART_Receive(&huart1,
                             canRxData,
                             frameHeader[2],
                             HAL_MAX_DELAY) != HAL_OK)
        {
            return 0U;
        }
    }
    canRxHeader.StdId = stdId;
    canRxHeader.ExtId = 0U;
    canRxHeader.IDE = CAN_ID_STD;
    canRxHeader.RTR = CAN_RTR_DATA;
    canRxHeader.DLC = frameHeader[2];
    if (stdId == CAN_ID_COMMAND)
    {
        Bootloader_CAN_ProcessCommand();
    }
    else if (stdId == CAN_ID_DATA)
    {
        Bootloader_CAN_ProcessData();
    }
    else if (stdId == CAN_ID_HEADER)
    {
        Bootloader_CAN_ProcessHeader();
    }
    else if (stdId == CAN_ID_UDS_REQUEST)
    {
        Bootloader_UDS_ProcessFrame();
    }
    else
    {
        return 0U;
    }
    return 1U;
}
static uint8_t Bootloader_CAN_SendResponse(uint8_t response,
                                           uint8_t context,
                                           uint16_t sequence,
                                           uint8_t errorCode)
{
    uint32_t startTick;
    uint32_t i;
    uint8_t simFrame[4U + CAN_RESPONSE_DLC];
    canTxHeader.StdId = CAN_ID_RESPONSE;
    canTxHeader.ExtId = 0U;
    canTxHeader.IDE = CAN_ID_STD;
    canTxHeader.RTR = CAN_RTR_DATA;
    canTxHeader.DLC = CAN_RESPONSE_DLC;
    canTxHeader.TransmitGlobalTime = DISABLE;
    canTxData[0] = response;
    canTxData[1] = context;
    canTxData[2] = (uint8_t)(sequence & 0xFFU);
    canTxData[3] = (uint8_t)((sequence >> 8U) & 0xFFU);
    canTxData[4] = errorCode;
    if (PROTEUS_SIMULATION != 0U)
    {
        simFrame[0] = CAN_SIM_FRAME_START;
        simFrame[1] = (uint8_t)(CAN_ID_RESPONSE & 0xFFU);
        simFrame[2] = (uint8_t)((CAN_ID_RESPONSE >> 8U) & 0xFFU);
        simFrame[3] = CAN_RESPONSE_DLC;
        for (i = 0U; i < CAN_RESPONSE_DLC; i++)
        {
            simFrame[4U + i] = canTxData[i];
        }
        if (HAL_UART_Transmit(&huart1,
                              simFrame,
                              sizeof(simFrame),
                              HAL_MAX_DELAY) != HAL_OK)
        {
            return 0U;
        }
        return 1U;
    }
    startTick = HAL_GetTick();
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0U)
    {
        if ((HAL_GetTick() - startTick) >= CAN_TX_TIMEOUT)
        {
            return 0U;
        }
    }
    if (HAL_CAN_AddTxMessage(&hcan,
                             &canTxHeader,
                             canTxData,
                             &canTxMailbox) != HAL_OK)
    {
        return 0U;
    }
    return 1U;
}
static uint8_t Bootloader_CAN_ValidateHeaderBuffer(uint8_t *errorCode)
{
    FirmwareHeader_t header;
    uint32_t maxFirmwareSize;
    uint32_t calculatedCRC;
    header.magic = Bootloader_ReadUint32LE(&headerBuffer[0]);
    header.firmware_size = Bootloader_ReadUint32LE(&headerBuffer[4]);
    header.crc32 = Bootloader_ReadUint32LE(&headerBuffer[8]);
    header.version = Bootloader_ReadUint32LE(&headerBuffer[12]);
    maxFirmwareSize = APP_END_ADDRESS - APP_START_ADDRESS;
    if (header.magic != FW_MAGIC_NUMBER)
    {
        *errorCode = CAN_ERROR_HEADER;
        return 0U;
    }
    if ((header.firmware_size == 0U) ||
        (header.firmware_size > maxFirmwareSize) ||
        (header.firmware_size != canFirmwareSize) ||
        (header.firmware_size != canFirmwareBytesReceived))
    {
        *errorCode = CAN_ERROR_RANGE;
        return 0U;
    }
    if (Bootloader_IsApplicationValid() == 0U)
    {
        *errorCode = CAN_ERROR_HEADER;
        return 0U;
    }
    calculatedCRC = Bootloader_CalculateCRC32(APP_START_ADDRESS,
                                               header.firmware_size);
    if (calculatedCRC != header.crc32)
    {
        *errorCode = CAN_ERROR_CRC;
        return 0U;
    }
    *errorCode = CAN_ERROR_NONE;
    return 1U;
}
static void Bootloader_CAN_ProcessCommand(void)
{
    uint8_t command;
    uint32_t firmwareSize;
    uint32_t maxFirmwareSize;
    uint32_t startTick;
    if (canRxHeader.DLC < 1U)
        return;
    command = canRxData[0];
    switch (command)
    {
        case CMD_PING:
        {
            Bootloader_CAN_SendResponse(BL_ACK,
                                        CMD_PING,
                                        0U,
                                        CAN_ERROR_NONE);
            break;
        }
        case CMD_START_UPDATE:
        {
            if (canRxHeader.DLC != 5U)
            {
                Bootloader_CAN_SendResponse(BL_NACK,
                                            CMD_START_UPDATE,
                                            0U,
                                            CAN_ERROR_LENGTH);
                break;
            }
            firmwareSize = Bootloader_ReadUint32LE(&canRxData[1]);
            maxFirmwareSize = APP_END_ADDRESS - APP_START_ADDRESS;
            if ((firmwareSize == 0U) || (firmwareSize > maxFirmwareSize))
            {
                Bootloader_CAN_SendResponse(BL_NACK,
                                            CMD_START_UPDATE,
                                            0U,
                                            CAN_ERROR_RANGE);
                break;
            }
            bootMode = BOOT_MODE_UPDATE;
            bootTransport = BOOT_TRANSPORT_CAN;
            canFirmwareSize = firmwareSize;
            Bootloader_CAN_ResetUpdateState();
            Bootloader_CAN_SendResponse(BL_ACK,
                                        CMD_START_UPDATE,
                                        0U,
                                        CAN_ERROR_NONE);
            break;
        }
        case CMD_ERASE:
        {
            if ((bootMode != BOOT_MODE_UPDATE) ||
                (bootTransport != BOOT_TRANSPORT_CAN) ||
                (canFirmwareSize == 0U))
            {
                Bootloader_CAN_SendResponse(BL_NACK,
                                            CMD_ERASE,
                                            0U,
                                            CAN_ERROR_STATE);
                break;
            }
            if (Bootloader_EraseFirmwareArea() == 0U)
            {
                Bootloader_CAN_SendResponse(BL_NACK,
                                            CMD_ERASE,
                                            0U,
                                            CAN_ERROR_FLASH);
                break;
            }
            Bootloader_CAN_ResetUpdateState();
            canEraseDone = 1U;
            Bootloader_CAN_SendResponse(BL_ACK,
                                        CMD_ERASE,
                                        0U,
                                        CAN_ERROR_NONE);
            break;
        }
        case CMD_VERIFY:
        {
            if ((bootMode != BOOT_MODE_UPDATE) ||
                (bootTransport != BOOT_TRANSPORT_CAN) ||
                (canHeaderWritten == 0U))
            {
                Bootloader_CAN_SendResponse(BL_NACK,
                                            CMD_VERIFY,
                                            0U,
                                            CAN_ERROR_STATE);
                break;
            }
            if ((Bootloader_IsHeaderValid() == 1U) &&
                (Bootloader_IsApplicationValid() == 1U) &&
                (Bootloader_IsFirmwareIntegrityValid() == 1U))
            {
                canVerified = 1U;
                Bootloader_CAN_SendResponse(BL_ACK,
                                            CMD_VERIFY,
                                            0U,
                                            CAN_ERROR_NONE);
            }
            else
            {
                canVerified = 0U;
                Bootloader_CAN_SendResponse(BL_NACK,
                                            CMD_VERIFY,
                                            0U,
                                            CAN_ERROR_CRC);
            }
            break;
        }
        case CMD_END_UPDATE:
        {
            if ((bootMode != BOOT_MODE_UPDATE) ||
                (bootTransport != BOOT_TRANSPORT_CAN) ||
                (canVerified == 0U) ||
                (Bootloader_IsHeaderValid() == 0U) ||
                (Bootloader_IsApplicationValid() == 0U) ||
                (Bootloader_IsFirmwareIntegrityValid() == 0U))
            {
                Bootloader_CAN_SendResponse(BL_NACK,
                                            CMD_END_UPDATE,
                                            0U,
                                            CAN_ERROR_STATE);
                break;
            }
            if (Bootloader_CAN_SendResponse(BL_ACK,
                                            CMD_END_UPDATE,
                                            0U,
                                            CAN_ERROR_NONE) == 1U)
            {
                if (PROTEUS_SIMULATION == 0U)
                {
                    startTick = HAL_GetTick();
                    while (HAL_CAN_IsTxMessagePending(&hcan,
                                                      canTxMailbox) != 0U)
                    {
                        if ((HAL_GetTick() - startTick) >= CAN_TX_TIMEOUT)
                        {
                            break;
                        }
                    }
                }
            }
            if (PROTEUS_SIMULATION != 0U)
            {
                HAL_Delay(1000U);
            }
            else
            {
                HAL_Delay(10U);
            }
            NVIC_SystemReset();
            break;
        }
        default:
        {
            Bootloader_CAN_SendResponse(BL_NACK,
                                        command,
                                        0U,
                                        CAN_ERROR_STATE);
            break;
        }
    }
}
static void Bootloader_CAN_ProcessData(void)
{
    uint16_t sequence;
    uint16_t previousSequence;
    uint8_t payloadLength;
    uint8_t expectedLength;
    uint8_t programLength;
    uint8_t programBuffer[CAN_DATA_PAYLOAD_SIZE + 1U];
    uint32_t remaining;
    uint32_t i;
    if ((bootMode != BOOT_MODE_UPDATE) ||
        (bootTransport != BOOT_TRANSPORT_CAN) ||
        (canEraseDone == 0U))
    {
        Bootloader_CAN_SendResponse(BL_NACK,
                                    CMD_DATA,
                                    0U,
                                    CAN_ERROR_STATE);
        return;
    }
    if ((canRxHeader.DLC < 3U) || (canRxHeader.DLC > 8U))
    {
        Bootloader_CAN_SendResponse(BL_NACK,
                                    CMD_DATA,
                                    0U,
                                    CAN_ERROR_LENGTH);
        return;
    }
    sequence = (uint16_t)canRxData[0] |
               ((uint16_t)canRxData[1] << 8U);
    if (canExpectedDataSequence > 0U)
    {
        previousSequence = (uint16_t)(canExpectedDataSequence - 1U);
        if (sequence == previousSequence)
        {
            Bootloader_CAN_SendResponse(BL_ACK,
                                        CMD_DATA,
                                        sequence,
                                        CAN_ERROR_NONE);
            return;
        }
    }
    if (sequence != canExpectedDataSequence)
    {
        Bootloader_CAN_SendResponse(BL_NACK,
                                    CMD_DATA,
                                    sequence,
                                    CAN_ERROR_SEQUENCE);
        return;
    }
    if (canFirmwareBytesReceived >= canFirmwareSize)
    {
        Bootloader_CAN_SendResponse(BL_NACK,
                                    CMD_DATA,
                                    sequence,
                                    CAN_ERROR_RANGE);
        return;
    }
    remaining = canFirmwareSize - canFirmwareBytesReceived;
    if (remaining > CAN_DATA_PAYLOAD_SIZE)
        expectedLength = CAN_DATA_PAYLOAD_SIZE;
    else
        expectedLength = (uint8_t)remaining;
    payloadLength = (uint8_t)(canRxHeader.DLC - CAN_SEQUENCE_SIZE);
    if (payloadLength != expectedLength)
    {
        Bootloader_CAN_SendResponse(BL_NACK,
                                    CMD_DATA,
                                    sequence,
                                    CAN_ERROR_LENGTH);
        return;
    }
    for (i = 0U; i < payloadLength; i++)
        programBuffer[i] = canRxData[i + CAN_SEQUENCE_SIZE];
    programLength = payloadLength;
    if ((programLength & 0x1U) != 0U)
    {
        programBuffer[programLength] = 0xFFU;
        programLength++;
    }
    if ((currentWriteAddress + programLength) > APP_END_ADDRESS)
    {
        Bootloader_CAN_SendResponse(BL_NACK,
                                    CMD_DATA,
                                    sequence,
                                    CAN_ERROR_RANGE);
        return;
    }
    if (Bootloader_ProgramBuffer(currentWriteAddress,
                                 programBuffer,
                                 programLength) == 0U)
    {
        Bootloader_CAN_SendResponse(BL_NACK,
                                    CMD_DATA,
                                    sequence,
                                    CAN_ERROR_FLASH);
        return;
    }
    currentWriteAddress += programLength;
    canFirmwareBytesReceived += payloadLength;
    canExpectedDataSequence++;
    Bootloader_CAN_SendResponse(BL_ACK,
                                CMD_DATA,
                                sequence,
                                CAN_ERROR_NONE);
}
static void Bootloader_CAN_ProcessHeader(void)
{
    uint16_t sequence;
    uint16_t previousSequence;
    uint8_t payloadLength;
    uint8_t expectedLength;
    uint8_t errorCode;
    uint32_t remaining;
    uint32_t i;
    if ((bootMode != BOOT_MODE_UPDATE) ||
        (bootTransport != BOOT_TRANSPORT_CAN) ||
        (canEraseDone == 0U) ||
        (canFirmwareBytesReceived != canFirmwareSize))
    {
        Bootloader_CAN_SendResponse(BL_NACK,
                                    CMD_HEADER,
                                    0U,
                                    CAN_ERROR_STATE);
        return;
    }
    if ((canRxHeader.DLC < 3U) || (canRxHeader.DLC > 8U))
    {
        Bootloader_CAN_SendResponse(BL_NACK,
                                    CMD_HEADER,
                                    0U,
                                    CAN_ERROR_LENGTH);
        return;
    }
    sequence = (uint16_t)canRxData[0] |
               ((uint16_t)canRxData[1] << 8U);
    if (canExpectedHeaderSequence > 0U)
    {
        previousSequence = (uint16_t)(canExpectedHeaderSequence - 1U);
        if (sequence == previousSequence)
        {
            Bootloader_CAN_SendResponse(BL_ACK,
                                        CMD_HEADER,
                                        sequence,
                                        CAN_ERROR_NONE);
            return;
        }
    }
    if (sequence != canExpectedHeaderSequence)
    {
        Bootloader_CAN_SendResponse(BL_NACK,
                                    CMD_HEADER,
                                    sequence,
                                    CAN_ERROR_SEQUENCE);
        return;
    }
    if (canHeaderBytesReceived >= HEADER_DATA_SIZE)
    {
        Bootloader_CAN_SendResponse(BL_NACK,
                                    CMD_HEADER,
                                    sequence,
                                    CAN_ERROR_RANGE);
        return;
    }
    remaining = HEADER_DATA_SIZE - canHeaderBytesReceived;
    if (remaining > CAN_DATA_PAYLOAD_SIZE)
        expectedLength = CAN_DATA_PAYLOAD_SIZE;
    else
        expectedLength = (uint8_t)remaining;
    payloadLength = (uint8_t)(canRxHeader.DLC - CAN_SEQUENCE_SIZE);
    if (payloadLength != expectedLength)
    {
        Bootloader_CAN_SendResponse(BL_NACK,
                                    CMD_HEADER,
                                    sequence,
                                    CAN_ERROR_LENGTH);
        return;
    }
    for (i = 0U; i < payloadLength; i++)
    {
        headerBuffer[canHeaderBytesReceived + i] =
            canRxData[i + CAN_SEQUENCE_SIZE];
    }
    canHeaderBytesReceived += payloadLength;
    if (canHeaderBytesReceived == HEADER_DATA_SIZE)
    {
        if (Bootloader_CAN_ValidateHeaderBuffer(&errorCode) == 0U)
        {
            canHeaderBytesReceived = 0U;
            canExpectedHeaderSequence = 0U;
            canHeaderWritten = 0U;
            canVerified = 0U;
            Bootloader_CAN_SendResponse(BL_NACK,
                                        CMD_HEADER,
                                        sequence,
                                        errorCode);
            return;
        }
        if (Bootloader_ProgramBuffer(APP_HEADER_ADDRESS,
                                     headerBuffer,
                                     HEADER_DATA_SIZE) == 0U)
        {
            canHeaderBytesReceived = 0U;
            canExpectedHeaderSequence = 0U;
            canHeaderWritten = 0U;
            Bootloader_CAN_SendResponse(BL_NACK,
                                        CMD_HEADER,
                                        sequence,
                                        CAN_ERROR_FLASH);
            return;
        }
        canHeaderWritten = 1U;
    }
    canExpectedHeaderSequence++;
    Bootloader_CAN_SendResponse(BL_ACK,
                                CMD_HEADER,
                                sequence,
                                CAN_ERROR_NONE);
}
static void Bootloader_CAN_Process(void)
{
    if (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) == 0U)
        return;
    if (HAL_CAN_GetRxMessage(&hcan,
                             CAN_RX_FIFO0,
                             &canRxHeader,
                             canRxData) != HAL_OK)
    {
        return;
    }
    if ((canRxHeader.IDE != CAN_ID_STD) ||
        (canRxHeader.RTR != CAN_RTR_DATA))
    {
        return;
    }
    if (canRxHeader.StdId == CAN_ID_COMMAND)
    {
        Bootloader_CAN_ProcessCommand();
    }
    else if (canRxHeader.StdId == CAN_ID_DATA)
    {
        Bootloader_CAN_ProcessData();
    }
    else if (canRxHeader.StdId == CAN_ID_HEADER)
    {
        Bootloader_CAN_ProcessHeader();
    }
    else if (canRxHeader.StdId == CAN_ID_UDS_REQUEST)
    {
        Bootloader_UDS_ProcessFrame();
    }
}
static void Bootloader_WaitForUpdateRequest(void)
{
    uint32_t startTick;
    startTick = HAL_GetTick();
    while ((HAL_GetTick() - startTick) < UART_RX_TIMEOUT)
    {
        if (PROTEUS_SIMULATION == 0U)
        {
            Bootloader_CAN_Process();
            if (bootMode == BOOT_MODE_UPDATE)
            {
                break;
            }
        }
        if (HAL_UART_Receive(&huart1,
                             &uartRxByte,
                             1U,
                             1U) == HAL_OK)
        {
            if ((PROTEUS_SIMULATION != 0U) &&
                (uartRxByte == CAN_SIM_FRAME_START))
            {
                (void)Bootloader_CAN_SimReceiveFrame();
            }
            else
            {
                Bootloader_ProcessCommand(uartRxByte);
            }
            if (bootMode == BOOT_MODE_UPDATE)
            {
                break;
            }
        }
    }
}
static uint8_t Bootloader_ProgramBuffer(uint32_t address,
                                        const uint8_t *data,
                                        uint32_t length)
{
    uint32_t i;
    uint16_t halfWord;
    if ((data == NULL) || (length == 0U))
        return 0U;
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
            bootTransport = BOOT_TRANSPORT_UART;
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
  /* MCU Configuration--------------------------------------------------------*/
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* USER CODE BEGIN Init */
  /* USER CODE END Init */
  /* Configure the system clock */
  SystemClock_Config();
  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  if (PROTEUS_SIMULATION == 0U)
  {
      MX_CAN_Init();
  }
  /* USER CODE BEGIN 2 */
  UDS_Init();
  /*
   * Proteus 9.0 SP2 does not correctly model bxCAN for this MCU model.
   * Keep CAN disabled only for simulation. Set PROTEUS_SIMULATION to 0
   * when the bootloader runs on a real STM32F103.
   */
  if (PROTEUS_SIMULATION == 0U)
  {
      if (Bootloader_CAN_Init() == 0U)
      {
          Error_Handler();
      }
  }
  Bootloader_WaitForUpdateRequest();
  if (bootMode == BOOT_MODE_UPDATE)
  {
      if (bootTransport == BOOT_TRANSPORT_UART)
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
      else if (bootTransport == BOOT_TRANSPORT_CAN)
      {
          while (1)
          {
              if (PROTEUS_SIMULATION != 0U)
              {
                  if (HAL_UART_Receive(&huart1,
                                       &uartRxByte,
                                       1U,
                                       HAL_MAX_DELAY) == HAL_OK)
                  {
                      if (uartRxByte == CAN_SIM_FRAME_START)
                      {
                          (void)Bootloader_CAN_SimReceiveFrame();
                      }
                  }
              }
              else
              {
                  Bootloader_CAN_Process();
              }
          }
      }
  }
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
/**
  * @brief CAN Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN_Init(void)
{
  /* USER CODE BEGIN CAN_Init 0 */
  /* USER CODE END CAN_Init 0 */
  /* USER CODE BEGIN CAN_Init 1 */
  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 1;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = ENABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */
  /* USER CODE END CAN_Init 2 */
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
  /* GPIO Ports Clock Enable */
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
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
