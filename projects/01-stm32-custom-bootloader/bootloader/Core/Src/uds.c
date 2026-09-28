#include "uds.h"

#define UDS_SECURITY_SEED_VALUE              0x12345678U
#define UDS_SECURITY_KEY_MASK                0xA5A5A5A5U

#define UDS_APP_START_ADDRESS                0x08004400U
#define UDS_APP_END_ADDRESS                  0x08010000U

/*
 * V6.4 uses a compact single-frame RequestDownload format.
 *
 * AddressAndLengthFormatIdentifier = 0x22
 *   - 2 bytes memory address
 *   - 2 bytes memory size
 *
 * The 16-bit memory address is interpreted as an offset from
 * UDS_APP_START_ADDRESS.
 *
 * This keeps RequestDownload inside one classic CAN frame.
 * A later ISO-TP multi-frame version can carry full 32-bit
 * address and size fields.
 */
#define UDS_DOWNLOAD_ALFID                   0x22U
#define UDS_DOWNLOAD_DFI                     0x00U
#define UDS_MAX_TRANSFER_BLOCK_LENGTH        0x07U

static uint8_t udsCurrentSession = UDS_SESSION_DEFAULT;
static uint8_t udsResetRequested = 0U;
static uint8_t udsSecurityUnlocked = 0U;
static uint8_t udsSeedRequested = 0U;
static uint32_t udsCurrentSeed = UDS_SECURITY_SEED_VALUE;

static uint8_t udsDownloadRequested = 0U;
static uint32_t udsDownloadAddress = 0U;
static uint32_t udsDownloadSize = 0U;

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
     * Educational V6.3/V6.4 algorithm only.
     * Real production ECUs must not use a fixed XOR secret.
     */
    return seed ^ UDS_SECURITY_KEY_MASK;
}

static void UDS_ResetDownloadState(void)
{
    udsDownloadRequested = 0U;
    udsDownloadAddress = 0U;
    udsDownloadSize = 0U;
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

    /*
     * UDS payload:
     * 34 | DFI | ALFID | Address(2) | Size(2)
     *
     * Total UDS length = 7 bytes.
     */
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

    if ((memorySize == 0U) ||
        ((uint32_t)memoryOffset >= maxApplicationSize) ||
        (memoryAddress < UDS_APP_START_ADDRESS) ||
        (memoryAddress >= UDS_APP_END_ADDRESS) ||
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

    udsDownloadRequested = 1U;
    udsDownloadAddress = memoryAddress;
    udsDownloadSize = memorySize;

    /*
     * Positive response:
     *
     * 03 74 10 07
     *
     * 74 = positive response to 0x34
     * 10 = one-byte maxNumberOfBlockLength field
     * 07 = maximum UDS bytes in one TransferData request
     *
     * V6.5 will use this value for single-frame TransferData.
     */
    response[0] = 0x03U;
    response[1] = UDS_SID_REQUEST_DOWNLOAD +
                  UDS_POSITIVE_RESPONSE_OFFSET;
    response[2] = 0x10U;
    response[3] = UDS_MAX_TRANSFER_BLOCK_LENGTH;

    *responseDlc = 4U;
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

uint32_t UDS_GetDownloadAddress(void)
{
    return udsDownloadAddress;
}

uint32_t UDS_GetDownloadSize(void)
{
    return udsDownloadSize;
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
