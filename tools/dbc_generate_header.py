#!/usr/bin/env python3
"""
从 JSON 信号定义生成 CAN 协议 C 头文件和 DBC 文件。

用法: python tools/dbc_generate_header.py
       generator: can_signals.json -> can_protocol_common.h + com_signal_defs.h
                                    + smart_cockpit.dbc + uds_protocol.h
"""

import json
import os
from datetime import datetime

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
JSON_PATH = os.path.join(SCRIPT_DIR, 'can_signals.json')

def load_json():
    with open(JSON_PATH, 'r', encoding='utf-8') as f:
        return json.load(f)

def c_type(sig):
    if sig['signed']:
        return 'int16_t' if sig['length'] > 8 else 'int8_t'
    return 'uint8_t' if sig['length'] <= 8 else 'uint16_t'

def gen_common_header(config):
    msgs = config['messages']
    h = [
        '// Auto-generated from tools/can_signals.json',
        f'// {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}',
        '',
        '#ifndef CAN_PROTOCOL_COMMON_H',
        '#define CAN_PROTOCOL_COMMON_H',
        '',
        '#ifdef __cplusplus',
        '#include <cstdint>',
        '#else',
        '#include <stdint.h>',
        '#endif',
        '',
        '#ifdef __cplusplus',
        'extern "C" {',
        '#endif',
        '',
        f'#define CAN_BITRATE_HZ {config["bitrate"]}',
        '#define CAN_HEARTBEAT_TIMEOUT_MS 3000',
        '',
    ]

    groups = [
        (0x080, 0x0FF),
        (0x100, 0x17F),
        (0x180, 0x1FF),
        (0x200, 0x27F),
        (0x300, 0x37F),
    ]
    for lo, hi in groups:
        for m in msgs:
            if lo <= m['id'] <= hi:
                macro = 'CAN_ID_' + m['name'].upper()
                h.append(f'#define {macro:<40} 0x{m["id"]:03X}')
        h.append('')

    h += [
        '#define CAN_FLAG_VALID           0x01',
        '#define CAN_ROLLING_COUNTER_MASK  0xF0',
        '#define CAN_ROLLING_COUNTER_SHIFT 4',
        '#define CAN_MAKE_FLAGS(v,c) ((((c)<<CAN_ROLLING_COUNTER_SHIFT)&CAN_ROLLING_COUNTER_MASK)|((v)?CAN_FLAG_VALID:0))',
        '#define CAN_GET_VALID(f)    ((f)&CAN_FLAG_VALID)',
        '#define CAN_GET_COUNTER(f)  (((f)&CAN_ROLLING_COUNTER_MASK)>>CAN_ROLLING_COUNTER_SHIFT)',
        '',
        '#define CAN_GEAR_P 0x00',
        '#define CAN_GEAR_R 0x01',
        '#define CAN_GEAR_D 0x02',
        '',
        '#define CAN_NODE_A_ID 0',
        '#define CAN_NODE_B_ID 1',
        '',
        '#define CAN_BUZZER_OFF   0x00',
        '#define CAN_BUZZER_ALERT 0x03',
        '#define CAN_LED_OFF      0x00',
        '#define CAN_LED_ALERT    0x03',
        '',
    ]

    for m in msgs:
        name = m['name'] + '_t'
        fields = [f'{c_type(s)} {s["name"]};' for s in m['signals']]
        if len(fields) == 1:
            h.append(f'typedef struct __attribute__((packed)) {{ {fields[0]} }} {name};')
        else:
            h.append(f'typedef struct __attribute__((packed)) {{ {" ".join(fields)} }} {name};')

    h += [
        '',
        'typedef struct { uint32_t id; uint8_t data[8]; uint8_t len; } CanRxMsg_t;',
        '',
        '#ifdef __cplusplus',
        '}',
        '#endif',
        '',
        '#endif',
    ]

    path = os.path.join(PROJECT_ROOT, 'shared', 'can_protocol_common.h')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(h) + '\n')
    print(f'  {path}')

def gen_dbc(config):
    msgs = config['messages']
    lines = [
        'VERSION ""',
        '',
        'NS_ :',
        '\tNS_DESC_',
        '\tCM_',
        '\tBA_DEF_',
        '\tBA_',
        '\tVAL_',
        '\tBO_TX_BU_',
        '',
        f'BS_: {config["bitrate"]}',
        '',
        f'BU_: {" ".join(config["nodes"])}',
        '',
    ]

    for m in msgs:
        lines.append(f'BO_ {m["id"]} {m["name"]}: {m["dlc"]} {m["sender"]}')
        for s in m['signals']:
            lines.append(
                f' SG_ {s["name"]} : {s["start"]}|{s["length"]}@1+ '
                f'({s["scale"]},{s["offset"]}) [{s["min"]}|{s["max"]}] "{s["unit"]}" Master'
            )
        lines.append('')

    for m in msgs:
        lines.append(f'CM_ BO_ {m["id"]} "{m["name"]}";')

    for name, vals in config.get('value_tables', {}).items():
        items = ' '.join(f'{k} "{v}"' for k, v in vals.items())
        for m in msgs:
            for s in m['signals']:
                if s['name'] == name:
                    lines.append(f'VAL_ {m["id"]} {name} {items} ;')

    path = os.path.join(PROJECT_ROOT, 'shared', 'smart_cockpit.dbc')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'  {path}')

def gen_signal_defs(config):
    msgs = config['messages']
    lines = [
        '// Auto-generated from tools/can_signals.json',
        f'// {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}',
        '',
        '#ifndef COM_SIGNAL_DEFS_H',
        '#define COM_SIGNAL_DEFS_H',
        '',
        '#include "can_protocol_common.h"',
        '#include "uds_protocol.h"',
        '',
        '#ifdef __cplusplus',
        'extern "C" {',
        '#endif',
        '',
        'typedef enum {',
    ]

    all_signals = []
    for m in msgs:
        if m['name'] == 'Diagnostic':
            continue
        for s in m['signals']:
            all_signals.append((m['id'], m['name'], s))

    for cid, mname, sig in all_signals:
        lines.append(f'    {sig["sid"]},')

    lines += [
        '    SID_COUNT',
        '} SignalId_t;',
        '',
        'typedef enum {',
        '    E2E_PROFILE_NONE    = 0,',
        '    E2E_PROFILE_COUNTER = 1,',
        '    E2E_PROFILE_CRC8    = 2,',
        '} E2eProfile_t;',
        '',
        'typedef struct {',
        '    SignalId_t   id;',
        '    uint32_t     can_id;',
        '    uint8_t      byte_offset;',
        '    uint8_t      bit_length;',
        '    uint8_t      is_signed;',
        '    uint8_t      has_flags;',
        '    E2eProfile_t e2e_profile;',
        '} SignalDesc_t;',
        '',
        f'static const SignalDesc_t g_signal_desc[SID_COUNT] = {{',
    ]

    for cid, mname, sig in all_signals:
        byte_off = sig['start'] // 8
        is_flags = (sig['start'] == 0 and sig['length'] == 8
                    and sum(1 for s in all_signals if s[1] == mname) > 1)
        signed = 1 if sig['signed'] else 0
        e2e = 'E2E_PROFILE_COUNTER' if is_flags else 'E2E_PROFILE_NONE'
        can_macro = f'CAN_ID_{mname.upper()}'
        lines.append(
            f'    {{ {sig["sid"]}, {can_macro}, {byte_off}, {sig["length"]}, '
            f'{signed}, {1 if is_flags else 0}, {e2e} }},'
        )

    lines += [
        '};',
        '',
        'void Com_Init(void);',
        'int  Com_SendSignal(SignalId_t id, uint32_t value);',
        'int  Com_SendFlags(SignalId_t flags_id, uint8_t valid, uint8_t counter);',
        'int  Com_Flush(void);',
        'int  Com_SendRaw(uint32_t can_id, const uint8_t *data, uint8_t dlc);',
        'uint16_t Com_GetTxCount(void);',
        'void     Com_ReceiveFrame(uint32_t can_id, const uint8_t *data, uint8_t dlc);',
        'uint32_t Com_ReceiveSignal(SignalId_t id);',
        '#ifdef __cplusplus',
        '}',
        '#endif',
        '',
        '#endif /* COM_SIGNAL_DEFS_H */',
    ]

    path = os.path.join(PROJECT_ROOT, 'shared', 'com_signal_defs.h')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'  {path}')


def gen_uds_protocol(config):
    lines = [
        '// Auto-generated from tools/can_signals.json',
        f'// {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}',
        '',
        '#ifndef UDS_PROTOCOL_H',
        '#define UDS_PROTOCOL_H',
        '',
        '#define UDS_SID_DIAG_SESSION_CTRL    0x10',
        '#define UDS_SID_ECU_RESET            0x11',
        '#define UDS_SID_CLEAR_DTC            0x14',
        '#define UDS_SID_READ_DTC             0x19',
        '#define UDS_SID_READ_DATA_BY_ID      0x22',
        '#define UDS_SID_WRITE_DATA_BY_ID     0x2E',
        '#define UDS_SID_TESTER_PRESENT       0x3E',
        '',
        '#define UDS_SESSION_DEFAULT       0x01',
        '#define UDS_SESSION_PROGRAMMING   0x02',
        '#define UDS_SESSION_EXTENDED      0x03',
        '',
        '#define UDS_NRC_SERVICE_NOT_SUPPORTED  0x11',
        '#define UDS_NRC_SUBFUNC_NOT_SUPPORTED  0x12',
        '#define UDS_NRC_INCORRECT_LENGTH       0x13',
        '#define UDS_NRC_CONDITIONS_NOT_CORRECT 0x22',
        '#define UDS_NRC_REQUEST_OUT_OF_RANGE   0x31',
        '',
        '#define CAN_ID_UDS_REQUEST   0x7E0',
        '#define CAN_ID_UDS_RESPONSE  0x7E8',
        '',
        '#define DID_ECU_SERIAL          0xF100',
        '#define DID_VIN_NUMBER          0xF190',
        '#define DID_SOFTWARE_VERSION    0xF1A0',
        '#define DID_BUS_LOAD            0xF1B0',
        '#define DID_TEC                 0xF1B1',
        '#define DID_REC                 0xF1B2',
        '#define DID_TX_COUNT            0xF1B3',
        '',
        '#define DTC_E2E_ERROR            0xC00600',
        '',
        '#endif',
    ]
    path = os.path.join(PROJECT_ROOT, 'shared', 'uds_protocol.h')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'  {path}')


def main():
    config = load_json()
    print(f'Reading {JSON_PATH} ({len(config["messages"])} messages)')
    print('Generating:')
    gen_common_header(config)
    gen_dbc(config)
    gen_signal_defs(config)
    gen_uds_protocol(config)
    print('Done.')

if __name__ == '__main__':
    main()
