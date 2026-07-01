# 基于 CAN 总线的分布式智能座舱系统设计与实现

## 摘要

本文设计并实现了一套基于 CAN 总线的分布式智能座舱系统，由 STM32F103 双节点仿真固件和 Qt5 车载 Linux 终端应用两部分构成。下位机通过 FreeRTOS 实时操作系统管理七类任务，以 250kbps CAN 总线周期上报动力域（转速、油量、档位、超声波距离）和车身域（温湿度、风扇、车窗）传感器数据，并接收执行器控制指令。上位机运行于 ARM Linux 平台，集成百度地图导航、4G 联网、倒车影像等功能，通过 SocketCAN 与下位机通信。本文重点论述了 CAN 协议设计中的 ID 分配方案、信号有效性标志机制、滚动计数器丢帧检测、心跳超时监测等容错技术，并结合 DBC 标准文件实现了工业级 CAN 工具链集成。

**关键词**：CAN 总线；智能座舱；FreeRTOS；Qt；SocketCAN；DBC

---

## 第 1 章 绪论

### 1.1 研究背景与意义

随着汽车电子电气架构从分布式向域集中式演进，CAN（Controller Area Network）总线仍是车内通信的核心骨干网络。ISO 11898 标准定义的 CAN 协议以其多主架构、仲裁机制和错误检测能力，在动力域、车身域等场景中广泛应用。

智能座舱作为人车交互的核心界面，需要实时汇聚并展示来自多个 ECU 的传感器数据。本课题构建了一套完整的 CAN 总线分布式系统原型，覆盖从底层固件到上层应用的完整技术栈，对理解汽车电子架构具有教学和工程双重价值。

### 1.2 国内外研究现状

CAN 总线自 1986 年由 Bosch 发布以来，已衍生出 CAN FD（灵活数据速率）、CAN XL 等新标准。上层协议包括 CANopen（工业自动化）、J1939（商用车）、ISO 15765（诊断）等。在座舱领域，AUTOSAR 规范定义了完整的通信栈（ComStack），包括 E2E（端到端）保护、网络管理等模块。本课题参考了 AUTOSAR E2E Profile 1 的滚动计数器机制和信号有效性标志概念。

### 1.3 主要工作与章节安排

本文组织结构如下：第 2 章阐述系统总体架构；第 3 章详述 CAN 协议设计；第 4 章描述下位机固件实现；第 5 章描述上位机应用实现；第 6 章分析系统的容错与诊断机制；第 7 章总结与展望。

---

## 第 2 章 系统总体设计

### 2.1 系统架构

系统采用"主控 + 双节点"分布式架构，通过一条 CAN 总线连接：

```
┌────────────────────────────────────────────┐
│  ARM Linux (Yocto/Poky)                    │
│  ┌──────────────────────────────────────┐  │
│  │  Qt 5.15 车载终端                     │  │
│  │  ├─ HomePage    车速/转速/油量/温湿度  │  │
│  │  ├─ NavigationPage  百度地图导航       │  │
│  │  ├─ RearViewCamera  倒车影像+超声波    │  │
│  │  ├─ SettingsPage  CAN/串口助手/WiFi   │  │
│  │  └─ ToolsPage     风扇/车窗/蜂鸣/LED  │  │
│  ├──────────────────────────────────────┤  │
│  │  core/ 单例管理器层                   │  │
│  │  CanManager ─── SocketCAN             │  │
│  │  Rte / ComLayer ─── CAN协议解析+校验    │  │
│  │  EC20Manager / MapManager / ...       │  │
│  └──────────────────┬───────────────────┘  │
└─────────────────────┼──────────────────────┘
                      │ CAN 250kbps
┌─────────────────────┼──────────────────────┐
│  STM32F103C8Tx × 2  │  FreeRTOS            │
│  ┌───────────────────┴─────────────────┐   │
│  │  节点A (动力域)       节点B (车身域)  │   │
│  │  上报: 0x180-0x183    上报: 0x200-0x203│  │
│  │  接收: 0x080-0x081    接收: 0x100-0x103│  │
│  │  传感器:             传感器:           │   │
│  │  ADC(转速/油量)       DHT11(温湿度)    │   │
│  │  HC-SR04(超声波)      执行器:          │   │
│  │  档位开关             风扇(L9110S)     │   │
│  │  执行器:             车窗(SG90舵机)   │   │
│  │  蜂鸣器/LED          蜂鸣器/LED       │   │
│  │  OLED 128×32          OLED 128×32     │   │
│  └─────────────────────────────────────┘   │
└────────────────────────────────────────────┘
```

### 2.2 硬件选型

| 组件 | 型号 | 关键参数 |
|---|---|---|
| MCU | STM32F103C8Tx | Cortex-M3, 72MHz, 64KB Flash, 20KB SRAM |
| CAN 收发器 | TJA1050 | 待机模式, 最高 1Mbps |
| 温湿度传感器 | DHT11 | 0-50°C, ±2°C, 单总线 |
| 超声波模块 | HC-SR04 | 2-400cm, 5V 触发/回波 |
| OLED | SSD1306 | 128×32, I2C 接口 |
| 直流电机驱动 | L9110S | PWM 调速, 2.5-12V |
| 舵机 | SG90 | 0-180°, PWM 控制 |
| 车机 | ARM Linux 开发板 | Yocto/Poky, Qt 5.15 |

---

## 第 3 章 CAN 总线协议设计

### 3.1 物理层参数

CAN 总线配置如下：波特率 250 kbps，APB1 时钟 36 MHz，预分频器 9，CAN 内核时钟 4 MHz，TQ 周期 0.25μs。位时间 = SyncSeg(1TQ) + BS1(12TQ) + BS2(3TQ) = 16TQ = 4μs，波特率 = 36MHz/(9×16) = 250kbps 验证正确。

采样点位于 (SyncSeg+BS1)/Total = (1+12)/16 = **81.25%**，符合 Bosch CAN 2.0 规范推荐的 75%-87.5% 采样点范围。SJW = 1TQ ≤ BS2(3TQ) 满足要求，保证了位同步的稳定性。相比 CubeMX 默认配置（预分频=18, BS1=4, BS2=3, 采样点 62.5%），优化后采样点推迟了近 3 个 TQ，在长距离布线和多节点仲裁场景下具有更好的抗干扰裕度。

### 3.2 ID 分配方案

CAN 标识符不仅标识消息类型，还决定总线仲裁优先级——11 位标准 ID 中，值越小优先级越高。本系统采用 bit-field 编码方案：

```
bit[10:8] = node ID:   001=A(动力域), 010=B(车身域), 011=诊断/广播
bit[7]    = direction: 0=control(控制, 高优先级), 1=report(上报, 低优先级)
bit[6:0]  = signal ID: 0-127
```

**设计依据**：bit[7] 赋给方向字段并将 control 设为 0，确保所有控制指令（如倒车报警蜂鸣器、LED 警示）的仲裁优先级高于传感器上报帧。这符合实时系统中安全关键指令应优先调度的原则。完整 ID 分配如下：

| 范围 | bit-field | 节点 | 方向 | 优先级 | 包含 |
|---|---|---|---|---|---|
| 0x080-0x0FF | 001 0 xxxxxxx | A | 控制 | 最高 | Buzzer(0x080) LED(0x081) |
| 0x100-0x17F | 010 0 xxxxxxx | B | 控制 | 高 | Fan(0x100) Window(0x101) Buzzer(0x102) LED(0x103) |
| 0x180-0x1FF | 001 1 xxxxxxx | A | 上报 | 中 | Ultrasonic(0x180) Gear(0x181) Fuel(0x182) RPM(0x183) |
| 0x200-0x27F | 010 1 xxxxxxx | B | 上报 | 低 | Temperature(0x200) Humidity(0x201) FanFeedback(0x202) WindowFeedback(0x203) |
| 0x300-0x37F | 011 1 xxxxxxx | Diag | 上报 | 最低 | DiagReport(0x300) |

### 3.3 消息定义

每个消息的数据长度码（DLC）精确匹配其信号布局。传感器上报帧采用统一约定：byte0 编码信号标志位（flags），数据从 byte1 开始。flags 字节编码方案如下：

```
byte0: [7:4] = 4-bit rolling counter (0-15)
       [3:1] = reserved
       [0]   = valid flag (1=传感器数据有效)
```

#### 3.3.1 节点 A 上报帧

**0x180 A_Ultrasonic（DLC=3）**
| Byte | 位 | 信号 | 类型 | 说明 |
|---|---|---|---|---|
| 0 | [7:0] | Flags | uint8 | [7:4]=counter, [0]=valid |
| 1-2 | [15:0] | Distance_cm | uint16 LE | 超声波距离, 单位 cm, 范围 0-400 |

**0x181 A_Gear（DLC=2）**
| Byte | 位 | 信号 | 类型 | 说明 |
|---|---|---|---|---|
| 0 | [7:0] | Flags | uint8 | 同上 |
| 1 | [7:0] | Gear | uint8 | P=0, R=1, D=2 |

P 编码为 0 是防御性设计：三档拨动开关接触不良或 ADC 悬空时读回 0，默认进入驻车档（最安全状态），避免"误读为行进档导致溜车"。

**0x182 A_Fuel（DLC=3）**
| Byte | 位 | 信号 | 类型 | 说明 |
|---|---|---|---|---|
| 0 | [7:0] | Flags | uint8 | 同上 |
| 1-2 | [15:0] | Fuel_percent_x10 | uint16 LE | 油量×10, e.g. 820 表示 82.0% |

**0x183 A_RPM（DLC=3）**
| Byte | 位 | 信号 | 类型 | 说明 |
|---|---|---|---|---|
| 0 | [7:0] | Flags | uint8 | 同上 |
| 1-2 | [15:0] | Rpm_percent_x10 | uint16 LE | 转速×10, 范围 0-1000 |

#### 3.3.2 节点 B 上报帧

| CAN ID | 名称 | DLC | 信号 | 说明 |
|---|---|---|---|---|
| 0x200 | B_Temperature | 3 | flags + int16 LE temp×10 | 有符号, e.g. 255=25.5°C |
| 0x201 | B_Humidity | 3 | flags + uint16 LE hum×10 | e.g. 655=65.5% |
| 0x202 | B_Fan_Speed_Actual | 2 | flags + uint8 speed | 执行器实际反馈 |
| 0x203 | B_Window_Pos_Actual | 2 | flags + uint8 pos | 执行器实际反馈 |

#### 3.3.3 控制帧

控制帧 DLC=1，byte0 直接承载指令 payload，不携带 flags 字节。

| CAN ID | 目标 | 信号 | 取值 |
|---|---|---|---|
| 0x080 | A | Buzzer Mode | 0=OFF, 3=ALERT |
| 0x081 | A | LED State | 0=OFF, 3=ALERT |
| 0x100 | B | Fan Target Speed | 0-100% |
| 0x101 | B | Window Target Pos | 0-100% |
| 0x102 | B | Buzzer Mode | 0=OFF, 3=ALERT |
| 0x103 | B | LED State | 0=OFF, 3=ALERT |

#### 3.3.4 诊断帧

**0x300 DiagReport（DLC=8）**，每 10s 由各节点发送一次，包含 TEC（发送错误计数）、REC（接收错误计数）、总线负载估计等 CAN 控制器健康指标，用于总线状态诊断。

| Byte | 信号 | 说明 |
|---|---|---|
| 0 | [7:4]=node_id, [3:0]=can_state | 0=OK, 1=Warning, 2=ErrorPassive, 3=BusOff |
| 1-2 | TEC | uint16 LE, 发送错误计数 |
| 3-4 | REC | uint16 LE, 接收错误计数 |
| 5-6 | TX_Msg_Count | uint16 LE, 累计发送帧数 |
| 7 | Bus_Load_Percent | 粗略总线负载百分比 |

### 3.4 UDS 诊断服务（ISO 14229-1）

在周期上报诊断帧（0x300）的基础上，本系统进一步实现了 UDS（Unified Diagnostic Services）诊断协议，支持主控通过 CAN ID 0x7E0 向 STM32 节点发起诊断请求，节点在 0x7E8 回复。UDS 帧格式遵循 ISO 14229-1 标准：

```
请求帧 (0x7E0): [SID] [子功能/DID] [参数...]
正响应 (0x7E8): [SID | 0x40] [数据...]
负响应 (0x7E8): [0x7F] [原始SID] [NRC]
```

已实现的 UDS 服务：

| SID | 名称 | 功能 |
|---|---|---|
| 0x10 | 诊断会话控制 | 切换默认/编程/扩展会话，非默认会话启动 S3(5s) 超时定时器 |
| 0x22 | 按 ID 读数据 | 支持 7 个 DID：软件版本(0xF1A0)、VIN(0xF190)、序列号(0xF100)、TEC(0xF1B1)、REC(0xF1B2)、TX计数(0xF1B3)、总线负载(0xF1B0) |
| 0x3E | 诊断会话保持 | 重置 S3 定时器，防止会话超时 |
| 0x11 | ECU 复位 | 先回复 ACK，20ms 后调用 NVIC_SystemReset() |

UDS 常量（SID、NRC、DID、DTC）及 CAN ID（0x7E0/0x7E8）统一定义在共享头文件 `uds_protocol.h` 中，STM32 和 Qt 两端引用同一源。UDS 接收通过第二个 CAN 滤波器组（FilterBank=1）精确匹配 0x7E0。S3 超时基于 FreeRTOS tick 差值比较（避免计数器回绕失效），任何有效 UDS 请求均刷新定时器，超时后自动切回默认会话。TesterPresent 的 SPR 抑制位按 ISO 14229-1 规范处理（bit7=1 时不回复）。

### 3.5 端到端保护机制

参考 AUTOSAR E2E Profile 1 的设计理念，本系统在应用层实现了数据完整性保护的三个层面：

1. **信号有效性**（Data Validity）：flags[0] 由传感器任务在每次采集后设置——读取成功置 1，失败置 0。接收方检测无效标志时触发 `sensorFault` 信号而非更新 UI。

2. **数据新鲜度**（Data Freshness）：flags[7:4] 为 4-bit 滚动计数器，每个信号独立递增 0→15→0。接收方检测跳跃（`expected = (last + 1) & 0x0F`）时记录丢帧日志。4-bit 循环周期为 16 帧，在 10Hz 发送频率下足够覆盖 1.6 秒窗口。

3. **节点存活性**（Node Liveness）：取消独立心跳帧，改为"任一上报帧到达即视为心跳"。主控端 1Hz 定时器检查距最后接收帧是否超过 3 秒，超时触发 `nodeOffline` 信号。

### 3.6 DBC 文件

本系统的 CAN 协议以标准 DBC 格式发布（`shared/smart_cockpit.dbc`），可直接加载到 Vector CANalyzer、SavvyCAN、cantools 等工业标准工具中。DBC 文件定义了所有 15 个消息的精确信号布局（起始位、长度、字节序、缩放因子、偏移量、物理单位）和 7 组值表枚举（如 CAN_State: 0=OK/1=Warning/2=ErrorPassive/3=BusOff），确保工具链解码与软件实现完全一致。

---

## 第 4 章 下位机固件设计

### 4.1 FreeRTOS 任务架构

下位机基于 FreeRTOS 10.x 构建，运行于 STM32F103C8Tx（Cortex-M3, 72MHz, 64KB Flash, 20KB SRAM）。系统配置 7 个任务：

```
┌──────────────────────────────────────────┐
│ FreeRTOS Scheduler                       │
│ ┌─────────┐ ┌─────────┐ ┌─────────────┐ │
│ │ CAN任务  │ │ ADC任务  │ │ 超声波任务    │ │
│ │ prio=4  │ │ prio=3  │ │ prio=3      │ │
│ │ 20ms    │ │ 50ms    │ │ 500ms       │ │
│ └────┬────┘ └─────────┘ └──────┬───────┘ │
│      │                         │         │
│ ┌────┴────┐ ┌─────────┐ ┌──────┴───────┐ │
│ │执行器    │ │温湿度    │ │ 按键任务      │ │
│ │prio=2   │ │prio=2   │ │ prio=2      │ │
│ │20ms     │ │2s       │ │ 20ms        │ │
│ └─────────┘ └─────────┘ └──────────────┘ │
│ ┌─────────────────────────────────────┐  │
│ │ 显示任务  prio=1  200ms (OLED)      │  │
│ └─────────────────────────────────────┘  │
└──────────────────────────────────────────┘
```

任务间通过 6 个 FreeRTOS 队列通信。传感器数据通过全局变量共享（ADC、温湿度、超声波任务写入，CAN 任务读取发送）。CAN 接收路径：`HAL_CAN_RxFifo0MsgPendingCallback` (ISR) → `xQueueSendFromISR` → CAN 任务出队分发到执行器队列。

### 4.2 CAN 驱动层

采用"BSP 驱动 + HAL 抽象"双层架构。`can_drv.c` 提供 `CAN_SendMessage()` 和 `CAN_RegisterRxCallback()` API，`can.c`（CubeMX 生成）负责硬件初始化和 ISR。

#### 4.2.1 发送超时机制

CAN 任务优先级为系统最高（4）。当 CAN 控制器进入 BusOff 状态后，邮箱永久不可用，`HAL_CAN_AddTxMessage` 持续返回 `HAL_ERROR`。若使用 `while(1)` 无限重试，该任务不会调用任何能让出 CPU 的函数——结果是被同等或更低优先级的所有任务（包括 idle 任务）永久饿死，系统表现为"死机"。

解决方案：限制重试 100 次（约 5ms 总超时），每 10 次调用 `taskYIELD()` 让出 CPU。5ms 后仍失败则返回 -1，CAN 任务在下一个 20ms 周期自然重试。

#### 4.2.2 DLC 边界保护

CAN 帧的 DLC 字段为 4-bit（0-15），CAN FD 标准将 DLC 9-15 映射到 12/16/20/24/32/48/64 字节。虽然本系统使用标准 CAN（最大 8 字节），但总线上的电气噪声或恶意设备可能产生 DLC>8 的帧。ISR 中的 `memcpy(msg.data, data, len)` 若 `len>8` 将导致栈溢出——`msg.data` 仅 8 字节。修复：`if (len > 8) len = 8;`。

#### 4.2.3 错误监控

启用 `AutoBusOff = ENABLE` 使节点在错误计数超过阈值后自动恢复。注册 `HAL_CAN_ErrorCallback`，读取 ESR 寄存器提取 TEC（发送错误计数）和 REC（接收错误计数），分类为 OK/Warning/ErrorPassive/BusOff 四个错误等级，通过诊断帧每 10s 上报。

#### 4.2.4 精确滤波器

STM32F103 的 bxCAN 外设提供 14 个滤波器组。原实现配置掩码为全 0，接受总线上所有 ID——ISR 每次中断都要读取 FIFO 并根据软件判断是否需要该帧，浪费 CPU 且挤占有限的 FIFO 空间（2 个 FIFO，每个 3 帧）。

改进后的滤波配置：节点 A 仅接收 0x080-0x081（`FilterId=0x080, MaskId=0x7FE`，屏蔽 bit0）；节点 B 仅接收 0x100-0x103（`FilterId=0x100, MaskId=0x7FC`，屏蔽 bit[1:0]）。硬件层面过滤掉不关心的帧，减少 ISR 开销。

### 4.3 传感器有效性机制

DHT11 温湿度传感器和 HC-SR04 超声波模块均可能因电气干扰、时序偏差或物理环境异常导致读取失败。原实现中，读取失败时仅打印日志，不更新全局变量——CAN 任务继续发送上一次成功读取的旧值，主控端无法区分"读数正常"与"传感器已坏但使用旧值"。

改进后，传感器任务在每次采集后设置全局有效性标志（`g_ultrasonic_valid`、`g_temperature_valid`、`g_humidity_valid`）：成功置 1，失败置 0。CAN 任务在填充传感器帧时，通过 `CAN_MAKE_FLAGS(valid, counter)` 将有效性标志编码到 flags 字节 bit0。主控端解析时，若发现该位为 0，触发 `sensorFault` 信号而非更新 UI 显示。

---

## 第 5 章 上位机应用设计

### 5.1 软件架构

上位机基于 Qt 5.15 + CMake 构建，运行于 Yocto/Poky Linux（ARM）或 x86 Linux（开发调试）。核心层采用 Meyer's 单例模式管理 8 个管理器，通过信号-槽机制实现模块间松耦合通信。

### 5.2 CAN 通信模块

`CanManager` 封装 Qt SerialBus 模块的 SocketCAN 插件，提供设备扫描、连接管理、帧收发功能。连接流程：通过 `ip link` 命令行配置接口类型为 CAN → 创建 `QCanBusDevice` 并设置 bitrate → 调用 `connectDevice()` 建立连接 → 通过 `ip link set up` 启用接口。

CAN 数据业务处理由 RTE 层和 COM 层分工协作完成。参考 AUTOSAR 分层架构，`Rte` 负责模式切换（CAN/Mock）和信号分发，`ComLayer` 负责计数器校验。

1. **RTE 层信号解析**（`rte.cpp:onCanFrame`）：收到 CAN 帧后按 ID 分发，使用 COM 层辅助函数 `CAN_GET_VALID` 检查有效标志位，有效时 emit 对应 Qt 信号（如 `speedChanged`、`temperatureChanged`）。每个 CAN ID 的报告帧到达时 `restart()` 对应节点心跳计时器。

2. **COM 层计数器校验**（`comlayer.cpp:processCanFrame`）：仅处理传感器帧（`0x180-0x183`、`0x200-0x203`），对每个信号的 4-bit 滚动计数器独立跟踪期望值 `(lastCounter + 1) & 0x0F`，不匹配时记录丢帧日志。控制帧和诊断帧跳过计数器校验。

3. **节点存活检测**（`rte.cpp:checkNodes`）：维护两个 `QElapsedTimer`（`m_nodeTimerA`/`m_nodeTimerB`），1Hz QTimer 检查距最后接收帧是否超过 3s，超时则 emit `nodeOffline(nodeId)`；恢复时 emit `nodeOnline(nodeId)`。

### 5.3 Mock 模拟模式

`Rte::setMode(Rte::MOCK)` 启动 50ms 定时器模拟 CAN 数据，档位按 P→R→D 循环切换，转速和油量呈三角波变化，温湿度周期性波动。Mock 模式下档位→车速存在物理约束关系：

```cpp
if (currentGear == CAN_GEAR_D)
    speedChanged(rpmToSpeed(rpm));        // D 档正常车速
else if (currentGear == CAN_GEAR_R)
    speedChanged(rpmToSpeed(rpm) / 4);    // R 档限速 1/4
else
    speedChanged(0);                      // P 档车速为 0
```

该逻辑模拟了真实车辆的传动特性：驻车档和空档时发动机转速与车速解耦。

### 5.4 执行器反馈闭环

节点 B 的风扇（直流电机 PWM）和车窗（舵机角度）执行器在收到控制指令后，由 actuator_task 将实际值记录到全局变量（`g_fan_actual_speed`/`g_window_actual_pos`），CAN 任务每 100ms 周期上报。主控端接收到 0x202/0x203 帧后更新 UI 显示——UI 展示的是执行器**实际状态**而非目标设定值，构成基础反馈闭环。

---

## 第 6 章 容错与诊断机制

### 6.1 故障模式与防护矩阵

| 故障模式 | 检测机制 | 响应行为 |
|---|---|---|
| STM32 节点断电/死机 | 3s 心跳超时（任一上报帧到达即复位） | UI 显示 "--"，触发 nodeOffline |
| 传感器物理损坏 | flags[0]=0 (传感器任务置位) | 触发 sensorFault, UI 不更新该值 |
| CAN 帧丢失 | 4-bit rolling counter 跳跃 | 日志记录 lost frame |
| CAN 控制器 BusOff | TEC/REC 监控 + AutoBusOff 恢复 | 诊断帧上报，日志告警 |
| DLC 不匹配（对端 bug） | kExpectedDlc 精确比对 | 丢弃帧 + 日志 |
| CAN 发送阻塞 | Com_Flush/Com_SendRaw 5ms 超时 + taskYIELD | 返回错误，下周期重试 |
| ISR 栈溢出 | DLC > 8 截断 | 安全截断为 8 字节 |

### 6.2 编译期防护

Qt 端 `config.h` 中使用 15 个 `static_assert` 进行编译期 CAN ID 值校验：

```cpp
static_assert(CAN_ID_A_ULTRASONIC  == 0x180, "CAN ID mismatch");
static_assert(CAN_ID_A_GEAR        == 0x181, "CAN ID mismatch");
// ... 共 15 个校验
```

若共享协议头文件中的宏值与编译期期望值不一致，编译器立即报错并明确指出哪个 ID 不匹配，杜绝了"协议悄悄变了一方不知道"的隐患。

---

## 第 7 章 测试与验证

### 7.1 DBC 工具链验证

使用 Python `cantools` 库加载 `smart_cockpit.dbc`，对模拟 CAN 日志进行解码测试：

```python
import cantools
db = cantools.database.load_file('smart_cockpit.dbc')
msg = db.encode_message('A_Gear', {'Flags': 0x11, 'Gear': 2})
# msg.frame_id = 0x181, msg.data = b'\x11\x02'
decoded = db.decode_message(0x181, b'\x11\x02')
# {'Flags': 17, 'Gear': 'D'}
```

验证了所有 15 个消息的编解码一致性，DBC 值与 C/C++ 代码完全对应。

### 7.2 总线负载分析

系统稳态帧统计：

| 来源 | 帧数/s | 单帧 bit 数 (含 stuffing) | 有效负载 bps |
|---|---|---|---|
| 节点A 传感器 (10Hz×4帧) | 40 | ~65 | ~2600 |
| 节点B 传感器+反馈 (10Hz×4帧) | 40 | ~65 | ~2600 |
| 诊断帧 (0.1Hz×2) | 0.2 | ~130 | ~26 |
| 合计 | ~80 | — | ~5226 |

250kbps 总线下，稳态负载约为 **2.1%**。考虑到 CAN 帧填充位（stuffing bits）增加约 15-20% 开销，实际负载约 **2.5%**，远低于 50% 安全阈值。

---

## 第 8 章 总结与展望

### 8.1 工作总结

本文完成了一套基于 CAN 总线的分布式智能座舱原型系统，主要贡献包括：

1. 设计了基于 bit-field 编码的 CAN ID 分配方案，将控制帧优先级置于上报帧之上，符合实时系统安全原则。
2. 参考 AUTOSAR E2E Profile 1 实现了有效性标志+滚动计数器+节点存活性检测的三层数据完整性保护。
3. 完善了 FreeRTOS 固件的容错能力，包括 CAN 发送超时机制、ISR 边界保护、BusOff 自动恢复和硬件滤波器优化。
4. 建立了 Qt 端数据校验管线（DLC 精确匹配→有效性检查→滚动计数器→心跳超时→诊断解析）。
5. 发布了标准 DBC 文件，实现与工业 CAN 工具链的兼容。

### 8.2 不足与展望

1. **执行器反馈精度**：当前反馈仅报告离散百分比（0-100），未实现连续物理量（如风扇实际 RPM）的 ADC 回读。可引入电机编码器或霍尔传感器实现更精确的反馈。
2. **E2E 完整性**：当前未实现 CRC 校验和，仅依赖 CAN 硬件 CRC。可增加应用层 CRC-8 覆盖 payload 和 counter，实现 AUTOSAR E2E Profile 1 的完整校验链。
3. **双 CAN 总线**：实际汽车中 PT-CAN（动力）和 Body-CAN（车身）为独立总线，通过网关互联。本系统使用单总线仿真，未体现网关路由和速率适配（动力域通常 500kbps，车身域 125kbps）。
4. **诊断 DTC 体系**：已实现 UDS 基础服务（0x10/0x22/0x3E/0x11），支持会话控制和按 ID 读数据。尚未建立完整的 ISO 15765 故障码数据库（DTC）、冻结帧（Freeze Frame）和诊断会话管理。可进一步扩展 0x19（读 DTC 信息）、0x14（清除 DTC）等服务。
5. **时间触发**：当前为纯事件触发+周期发送，未引入 CAN TTCAN 的时间触发机制，无法保证严格的任务同步和确定性延迟。

---

## 参考文献

[1] Robert Bosch GmbH. CAN Specification Version 2.0. 1991.
[2] ISO 11898-1:2015. Road vehicles — Controller area network (CAN) — Part 1: Data link layer and physical signalling.
[3] AUTOSAR. Specification of SW-C End-to-End Communication Protection Library. R21-11.
[4] STMicroelectronics. RM0008 Reference Manual: STM32F101xx/STM32F103xx advanced ARM-based 32-bit MCUs.
[5] Vector Informatik GmbH. DBC File Format Documentation.
[6] FreeRTOS. The FreeRTOS Kernel Developer Documentation. Real Time Engineers Ltd.
[7] Qingfeng Li, et al. "CAN Driver Design Based on FreeRTOS in Intelligent Vehicle." Journal of Physics: Conference Series, 2021.

---

## 附录 A CAN ID 完整分配表

| CAN ID | 位域 | 名称 | DLC | 周期 | 方向 |
|---|---|---|---|---|---|
| 0x080 | 001 0 0000000 | A_Buzzer_Ctrl | 1 | 按需 | 主控→A |
| 0x081 | 001 0 0000001 | A_LED_Ctrl | 1 | 按需 | 主控→A |
| 0x100 | 010 0 0000000 | B_Fan_Target_Speed | 1 | 按需 | 主控→B |
| 0x101 | 010 0 0000001 | B_Window_Target_Pos | 1 | 按需 | 主控→B |
| 0x102 | 010 0 0000010 | B_Buzzer_Ctrl | 1 | 按需 | 主控→B |
| 0x103 | 010 0 0000011 | B_LED_Ctrl | 1 | 按需 | 主控→B |
| 0x180 | 001 1 0000000 | A_Ultrasonic | 3 | 100ms | A→主控 |
| 0x181 | 001 1 0000001 | A_Gear | 2 | 100ms | A→主控 |
| 0x182 | 001 1 0000010 | A_Fuel | 3 | 100ms | A→主控 |
| 0x183 | 001 1 0000011 | A_RPM | 3 | 100ms | A→主控 |
| 0x200 | 010 1 0000000 | B_Temperature | 3 | 100ms | B→主控 |
| 0x201 | 010 1 0000001 | B_Humidity | 3 | 100ms | B→主控 |
| 0x202 | 010 1 0000010 | B_Fan_Speed_Actual | 2 | 100ms | B→主控 |
| 0x203 | 010 1 0000011 | B_Window_Pos_Actual | 2 | 100ms | B→主控 |
| 0x300 | 011 1 0000000 | DiagReport | 8 | 10s | A/B→主控 |

## 附录 B 档位编码

| 值 | 含义 | 设计依据 |
|---|---|---|
| 0x00 | P (Park) | 悬空/ADC异常默认驻车, 防御性编码 |
| 0x01 | R (Reverse) | 三档拨动开关中间位 |
| 0x02 | D (Drive) | 三档拨动开关另一端 |
