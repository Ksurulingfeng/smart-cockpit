#ifndef UDS_HANDLER_H
#define UDS_HANDLER_H

#include "sys.h"

void Uds_HandleRequest(uint8_t *data, uint8_t len);
void Uds_CheckS3Timeout(void);

#endif
