#ifndef UDS_H
#define UDS_H

#include <stdint.h>

#define UDS_POSITIVE_RESPONSE_OFFSET          0x40U

#define UDS_SID_DIAGNOSTIC_SESSION_CONTROL   0x10U
#define UDS_SID_ECU_RESET                    0x11U
#define UDS_SID_SECURITY_ACCESS              0x27U
#define UDS_SID_REQUEST_DOWNLOAD             0x34U
#define UDS_SID_TRANSFER_DATA                0x36U
#define UDS_SID_REQUEST_TRANSFER_EXIT        0x37U

#define UDS_SESSION_DEFAULT                  0x01U
#define UDS_SESSION_PROGRAMMING              0x02U
#define UDS_SESSION_EXTENDED                 0x03U

#define UDS_RESET_HARD                       0x01U

#define UDS_SECURITY_REQUEST_SEED            0x01U
#define UDS_SECURITY_SEND_KEY                0x02U

#define UDS_NRC_SERVICE_NOT_SUPPORTED        0x11U
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED    0x12U
#define UDS_NRC_INCORRECT_MESSAGE_LENGTH     0x13U
#define UDS_NRC_CONDITIONS_NOT_CORRECT       0x22U
#define UDS_NRC_REQUEST_SEQUENCE_ERROR       0x24U
#define UDS_NRC_REQUEST_OUT_OF_RANGE         0x31U
#define UDS_NRC_SECURITY_ACCESS_DENIED       0x33U
#define UDS_NRC_INVALID_KEY                  0x35U
#define UDS_NRC_GENERAL_PROGRAMMING_FAILURE  0x72U
#define UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER 0x73U

void UDS_Init(void);

uint8_t UDS_ProcessSingleFrame(const uint8_t *request,
                               uint8_t requestDlc,
                               uint8_t *response,
                               uint8_t *responseDlc);

uint8_t UDS_GetSession(void);

uint8_t UDS_IsResetRequested(void);

void UDS_ClearResetRequest(void);

uint8_t UDS_IsSecurityUnlocked(void);

uint8_t UDS_IsDownloadRequested(void);

uint8_t UDS_IsDownloadPreparationPending(void);

void UDS_CompleteDownloadPreparation(uint8_t success,
                                     uint8_t *response,
                                     uint8_t *responseDlc);

uint32_t UDS_GetDownloadAddress(void);

uint32_t UDS_GetDownloadSize(void);

uint32_t UDS_GetTransferredBytes(void);

uint8_t UDS_IsDownloadComplete(void);

uint8_t UDS_IsTransferExited(void);

uint8_t UDS_IsTransferDataPending(void);

uint8_t UDS_GetPendingTransferData(uint32_t *address,
                                   uint8_t *data,
                                   uint8_t *length,
                                   uint8_t *finalBlock);

void UDS_CompleteTransferData(uint8_t success,
                              uint8_t *response,
                              uint8_t *responseDlc);

#endif /* UDS_H */
