# shared/ — 跨项目共享文件

> 本目录所有 `.h` 文件由 `tools/dbc_generate_header.py` 从 `tools/can_signals.json` 自动生成，勿手动编辑。

## can_protocol_common.h

CAN 协议头文件，STM32 和 Qt 两端共享。

### CAN ID 宏

| 范围 | 含义 |
|---|---|
| 0x080-0x0FF | 主控→节点A 控制帧（最高优先级） |
| 0x100-0x17F | 主控→节点B 控制帧 |
| 0x180-0x1FF | 节点A→主控 上报帧 |
| 0x200-0x27F | 节点B→主控 上报帧 |
| 0x300-0x37F | 诊断帧（最低优先级） |

ID 分配：`bit[10:8]=节点` `bit[7]=方向(0=控制)` `bit[6:0]=信号`。

### Flags 字节编码

`byte0: bit[0]=有效标志  bit[7:4]=4-bit滚动计数器(0-15)`

| 宏 | 说明 |
|---|---|
| `CAN_FLAG_VALID` | 有效标志位掩码 (0x01) |
| `CAN_MAKE_FLAGS(v,c)` | 构造 flags 字节 (valid+counter) |
| `CAN_GET_VALID(f)` | 提取 valid 位 |
| `CAN_GET_COUNTER(f)` | 提取 counter 值 |

### 档位 / 执行器值

- `CAN_GEAR_P=0` 防御性编码：悬空默认驻车
- `CAN_GEAR_R=1` `CAN_GEAR_D=2`
- `CAN_BUZZER_OFF=0` `CAN_BUZZER_ALERT=3`
- `CAN_LED_OFF=0` `CAN_LED_ALERT=3`

### 帧结构体

每个消息对应一个 `__attribute__((packed))` 结构体。传感器上报帧 byte0 为 flags，控制帧 byte0 为 payload。

---

## com_signal_defs.h

COM 层信号定义，STM32 和 Qt 两端共享。

### SignalId_t 枚举

22 个信号 ID，命名规则：
- 控制帧：`SID_` + 节点 + 功能 + 角色（如 `SID_A_BUZZER_MODE`）
- Flags 信号：`SID_FLAGS_` + 信号组名（如 `SID_FLAGS_ULTRASONIC`）
- 数据信号：`SID_` + 信号名大写（如 `SID_DISTANCE_CM`）

### E2eProfile_t

| 值 | 含义 |
|---|---|
| `E2E_PROFILE_NONE` | 控制帧，无 E2E 保护 |
| `E2E_PROFILE_COUNTER` | Flags 信号，含 valid+counter |
| `E2E_PROFILE_CRC8` | 预留 |

### SignalDesc_t（描述符表）

每个信号一条记录，包含：所属 CAN ID、在 8 字节 payload 中的字节偏移、位宽、是否有符号、是否含 flags 字节、E2E 保护级别。

发送时 `Com_SendSignal(SID, value)` 查表定位字节偏移；接收时 `Com_ReceiveSignal(SID)` 逆向解包。

### COM API

- **发送**：`Com_SendSignal` / `Com_SendFlags` / `Com_Flush` / `Com_SendRaw`
- **接收**：`Com_ReceiveFrame` / `Com_ReceiveSignal`
- **计数**：`Com_GetTxCount`
- **初始化**：`Com_Init`

---

## uds_protocol.h

UDS 诊断协议常量（ISO 14229-1），STM32 和 Qt 两端共享。

### UDS 服务 ID (SID)

| 宏 | 值 | 服务 |
|---|---|---|
| `UDS_SID_DIAG_SESSION_CTRL` | 0x10 | 诊断会话控制 |
| `UDS_SID_ECU_RESET` | 0x11 | ECU 复位 |
| `UDS_SID_CLEAR_DTC` | 0x14 | 清除故障码 |
| `UDS_SID_READ_DTC` | 0x19 | 读故障码 |
| `UDS_SID_READ_DATA_BY_ID` | 0x22 | 按 ID 读数据 |
| `UDS_SID_WRITE_DATA_BY_ID` | 0x2E | 按 ID 写数据 |
| `UDS_SID_TESTER_PRESENT` | 0x3E | 诊断会话保持 |

### 否定响应码 (NRC)

| 宏 | 值 | 含义 |
|---|---|---|
| `UDS_NRC_SERVICE_NOT_SUPPORTED` | 0x11 | 服务不支持 |
| `UDS_NRC_SUBFUNC_NOT_SUPPORTED` | 0x12 | 子功能不支持 |
| `UDS_NRC_INCORRECT_LENGTH` | 0x13 | 消息长度错误 |
| `UDS_NRC_CONDITIONS_NOT_CORRECT` | 0x22 | 条件不满足 |
| `UDS_NRC_REQUEST_OUT_OF_RANGE` | 0x31 | 请求超出范围 |

### 数据标识符 (DID)

| 宏 | 值 | 数据内容 |
|---|---|---|
| `DID_ECU_SERIAL` | 0xF100 | ECU 序列号 |
| `DID_VIN_NUMBER` | 0xF190 | VIN 码 |
| `DID_SOFTWARE_VERSION` | 0xF1A0 | 软件版本号 |
| `DID_BUS_LOAD` | 0xF1B0 | CAN 总线负载 |
| `DID_TEC` | 0xF1B1 | 发送错误计数 |
| `DID_REC` | 0xF1B2 | 接收错误计数 |
| `DID_TX_COUNT` | 0xF1B3 | 累计发送帧数 |

### CAN ID

- `CAN_ID_UDS_REQUEST = 0x7E0`（诊断仪→ECU）
- `CAN_ID_UDS_RESPONSE = 0x7E8`（ECU→诊断仪）

### 诊断故障码 (DTC)

- `DTC_E2E_ERROR = 0xC00600`

---

## smart_cockpit.dbc

CAN Database 标准格式文件，可直接加载到 SavvyCAN、Vector CANalyzer、cantools 等工具中解码本项目 CAN 报文。定义了所有 15 个消息的信号布局（起始位、长度、字节序、缩放因子、物理单位）和 7 组值表枚举。

## 代码生成流程

```
tools/can_signals.json  (编辑协议)
    │
    ▼
python tools/dbc_generate_header.py
    │
    ├─→ can_protocol_common.h
    ├─→ com_signal_defs.h
    ├─→ uds_protocol.h
    └─→ smart_cockpit.dbc
```
