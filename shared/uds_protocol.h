// Auto-generated from tools/can_signals.json
// 2026-07-01 21:45:21

#ifndef UDS_PROTOCOL_H
#define UDS_PROTOCOL_H

#define UDS_SID_DIAG_SESSION_CTRL    0x10
#define UDS_SID_ECU_RESET            0x11
#define UDS_SID_CLEAR_DTC            0x14
#define UDS_SID_READ_DTC             0x19
#define UDS_SID_READ_DATA_BY_ID      0x22
#define UDS_SID_WRITE_DATA_BY_ID     0x2E
#define UDS_SID_TESTER_PRESENT       0x3E

#define UDS_SESSION_DEFAULT       0x01
#define UDS_SESSION_PROGRAMMING   0x02
#define UDS_SESSION_EXTENDED      0x03

#define UDS_NRC_SERVICE_NOT_SUPPORTED  0x11
#define UDS_NRC_SUBFUNC_NOT_SUPPORTED  0x12
#define UDS_NRC_INCORRECT_LENGTH       0x13
#define UDS_NRC_CONDITIONS_NOT_CORRECT 0x22
#define UDS_NRC_REQUEST_OUT_OF_RANGE   0x31

#define CAN_ID_UDS_REQUEST   0x7E0
#define CAN_ID_UDS_RESPONSE  0x7E8

#define DID_ECU_SERIAL          0xF100
#define DID_VIN_NUMBER          0xF190
#define DID_SOFTWARE_VERSION    0xF1A0
#define DID_BUS_LOAD            0xF1B0
#define DID_TEC                 0xF1B1
#define DID_REC                 0xF1B2
#define DID_TX_COUNT            0xF1B3

#define DTC_E2E_ERROR            0xC00600

#endif
