#include "uds.h"

#define UDS_SECURITY_SEED_VALUE              0x12345678U
#define UDS_SECURITY_KEY_MASK                0xA5A5A5A5U

static uint8_t udsCurrentSession = UDS_SESSION_DEFAULT;
static uint8_t udsResetRequested = 0U;
static uint8_t udsSecurityUnlocked = 0U;
static uint8_t udsSeedRequested = 0U;
static uint32_t udsCurrentSeed = UDS_SECURITY_SEED_VALUE;

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
     * Educational V6.3 algorithm only.
     * Real production ECUs must not use a fixed XOR secret.
     */
    return seed ^ UDS_SECURITY_KEY_MASK;
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

void UDS_Init(void)
{
    udsCurrentSession = UDS_SESSION_DEFAULT;
    udsResetRequested = 0U;
    udsSecurityUnlocked = 0U;
    udsSeedRequested = 0U;
    udsCurrentSeed = UDS_SECURITY_SEED_VALUE;
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
