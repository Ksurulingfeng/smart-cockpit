#ifndef UDS_HANDLER_H
#define UDS_HANDLER_H

#include "sys.h"
#include "../../../shared/uds_protocol.h"

#ifdef NODE_A
#define UDS_RESP_ID CAN_ID_UDS_RESP_A
#else
#define UDS_RESP_ID CAN_ID_UDS_RESP_B
#endif

void Uds_HandleRequest(uint8_t *data, uint8_t len);
void Uds_CheckS3Timeout(void);

#endif
