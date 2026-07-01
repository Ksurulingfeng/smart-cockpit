#!/usr/bin/env python3
"""
从 JSON 信号定义生成 CAN 协议 C 头文件和 DBC 文件。

用法: python tools/dbc_generate_header.py
       generator: can_signals.json -> can_protocol_common.h + signal_id.h + smart_cockpit.dbc
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
    if sig['signed'] and sig['length'] > 8:
        return 'int16_t'
    if sig['length'] <= 8:
        return 'uint8_t'
    return 'uint16_t'

def gen_common_header(config):
    msgs = config['messages']
    h = [
        '// Auto-generated from tools/can_signals.json',
        f'// {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}',
        '//',
        '// ID scheme: bit[10:8]=node  bit[7]=dir(0=ctrl)  bit[6:0]=signal',
        '//   Master→A ctrl: 0x080-0x0FF  (highest priority)',
        '//   Master→B ctrl: 0x100-0x17F',
        '//   A→Master rpt:  0x180-0x1FF',
        '//   B→Master rpt:  0x200-0x27F',
        '//   Diagnostic:    0x300-0x37F  (lowest priority)',
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

    # group messages by ID range for sectioned output
    groups = [
        (0x080, 0x0FF, 'Master→A control (highest priority)'),
        (0x100, 0x17F, 'Master→B control'),
        (0x180, 0x1FF, 'A→Master report'),
        (0x200, 0x27F, 'B→Master report'),
        (0x300, 0x37F, 'Diagnostic (lowest priority)'),
    ]
    for lo, hi, desc in groups:
        h.append(f'// ---- {desc} (0x{lo:03X}-0x{hi:03X}) ----')
        for m in msgs:
            if lo <= m['id'] <= hi:
                macro = 'CAN_ID_' + m['name'].upper()
                h.append(f'#define {macro:<40} 0x{m["id"]:03X}')
        h.append('')

    h += [
        '// ---- Flags byte encoding ----',
        '// byte0 bit[0]=valid  bit[7:4]=rolling_counter(0-15)  bit[3:1]=reserved',
        '#define CAN_FLAG_VALID           0x01',
        '#define CAN_ROLLING_COUNTER_MASK  0xF0',
        '#define CAN_ROLLING_COUNTER_SHIFT 4',
        '#define CAN_MAKE_FLAGS(v,c) ((((c)<<CAN_ROLLING_COUNTER_SHIFT)&CAN_ROLLING_COUNTER_MASK)|((v)?CAN_FLAG_VALID:0))',
        '#define CAN_GET_VALID(f)    ((f)&CAN_FLAG_VALID)',
        '#define CAN_GET_COUNTER(f)  (((f)&CAN_ROLLING_COUNTER_MASK)>>CAN_ROLLING_COUNTER_SHIFT)',
        '',
        '// ---- Gear ----',
        '#define CAN_GEAR_P 0x00 // fail-safe: open circuit → park',
        '#define CAN_GEAR_R 0x01',
        '#define CAN_GEAR_D 0x02',
        '',
        '#define CAN_NODE_A_ID 0',
        '#define CAN_NODE_B_ID 1',
        '',
        '// ---- Actuator values ----',
        '#define CAN_BUZZER_OFF   0x00',
        '#define CAN_BUZZER_ALERT 0x03',
        '#define CAN_LED_OFF      0x00',
        '#define CAN_LED_ALERT    0x03',
        '',
        '// ---- Frame structs (byte0=flags for sensor, byte0=payload for ctrl) ----',
    ]

    for m in msgs:
        name = m['name'] + '_t'
        fields = []
        for s in m['signals']:
            fields.append(f'{c_type(s)} {s["name"]};')
        if len(fields) == 1:
            h.append(f'typedef struct __attribute__((packed)) {{ {fields[0]} }} {name};')
        else:
            h.append(f'typedef struct __attribute__((packed)) {{ {" ".join(fields)} }} {name};')

    # add struct layout comment for multi-field frames
    for m in msgs:
        if len(m['signals']) > 1:
            name = m['name'] + '_t'
            byte_note = '  '.join(f'{s["start"]//8}:{s["name"]}' for s in m['signals'])
            h.append(f'// {name} layout: {byte_note}')

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
            endian = '@1+'
            lines.append(
                f' SG_ {s["name"]} : {s["start"]}|{s["length"]}{endian} '
                f'({s["scale"]},{s["offset"]}) [{s["min"]}|{s["max"]}] "{s["unit"]}" Master'
            )
        lines.append('')

    # Comment lines
    for m in msgs:
        lines.append(f'CM_ BO_ {m["id"]} "{m["name"]}";')

    # Value tables
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
        '/**',
        ' * COM 层信号定义 — 从 tools/can_signals.json 自动生成',
        ' * 所有信号名和参数与 DBC 一致。',
        ' * SWC 通过 Com_SendSignal(SID_xxx, value) 发送,',
        ' *        通过 Com_ReceiveSignal(SID_xxx) 接收。',
        ' * 不直接操作 CAN 帧。',
        ' */',
        '',
        '#ifndef COM_SIGNAL_DEFS_H',
        '#define COM_SIGNAL_DEFS_H',
        '',
        '#include "can_protocol_common.h"',
        '',
        '#ifdef __cplusplus',
        'extern "C" {',
        '#endif',
        '',
        '/* ==================== 信号 ID 枚举 ==================== */',
        '',
        'typedef enum {',
    ]

    # 收集所有信号 (跳过 Diagnostic，它不通过 COM 层收发)
    all_signals = []
    for m in msgs:
        if m['name'] == 'Diagnostic':
            continue
        for s in m['signals']:
            all_signals.append((m['id'], m['name'], s))

    for i, (cid, mname, sig) in enumerate(all_signals):
        comment = f'// 0x{cid:03X} {sig["name"]}'
        if i < len(all_signals) - 1:
            lines.append(f'    {sig["sid"]}, {comment:<40}')
        else:
            lines.append(f'    {sig["sid"]}, {comment:<40}')

    lines += [
        '',
        '    SID_COUNT',
        '} SignalId_t;',
        '',
        '/* ==================== 信号描述符 ==================== */',
        '',
        'typedef struct {',
        '    SignalId_t  id;',
        '    uint32_t    can_id;',
        '    uint8_t     byte_offset;',
        '    uint8_t     bit_length;',
        '    uint8_t     is_signed;',
        '    uint8_t     has_flags;',
        '} SignalDesc_t;',
        '',
        f'static const SignalDesc_t g_signal_desc[SID_COUNT] = {{',
    ]

    for i, (cid, mname, sig) in enumerate(all_signals):
        byte_off = sig['start'] // 8
        has_flags = 1 if sig['name'] == 'Flags' else 0
        signed = 1 if sig['signed'] else 0
        lines.append(
            f'    {{ {sig["sid"]}, 0x{cid:03X}, {byte_off}, {sig["length"]}, {signed}, {has_flags} }},'
        )

    lines += [
        '};',
        '',
        '/* COM 层接口 */',
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


def main():
    config = load_json()
    print(f'Reading {JSON_PATH} ({len(config["messages"])} messages)')
    print('Generating:')
    gen_common_header(config)
    gen_dbc(config)
    gen_signal_defs(config)
    print('Done.')

if __name__ == '__main__':
    main()
