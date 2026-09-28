#include "uds.h"

#define UDS_SECURITY_SEED_VALUE              0x12345678U
#define UDS_SECURITY_KEY_MASK                0xA5A5A5A5U

#define UDS_APP_START_ADDRESS                0x08004400U
#define UDS_APP_END_ADDRESS                  0x08010000U

#define UDS_DOWNLOAD_ALFID                   0x22U
#define UDS_DOWNLOAD_DFI                     0x00U
#define UDS_MAX_TRANSFER_BLOCK_LENGTH        0x07U
#define UDS_TRANSFER_DATA_MAX_PAYLOAD        5U

static uint8_t udsCurrentSession = UDS_SESSION_DEFAULT;
static uint8_t udsResetRequested = 0U;

static uint8_t udsSecurityUnlocked = 0U;
static uint8_t udsSeedRequested = 0U;
static uint32_t udsCurrentSeed = UDS_SECURITY_SEED_VALUE;

static uint8_t udsDownloadRequested = 0U;
static uint8_t udsDownloadPreparationPending = 0U;
static uint8_t udsDownloadReady = 0U;
static uint8_t udsTransferExited = 0U;
static uint32_t udsDownloadAddress = 0U;
static uint32_t udsDownloadSize = 0U;
static uint32_t udsTransferredBytes = 0U;

static uint8_t udsExpectedBlockSequenceCounter = 1U;
static uint8_t udsLastBlockSequenceCounter = 0U;
static uint8_t udsLastBlockValid = 0U;

static uint8_t udsTransferDataPending = 0U;
static uint8_t udsPendingBlockSequenceCounter = 0U;
static uint8_t udsPendingTransferLength = 0U;
static uint8_t udsPendingTransferData[UDS_TRANSFER_DATA_MAX_PAYLOAD];

static void UDS_BuildNegativeResponse(uint8_t requestSid,
                                      uint8_t nrc,
                                      uint8_t *response,
                                      uint8_t *responseDlc)
{
    response[0] = 0x03U;
    response[1] = 0x7FU;
    response[2] = requestSid;
    response[3] = nrc;

    *responseDlc = 4U;
}

static uint16_t UDS_ReadUint16BE(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) |
                      ((uint16_t)data[1]));
}

static uint32_t UDS_ReadUint32BE(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24U) |
           ((uint32_t)data[1] << 16U) |
           ((uint32_t)data[2] << 8U)  |
           ((uint32_t)data[3]);
}

static void UDS_WriteUint32BE(uint32_t value,
                              uint8_t *data)
{
    data[0] = (uint8_t)((value >> 24U) & 0xFFU);
    data[1] = (uint8_t)((value >> 16U) & 0xFFU);
    data[2] = (uint8_t)((value >> 8U) & 0xFFU);
    data[3] = (uint8_t)(value & 0xFFU);
}

static uint32_t UDS_CalculateSecurityKey(uint32_t seed)
{
    /*
     * Educational algorithm only.
     * A production ECU must use a proper OEM security design.
     */
    return seed ^ UDS_SECURITY_KEY_MASK;
}

static void UDS_ResetTransferState(void)
{
    uint32_t i;

    udsTransferredBytes = 0U;
    udsExpectedBlockSequenceCounter = 1U;
    udsLastBlockSequenceCounter = 0U;
    udsLastBlockValid = 0U;

    udsTransferDataPending = 0U;
    udsPendingBlockSequenceCounter = 0U;
    udsPendingTransferLength = 0U;

    for (i = 0U; i < UDS_TRANSFER_DATA_MAX_PAYLOAD; i++)
    {
        udsPendingTransferData[i] = 0U;
    }
}

static void UDS_ResetDownloadState(void)
{
    udsDownloadRequested = 0U;
    udsDownloadPreparationPending = 0U;
    udsDownloadReady = 0U;
    udsTransferExited = 0U;
    udsDownloadAddress = 0U;
    udsDownloadSize = 0U;

    UDS_ResetTransferState();
}

static void UDS_ProcessDiagnosticSessionControl(const uint8_t *request,
                                                uint8_t singleFrameLength,
                                                uint8_t *response,
                                                uint8_t *responseDlc)
{
    uint8_t subFunction;
    uint8_t suppressPositiveResponse;

    if (singleFrameLength != 2U)
    {
        UDS_BuildNegativeResponse(
            UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
            UDS_NRC_INCORRECT_MESSAGE_LENGTH,
            response,
            responseDlc
        );
        return;
    }

    suppressPositiveResponse = request[2] & 0x80U;
    subFunction = request[2] & 0x7FU;

    if ((subFunction != UDS_SESSION_DEFAULT) &&
        (subFunction != UDS_SESSION_PROGRAMMING) &&
        (subFunction != UDS_SESSION_EXTENDED))
    {
        UDS_BuildNegativeResponse(
            UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
            UDS_NRC_SUBFUNCTION_NOT_SUPPORTED,
            response,
            responseDlc
        );
        return;
    }

    udsCurrentSession = subFunction;

    if (subFunction == UDS_SESSION_DEFAULT)
    {
        udsSecurityUnlocked = 0U;
        udsSeedRequested = 0U;
    }

    if (subFunction != UDS_SESSION_PROGRAMMING)
    {
        UDS_ResetDownloadState();
    }

    if (suppressPositiveResponse != 0U)
    {
        *responseDlc = 0U;
        return;
    }

    response[0] = 0x06U;
    response[1] = UDS_SID_DIAGNOSTIC_SESSION_CONTROL +
                  UDS_POSITIVE_RESPONSE_OFFSET;
    response[2] = subFunction;
    response[3] = 0x00U;
    response[4] = 0x32U;
    response[5] = 0x01U;
    response[6] = 0xF4U;

    *responseDlc = 7U;
}

static void UDS_ProcessECUReset(const uint8_t *request,
                                uint8_t singleFrameLength,
                                uint8_t *response,
                                uint8_t *responseDlc)
{
    uint8_t subFunction;
    uint8_t suppressPositiveResponse;

    if (singleFrameLength != 2U)
    {
        UDS_BuildNegativeResponse(
            UDS_SID_ECU_RESET,
            UDS_NRC_INCORRECT_MESSAGE_LENGTH,
            response,
            responseDlc
        );
        return;
    }

    suppressPositiveResponse = request[2] & 0x80U;
    subFunction = request[2] & 0x7FU;

    if (subFunction != UDS_RESET_HARD)
    {
        UDS_BuildNegativeResponse(
            UDS_SID_ECU_RESET,
            UDS_NRC_SUBFUNCTION_NOT_SUPPORTED,
            response,
            responseDlc
        );
        return;
    }

    udsResetRequested = 1U;

    if (suppressPositiveResponse != 0U)
    {
        *responseDlc = 0U;
        return;
    }

    response[0] = 0x02U;
    response[1] = UDS_SID_ECU_RESET +
                  UDS_POSITIVE_RESPONSE_OFFSET;
    response[2] = UDS_RESET_HARD;

    *responseDlc = 3U;
}

static void UDS_ProcessSecurityAccess(const uint8_t *request,
                                      uint8_t singleFrameLength,
                                      uint8_t *response,
                                      uint8_t *responseDlc)
{
    uint8_t subFunction;
    uint32_t receivedKey;
    uint32_t expectedKey;

    if (singleFrameLength < 2U)
    {
        UDS_BuildNegativeResponse(
            UDS_SID_SECURITY_ACCESS,
            UDS_NRC_INCORRECT_MESSAGE_LENGTH,
            response,
            responseDlc
        );
        return;
    }

    subFunction = request[2] & 0x7FU;

    if (subFunction == UDS_SECURITY_REQUEST_SEED)
    {
        if (singleFrameLength != 2U)
        {
            UDS_BuildNegativeResponse(
                UDS_SID_SECURITY_ACCESS,
                UDS_NRC_INCORRECT_MESSAGE_LENGTH,
                response,
                responseDlc
            );
            return;
        }

        response[0] = 0x06U;
        response[1] = UDS_SID_SECURITY_ACCESS +
                      UDS_POSITIVE_RESPONSE_OFFSET;
        response[2] = UDS_SECURITY_REQUEST_SEED;

        if (udsSecurityUnlocked != 0U)
        {
            response[3] = 0x00U;
            response[4] = 0x00U;
            response[5] = 0x00U;
            response[6] = 0x00U;
        }
        else
        {
            udsCurrentSeed = UDS_SECURITY_SEED_VALUE;
            udsSeedRequested = 1U;

            UDS_WriteUint32BE(
                udsCurrentSeed,
                &response[3]
            );
        }

        *responseDlc = 7U;
        return;
    }

    if (subFunction == UDS_SECURITY_SEND_KEY)
    {
        if (singleFrameLength != 6U)
        {
            UDS_BuildNegativeResponse(
                UDS_SID_SECURITY_ACCESS,
                UDS_NRC_INCORRECT_MESSAGE_LENGTH,
                response,
                responseDlc
            );
            return;
        }

        if (udsSeedRequested == 0U)
        {
            UDS_BuildNegativeResponse(
                UDS_SID_SECURITY_ACCESS,
                UDS_NRC_REQUEST_SEQUENCE_ERROR,
                response,
                responseDlc
            );
            return;
        }

        receivedKey = UDS_ReadUint32BE(&request[3]);
        expectedKey = UDS_CalculateSecurityKey(udsCurrentSeed);

        if (receivedKey != expectedKey)
        {
            udsSeedRequested = 0U;

            UDS_BuildNegativeResponse(
                UDS_SID_SECURITY_ACCESS,
                UDS_NRC_INVALID_KEY,
                response,
                responseDlc
            );
            return;
        }

        udsSecurityUnlocked = 1U;
        udsSeedRequested = 0U;

        response[0] = 0x02U;
        response[1] = UDS_SID_SECURITY_ACCESS +
                      UDS_POSITIVE_RESPONSE_OFFSET;
        response[2] = UDS_SECURITY_SEND_KEY;

        *responseDlc = 3U;
        return;
    }

    UDS_BuildNegativeResponse(
        UDS_SID_SECURITY_ACCESS,
        UDS_NRC_SUBFUNCTION_NOT_SUPPORTED,
        response,
        responseDlc
    );
}

static void UDS_ProcessRequestDownload(const uint8_t *request,
                                       uint8_t singleFrameLength,
                                       uint8_t *response,
                                       uint8_t *responseDlc)
{
    uint8_t dataFormatIdentifier;
    uint8_t addressLengthFormatIdentifier;
    uint16_t memoryOffset;
    uint16_t memorySize16;
    uint32_t memoryAddress;
    uint32_t memorySize;
    uint32_t maxApplicationSize;

    if (udsCurrentSession != UDS_SESSION_PROGRAMMING)
    {
        UDS_BuildNegativeResponse(
            UDS_SID_REQUEST_DOWNLOAD,
            UDS_NRC_CONDITIONS_NOT_CORRECT,
            response,
            responseDlc
        );
        return;
    }

    if (udsSecurityUnlocked == 0U)
    {
        UDS_BuildNegativeResponse(
            UDS_SID_REQUEST_DOWNLOAD,
            UDS_NRC_SECURITY_ACCESS_DENIED,
            response,
            responseDlc
        );
        return;
    }

    if (singleFrameLength != 7U)
    {
        UDS_BuildNegativeResponse(
            UDS_SID_REQUEST_DOWNLOAD,
            UDS_NRC_INCORRECT_MESSAGE_LENGTH,
            response,
            responseDlc
        );
        return;
    }

    dataFormatIdentifier = request[2];
    addressLengthFormatIdentifier = request[3];

    if ((dataFormatIdentifier != UDS_DOWNLOAD_DFI) ||
        (addressLengthFormatIdentifier != UDS_DOWNLOAD_ALFID))
    {
        UDS_BuildNegativeResponse(
            UDS_SID_REQUEST_DOWNLOAD,
            UDS_NRC_REQUEST_OUT_OF_RANGE,
            response,
            responseDlc
        );
        return;
    }

    memoryOffset = UDS_ReadUint16BE(&request[4]);
    memorySize16 = UDS_ReadUint16BE(&request[6]);

    memoryAddress = UDS_APP_START_ADDRESS +
                    (uint32_t)memoryOffset;
    memorySize = (uint32_t)memorySize16;

    maxApplicationSize =
        UDS_APP_END_ADDRESS - UDS_APP_START_ADDRESS;

    /*
     * V6.5 performs a full Application-area erase.
     * For this stage, downloads therefore start at APP_START.
     */
    if ((memoryOffset != 0U) ||
        (memorySize == 0U) ||
        (memoryAddress != UDS_APP_START_ADDRESS) ||
        (memorySize > maxApplicationSize) ||
        ((memoryAddress + memorySize) > UDS_APP_END_ADDRESS))
    {
        UDS_BuildNegativeResponse(
            UDS_SID_REQUEST_DOWNLOAD,
            UDS_NRC_REQUEST_OUT_OF_RANGE,
            response,
            responseDlc
        );
        return;
    }

    UDS_ResetTransferState();

    udsDownloadRequested = 1U;
    udsDownloadPreparationPending = 1U;
    udsDownloadReady = 0U;
    udsTransferExited = 0U;
    udsDownloadAddress = memoryAddress;
    udsDownloadSize = memorySize;

    response[0] = 0x03U;
    response[1] = UDS_SID_REQUEST_DOWNLOAD +
                  UDS_POSITIVE_RESPONSE_OFFSET;
    response[2] = 0x10U;
    response[3] = UDS_MAX_TRANSFER_BLOCK_LENGTH;

    *responseDlc = 4U;
}

static void UDS_ProcessTransferData(const uint8_t *request,
                                    uint8_t singleFrameLength,
                                    uint8_t *response,
                                    uint8_t *responseDlc)
{
    uint8_t blockSequenceCounter;
    uint8_t dataLength;
    uint32_t remaining;
    uint32_t i;

    if ((udsCurrentSession != UDS_SESSION_PROGRAMMING) ||
        (udsSecurityUnlocked == 0U) ||
        (udsDownloadRequested == 0U) ||
        (udsDownloadReady == 0U) ||
        (udsTransferExited != 0U))
    {
        UDS_BuildNegativeResponse(
            UDS_SID_TRANSFER_DATA,
            UDS_NRC_REQUEST_SEQUENCE_ERROR,
            response,
            responseDlc
        );
        return;
    }

    /*
     * TransferData UDS payload:
     * 36 | BlockSequenceCounter | Data...
     *
     * At least one data byte is required.
     */
    if ((singleFrameLength < 3U) ||
        (singleFrameLength > UDS_MAX_TRANSFER_BLOCK_LENGTH))
    {
        UDS_BuildNegativeResponse(
            UDS_SID_TRANSFER_DATA,
            UDS_NRC_INCORRECT_MESSAGE_LENGTH,
            response,
            responseDlc
        );
        return;
    }

    blockSequenceCounter = request[2];
    dataLength = (uint8_t)(singleFrameLength - 2U);

    /*
     * A repeated previous block is acknowledged again without
     * programming Flash a second time.
     */
    if ((udsLastBlockValid != 0U) &&
        (blockSequenceCounter == udsLastBlockSequenceCounter))
    {
        response[0] = 0x02U;
        response[1] = UDS_SID_TRANSFER_DATA +
                      UDS_POSITIVE_RESPONSE_OFFSET;
        response[2] = blockSequenceCounter;
        *responseDlc = 3U;
        return;
    }

    if (blockSequenceCounter != udsExpectedBlockSequenceCounter)
    {
        UDS_BuildNegativeResponse(
            UDS_SID_TRANSFER_DATA,
            UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER,
            response,
            responseDlc
        );
        return;
    }

    if (udsTransferredBytes >= udsDownloadSize)
    {
        UDS_BuildNegativeResponse(
            UDS_SID_TRANSFER_DATA,
            UDS_NRC_REQUEST_SEQUENCE_ERROR,
            response,
            responseDlc
        );
        return;
    }

    remaining = udsDownloadSize - udsTransferredBytes;

    if ((uint32_t)dataLength > remaining)
    {
        UDS_BuildNegativeResponse(
            UDS_SID_TRANSFER_DATA,
            UDS_NRC_REQUEST_OUT_OF_RANGE,
            response,
            responseDlc
        );
        return;
    }

    for (i = 0U; i < dataLength; i++)
    {
        udsPendingTransferData[i] = request[3U + i];
    }

    udsPendingBlockSequenceCounter = blockSequenceCounter;
    udsPendingTransferLength = dataLength;
    udsTransferDataPending = 1U;

    /*
     * The positive response is created only after main.c confirms
     * successful Flash programming.
     */
    *responseDlc = 0U;
}


static void UDS_ProcessRequestTransferExit(uint8_t singleFrameLength,
                                           uint8_t *response,
                                           uint8_t *responseDlc)
{
    /*
     * V6.6 does not use transferRequestParameterRecord.
     * Therefore the request contains only SID 0x37.
     */
    if (singleFrameLength != 1U)
    {
        UDS_BuildNegativeResponse(
            UDS_SID_REQUEST_TRANSFER_EXIT,
            UDS_NRC_INCORRECT_MESSAGE_LENGTH,
            response,
            responseDlc
        );
        return;
    }

    if ((udsCurrentSession != UDS_SESSION_PROGRAMMING) ||
        (udsSecurityUnlocked == 0U) ||
        (udsDownloadRequested == 0U) ||
        (udsDownloadReady == 0U) ||
        (udsTransferExited != 0U) ||
        (udsTransferDataPending != 0U))
    {
        UDS_BuildNegativeResponse(
            UDS_SID_REQUEST_TRANSFER_EXIT,
            UDS_NRC_REQUEST_SEQUENCE_ERROR,
            response,
            responseDlc
        );
        return;
    }

    /*
     * TransferExit is accepted only when every byte announced
     * by RequestDownload has been successfully programmed.
     */
    if (udsTransferredBytes != udsDownloadSize)
    {
        UDS_BuildNegativeResponse(
            UDS_SID_REQUEST_TRANSFER_EXIT,
            UDS_NRC_REQUEST_SEQUENCE_ERROR,
            response,
            responseDlc
        );
        return;
    }

    udsTransferExited = 1U;

    /*
     * Positive response:
     * 01 77
     */
    response[0] = 0x01U;
    response[1] = UDS_SID_REQUEST_TRANSFER_EXIT +
                  UDS_POSITIVE_RESPONSE_OFFSET;

    *responseDlc = 2U;
}

void UDS_Init(void)
{
    udsCurrentSession = UDS_SESSION_DEFAULT;
    udsResetRequested = 0U;

    udsSecurityUnlocked = 0U;
    udsSeedRequested = 0U;
    udsCurrentSeed = UDS_SECURITY_SEED_VALUE;

    UDS_ResetDownloadState();
}

uint8_t UDS_GetSession(void)
{
    return udsCurrentSession;
}

uint8_t UDS_IsResetRequested(void)
{
    return udsResetRequested;
}

void UDS_ClearResetRequest(void)
{
    udsResetRequested = 0U;
}

uint8_t UDS_IsSecurityUnlocked(void)
{
    return udsSecurityUnlocked;
}

uint8_t UDS_IsDownloadRequested(void)
{
    return udsDownloadRequested;
}

uint8_t UDS_IsDownloadPreparationPending(void)
{
    return udsDownloadPreparationPending;
}

void UDS_CompleteDownloadPreparation(uint8_t success,
                                     uint8_t *response,
                                     uint8_t *responseDlc)
{
    if ((response == 0) || (responseDlc == 0))
    {
        return;
    }

    udsDownloadPreparationPending = 0U;

    if (success != 0U)
    {
        udsDownloadReady = 1U;
        return;
    }

    UDS_ResetDownloadState();

    UDS_BuildNegativeResponse(
        UDS_SID_REQUEST_DOWNLOAD,
        UDS_NRC_GENERAL_PROGRAMMING_FAILURE,
        response,
        responseDlc
    );
}

uint32_t UDS_GetDownloadAddress(void)
{
    return udsDownloadAddress;
}

uint32_t UDS_GetDownloadSize(void)
{
    return udsDownloadSize;
}

uint32_t UDS_GetTransferredBytes(void)
{
    return udsTransferredBytes;
}

uint8_t UDS_IsDownloadComplete(void)
{
    if ((udsDownloadRequested != 0U) &&
        (udsDownloadReady != 0U) &&
        (udsTransferredBytes == udsDownloadSize))
    {
        return 1U;
    }

    return 0U;
}

uint8_t UDS_IsTransferExited(void)
{
    return udsTransferExited;
}

uint8_t UDS_IsTransferDataPending(void)
{
    return udsTransferDataPending;
}

uint8_t UDS_GetPendingTransferData(uint32_t *address,
                                   uint8_t *data,
                                   uint8_t *length,
                                   uint8_t *finalBlock)
{
    uint32_t i;

    if ((address == 0) ||
        (data == 0) ||
        (length == 0) ||
        (finalBlock == 0) ||
        (udsTransferDataPending == 0U))
    {
        return 0U;
    }

    *address = udsDownloadAddress + udsTransferredBytes;
    *length = udsPendingTransferLength;

    if ((udsTransferredBytes +
         (uint32_t)udsPendingTransferLength) == udsDownloadSize)
    {
        *finalBlock = 1U;
    }
    else
    {
        *finalBlock = 0U;
    }

    for (i = 0U; i < udsPendingTransferLength; i++)
    {
        data[i] = udsPendingTransferData[i];
    }

    return 1U;
}

void UDS_CompleteTransferData(uint8_t success,
                              uint8_t *response,
                              uint8_t *responseDlc)
{
    uint8_t blockSequenceCounter;

    if ((response == 0) ||
        (responseDlc == 0) ||
        (udsTransferDataPending == 0U))
    {
        return;
    }

    blockSequenceCounter = udsPendingBlockSequenceCounter;

    if (success == 0U)
    {
        udsTransferDataPending = 0U;
        udsDownloadReady = 0U;

        UDS_BuildNegativeResponse(
            UDS_SID_TRANSFER_DATA,
            UDS_NRC_GENERAL_PROGRAMMING_FAILURE,
            response,
            responseDlc
        );
        return;
    }

    udsTransferredBytes +=
        (uint32_t)udsPendingTransferLength;

    udsLastBlockSequenceCounter =
        udsPendingBlockSequenceCounter;
    udsLastBlockValid = 1U;

    udsExpectedBlockSequenceCounter++;

    udsTransferDataPending = 0U;
    udsPendingTransferLength = 0U;

    response[0] = 0x02U;
    response[1] = UDS_SID_TRANSFER_DATA +
                  UDS_POSITIVE_RESPONSE_OFFSET;
    response[2] = blockSequenceCounter;

    *responseDlc = 3U;
}

uint8_t UDS_ProcessSingleFrame(const uint8_t *request,
                               uint8_t requestDlc,
                               uint8_t *response,
                               uint8_t *responseDlc)
{
    uint8_t singleFrameLength;
    uint8_t serviceId;

    if ((request == 0) ||
        (response == 0) ||
        (responseDlc == 0) ||
        (requestDlc < 2U) ||
        (requestDlc > 8U))
    {
        return 0U;
    }

    *responseDlc = 0U;

    if ((request[0] & 0xF0U) != 0x00U)
    {
        return 0U;
    }

    singleFrameLength = request[0] & 0x0FU;
    serviceId = request[1];

    if ((singleFrameLength == 0U) ||
        (singleFrameLength > 7U) ||
        ((uint8_t)(singleFrameLength + 1U) > requestDlc))
    {
        UDS_BuildNegativeResponse(
            serviceId,
            UDS_NRC_INCORRECT_MESSAGE_LENGTH,
            response,
            responseDlc
        );
        return 1U;
    }

    switch (serviceId)
    {
        case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:
        {
            UDS_ProcessDiagnosticSessionControl(
                request,
                singleFrameLength,
                response,
                responseDlc
            );
            break;
        }

        case UDS_SID_ECU_RESET:
        {
            UDS_ProcessECUReset(
                request,
                singleFrameLength,
                response,
                responseDlc
            );
            break;
        }

        case UDS_SID_SECURITY_ACCESS:
        {
            UDS_ProcessSecurityAccess(
                request,
                singleFrameLength,
                response,
                responseDlc
            );
            break;
        }

        case UDS_SID_REQUEST_DOWNLOAD:
        {
            UDS_ProcessRequestDownload(
                request,
                singleFrameLength,
                response,
                responseDlc
            );
            break;
        }

        case UDS_SID_TRANSFER_DATA:
        {
            UDS_ProcessTransferData(
                request,
                singleFrameLength,
                response,
                responseDlc
            );
            break;
        }

        case UDS_SID_REQUEST_TRANSFER_EXIT:
        {
            UDS_ProcessRequestTransferExit(
                singleFrameLength,
                response,
                responseDlc
            );
            break;
        }

        default:
        {
            UDS_BuildNegativeResponse(
                serviceId,
                UDS_NRC_SERVICE_NOT_SUPPORTED,
                response,
                responseDlc
            );
            break;
        }
    }

    return 1U;
}
