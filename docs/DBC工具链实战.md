# DBC 工具链实战

> DBC（CAN Database）是汽车行业描述 CAN 消息的标准格式。本文讲解 DBC 文件的结构、与项目代码的对应关系，以及如何用 cantools 和 SavvyCAN 进行离线/在线 CAN 数据分析。

---

## 一、DBC 是什么

DBC 由 Vector Informatik 定义，是 CAN 消息的"字典"——把一堆无意义的 ID 和十六进制数据映射为人类可读的信号名和物理值。

**不加载 DBC** 看到的 CAN 数据：

```
can0  180   [3]  31 96 00
can0  200   [3]  21 FF 00
```

**加载 DBC 后**：

```
A_Ultrasonic  Distance_cm=150  Flags: valid=1 counter=3
B_Temperature Temperature_x10=255 (25.5°C)  Flags: valid=1 counter=2
```

---

## 二、DBC 文件结构详解

以本项目 `smart_cockpit.dbc` 中超声波帧为例：

```
BO_ 384 A_Ultrasonic: 3 NodeA
 SG_ Flags : 0|8@1+ (1,0) [0|255] "" Master
 SG_ Distance_cm : 8|16@1+ (1,0) [0|400] "cm" Master
```

### 2.1 BO_ 行（Message）

```
BO_ 384 A_Ultrasonic: 3 NodeA
│   │   │              │  │
│   │   │              │  └── 发送方
│   │   │              └───── DLC (数据长度)
│   │   └──────────────────── 消息名
│   └──────────────────────── CAN ID (十进制)
└──────────────────────────── 固定关键字
```

CAN ID 用十进制——`384` = `0x180`。

### 2.2 SG_ 行（Signal）

```
 SG_ Distance_cm : 8|16@1+ (1,0) [0|400] "cm" Master
      │             │ │ │ │  │  │   │     │    │
      │             │ │ │ │  │  │   │     │    └── 接收节点 (本项目统一 Master)
      │             │ │ │ │  │  │   │     └─────── 物理单位
      │             │ │ │ │  │  │   └───────────── 最大值
      │             │ │ │ │  │  └───────────────── 最小值
      │             │ │ │ │  └──────────────────── 偏移量
      │             │ │ │ └─────────────────────── 缩放因子
      │             │ │ └───────────────────────── 字节序: + = Intel (LE), - = Motorola (BE)
      │             │ └─────────────────────────── 位宽 (16-bit)
      │             └───────────────────────────── 起始位
      └─────────────────────────────────────────── 信号名
```

**物理值 = raw_value × scale + offset**

例：`Temperature_x10 = 255` → `255 × 0.1 + 0 = 25.5°C`

### 2.3 VAL_ 行（Value Table）

```
VAL_ 385 Gear 0 "P" 1 "R" 2 "D" ;
```

告诉工具"值为 1 时显示 'R' 而不是 '1'"。

### 2.4 字节序：Intel vs Motorola

| 格式 | 符号 | 规则 |
|---|---|---|
| Intel (Little-Endian) | `@1+` | 从 start bit 开始，向后排列 |
| Motorola (Big-Endian) | `@1-` | 从 start bit 开始，向前排列 |

本项目全部消息统一使用 Intel 格式（Little-Endian，`@1+`）。

---

## 三、从 JSON 自动生成 DBC

本项目的 DBC 不是手写的，而是由 Python 脚本从 `tools/can_signals.json` 自动生成：

```python
# tools/dbc_generate_header.py
def gen_dbc(config):
    for m in msgs:
        lines.append(f'BO_ {m["id"]} {m["name"]}: {m["dlc"]} {m["sender"]}')
        for s in m['signals']:
            # Temperature_x10 用 Motorola 字节序, 其余 Intel
            endian = '@1-' if s['name'] == 'Temperature_x10' else '@1+'
            lines.append(
                f' SG_ {s["name"]} : {s["start"]}|{s["length"]}{endian} '
                f'({s["scale"]},{s["offset"]}) [{s["min"]}|{s["max"]}] "{s["unit"]}" Master'
            )
```

### 修改协议的完整流程

1. 编辑 `tools/can_signals.json`
2. 运行：
```bash
python tools/dbc_generate_header.py
```
3. 输出：
   - `shared/can_protocol_common.h` — STM32 + Qt 共享的 C 头文件
   - `shared/smart_cockpit.dbc` — DBC 文件

---

## 四、cantools 的使用

[cantools](https://github.com/cantools/cantools) 是 Python 生态的 CAN 工具库，支持 DBC 编解码。

### 4.1 安装

```bash
pip install cantools
```

### 4.2 编码（生成 CAN 帧）

```python
import cantools
db = cantools.database.load_file('shared/smart_cockpit.dbc')

# 编码超声波帧
msg = db.encode_message('A_Ultrasonic', {
    'Flags': 0x31,        # valid=1, counter=3
    'Distance_cm': 150
})
# msg.frame_id → 0x180
# msg.data → b'\x31\x96\x00'
```

### 4.3 解码（解析 CAN 帧）

```python
decoded = db.decode_message(0x200, b'\x21\xFF\x00')
# → {'Flags': 33, 'Temperature_x10': 255}
# → 温度 = 255 × 0.1 = 25.5°C
```

### 4.4 配合 candump 使用

```bash
# 录制 CAN 日志
candump can0 -l

# Python 离线回放解码
python -c "
import cantools
db = cantools.database.load_file('smart_cockpit.dbc')
# 读取 candump 日志, 逐帧解码
"
```

---

## 五、SavvyCAN 图形化分析

[SavvyCAN](https://www.savvycan.com/) 是跨平台 CAN 总线分析工具，支持 DBC 加载、帧过滤、图形化信号绘制。

### 5.1 连接流程

```
硬件 CAN → USB-CAN 适配器 → SavvyCAN
                                │
                     加载 smart_cockpit.dbc
                                │
                    实时解码: ID→信号名, raw→物理值
```

### 5.2 本项目 DBC 的加载步骤

1. SavvyCAN → File → Load DBC File
2. 选择 `shared/smart_cockpit.dbc`
3. 连接 CAN 设备，开始捕获
4. 右侧面板显示解码后的信号值

---

## 六、DBC 与 C 代码的对应验证

可以用 cantools 做一致性测试——DBC 编解码结果与 C 代码的输出一致：

```python
# DBC 验证
msg = db.encode_message('A_Gear', {'Flags': 0x11, 'Gear': 2})
assert msg.frame_id == 0x181      # 与 can_protocol_common.h 中 CAN_ID_A_GEAR 一致
assert msg.data == b'\x11\x02'    # 与 C struct A_Gear_t 布局一致

# 反向验证
decoded = db.decode_message(0x181, b'\x11\x02')
assert decoded['Gear'] == 'D'     # VAL_ 值表映射正确
```

---

## 七、总结

1. **DBC 是 CAN 字典**——将 ID/字节映射为信号名/物理值
2. **本项目 DBC 自动生成**——编辑 JSON 后运行脚本，C 头文件和 DBC 同步更新
3. **cantools** 用于 Python 离线分析、CI 自动化测试
4. **SavvyCAN** 用于在线实时调试、信号波形可视化
5. **一致性验证** — DBC 编解码结果与 C 代码一致（统一 Intel 小端序）

## 参考资料

- [smart_cockpit.dbc](../shared/smart_cockpit.dbc) — 本项目 DBC 文件
- [can_signals.json](../tools/can_signals.json) — JSON 数据源
- [dbc_generate_header.py](../tools/dbc_generate_header.py) — 生成脚本
- [cantools Documentation](https://cantools.readthedocs.io/)
- [SavvyCAN](https://www.savvycan.com/)
