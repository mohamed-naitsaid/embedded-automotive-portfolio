#include "uds.h"

static uint8_t udsCurrentSession = UDS_SESSION_DEFAULT;

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

void UDS_Init(void)
{
    udsCurrentSession = UDS_SESSION_DEFAULT;
}

uint8_t UDS_GetSession(void)
{
    return udsCurrentSession;
}

uint8_t UDS_ProcessSingleFrame(const uint8_t *request,
                               uint8_t requestDlc,
                               uint8_t *response,
                               uint8_t *responseDlc)
{
    uint8_t singleFrameLength;
    uint8_t serviceId;
    uint8_t subFunction;
    uint8_t suppressPositiveResponse;

    if ((request == 0) ||
        (response == 0) ||
        (responseDlc == 0) ||
        (requestDlc < 2U) ||
        (requestDlc > 8U))
    {
        return 0U;
    }

    *responseDlc = 0U;

    /*
     * V6.1 supports ISO-TP Single Frames only.
     * High nibble 0x0 = Single Frame.
     */
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
        UDS_BuildNegativeResponse(serviceId,
                                  UDS_NRC_INCORRECT_MESSAGE_LENGTH,
                                  response,
                                  responseDlc);
        return 1U;
    }

    if (serviceId != UDS_SID_DIAGNOSTIC_SESSION_CONTROL)
    {
        UDS_BuildNegativeResponse(serviceId,
                                  UDS_NRC_SERVICE_NOT_SUPPORTED,
                                  response,
                                  responseDlc);
        return 1U;
    }

    if ((singleFrameLength != 2U) || (requestDlc < 3U))
    {
        UDS_BuildNegativeResponse(serviceId,
                                  UDS_NRC_INCORRECT_MESSAGE_LENGTH,
                                  response,
                                  responseDlc);
        return 1U;
    }

    suppressPositiveResponse = request[2] & 0x80U;
    subFunction = request[2] & 0x7FU;

    if ((subFunction != UDS_SESSION_DEFAULT) &&
        (subFunction != UDS_SESSION_PROGRAMMING) &&
        (subFunction != UDS_SESSION_EXTENDED))
    {
        UDS_BuildNegativeResponse(serviceId,
                                  UDS_NRC_SUBFUNCTION_NOT_SUPPORTED,
                                  response,
                                  responseDlc);
        return 1U;
    }

    udsCurrentSession = subFunction;

    if (suppressPositiveResponse != 0U)
    {
        *responseDlc = 0U;
        return 1U;
    }

    /*
     * ISO-TP Single Frame positive response:
     * 06 50 xx P2_H P2_L P2*_H P2*_L
     *
     * P2ServerMax  = 50 ms
     * P2*ServerMax = 5000 ms
     */
    response[0] = 0x06U;
    response[1] = UDS_SID_DIAGNOSTIC_SESSION_CONTROL +
                  UDS_POSITIVE_RESPONSE_OFFSET;
    response[2] = subFunction;
    response[3] = 0x00U;
    response[4] = 0x32U;
    response[5] = 0x01U;
    response[6] = 0xF4U;

    *responseDlc = 7U;

    return 1U;
}
