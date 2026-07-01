# 智能座舱 CAN 总线系统 — 企业级改造总结

> 本文档记录从"能跑的原型"到"具备企业级容错能力"的完整改造过程，可作为技术面试的案例素材。

## 改造前状态评估

三个核心问题：

1. **容错能力为零**：STM32 死机、传感器坏、总线断——Qt 端完全无感知，UI 显示僵尸数据
2. **CAN 协议定义三份拷贝**：STM32 的 `can_protocol.h`、Qt 的 `config.h`、共享头文件——已经提取了但被 IDE 反复还原
3. **20+ 处代码缺陷**：从死循环到栈溢出到队列消息静默丢弃

---

## 第一阶段：项目结构治理

### 1.1 清理 Git 仓库混乱状态

**问题**：451 个文件在 `stm32_can_node/Project/` 旧路径被追踪，工作目录中已被移出。Git 状态显示为 deleted+untracked 各一套，仓库无法正常 commit。

**修复**：`git rm -r stm32_can_node/Project/` 清理旧索引，`git add` 重新追踪新路径下的文件。

**面试话术**："这是一个典型的 EIDE 模板项目扁平化后的 Git 管理问题。本质是 `git mv` vs 直接文件系统操作的差异。教训是重命名操作必须通过 `git mv`，否则 Git 丢失 rename tracking。"

### 1.2 清理膨胀和遗留

| 问题 | 位置 | 操作 |
|---|---|---|
| `.cmsis/` 含 18 个无关 ARM 内核（4.8MB） | `stm32_can_node/.cmsis/` | 删除除 ARMCM3 外所有，节省 4.7MB |
| `smart_door_lock` 旧项目残留（构建产物+配置） | `build/smart_door_lock/`, `.eide/` | 删除 |
| 空文件 | `.eide/env.ini` | 删除 |
| 无关图片 | `docs/stm32_pcb/坤坤.png` | 删除 |
| 硬编码 WiFi 密码 | `Config/config.h` | 清除 |
| RTE 目录命名 | `MDK-ARM/RTE/_smart_door_lock` | 重命名为 `_stm32_can_node` |

### 1.3 缺失文件补齐

- `LICENSE`（MIT）
- `.editorconfig`（多编辑器协作）
- 根目录、子项目 README.md
- 统一工作区 `smart_cockpit.code-workspace`

---

## 第二阶段：CAN 协议统一

### 2.1 问题诊断

CAN ID 定义存在三份拷贝，任何修改需要同步三处：

```
stm32_can_node/Tasks/Inc/can_protocol.h  →  #define CAN_ID_A_STATUS 0x100
vehicle-terminal/src/config/config.h     →  constexpr uint32_t CAN_ID_A_STATUS = 0x100
shared/can_protocol_common.h             →  (新建，但被 IDE 反复删除)
```

而且命名不一致——STM32 叫 `CAN_ID_A_BUZZER_CTRL`，Qt 叫 `CAN_ID_A_BUZZER`（去掉了 `_CTRL` 后缀）。

### 2.2 解决方案

```
shared/can_protocol_common.h              ← 唯一权威源（纳入 Git 追踪）
    ├── #include "../../../shared/..."    ← STM32 用相对路径（不依赖 eide.yml）
    │   stm32_can_node/Tasks/Inc/can_protocol.h
    │
    └── #include "can_protocol_common.h"   ← Qt 用 CMake include path
        vehicle-terminal/src/config/config.h
        ├── static_assert × 14            ← 编译期验证 CAN ID 与共享头一致
        └── namespace Config { CAN_DEVICE, GPS_DEVICE ... }  ← 仅平台配置
```

**面试话术**："这是一个典型的 Single Source of Truth 重构。核心决策有三：第一，共享头文件用 `#define` 而非 `constexpr`，因为 STM32 是 C 语言；第二，STM32 端用相对路径 `../../../shared/` 而不依赖 eide.yml 的 incList，因为 IDE 插件会覆盖配置文件；第三，Qt 端用 14 个 `static_assert` 做编译期校验——如果有人不小心在 config.h 里又写了一份 CAN ID，编译立刻报错。"

### 2.3 Qt 端命名统一

旧命名 → 新命名（全部改为 STM32 端规范命名）：

| 旧 | 新 |
|---|---|
| `Config::CAN_ID_A_BUZZER` | `CAN_ID_A_BUZZER_CTRL` |
| `Config::CAN_ID_A_LED` | `CAN_ID_A_LED_CTRL` |
| `Config::CAN_ID_B_FAN_TARGET` | `CAN_ID_B_FAN_TARGET_SPEED` |
| `Config::CAN_ID_B_WINDOW_TARGET` | `CAN_ID_B_WINDOW_TARGET_POS` |
| `Config::CAN_ID_B_LED` | `CAN_ID_B_LED_CTRL` |
| `Config::CAN_BITRATE` | `CAN_BITRATE_HZ` |
| `Config::GEAR_D/P/R` | `CAN_GEAR_P=0/R=1/D=2`（防御性编码） |

### 2.4 协议完善：执行器反馈闭环

新增两个 CAN ID 实现风扇和车窗的反馈确认——主控发出目标值后，节点上报实际执行结果：

| CAN ID | 名称 | 方向 |
|---|---|---|
| 0x203 | `B_Fan_Speed_Actual` | 节点B→主控，风扇实际转速 |
| 0x204 | `B_Window_Pos_Actual` | 节点B→主控，车窗实际位置 |

STM32 的 actuator_task 收到控制指令后记录实际值到全局变量，CAN 任务每 100ms 周期上报。

### 2.5 工业标准工具链：DBC 文件

新增 `shared/smart_cockpit.dbc` — CAN Database 标准格式文件，任何 CAN 分析工具均可直接加载解码本项目报文。定义了所有 14 个消息的精确信号布局、物理单位和值表枚举。

---

## 第三阶段：CAN ID 分配重构

### 3.1 问题诊断

原方案 `bit[7]=0` 给 report、`bit[7]=1` 给 control。CAN 仲裁是 ID 越小优先级越高——导致**所有上报帧优先级高于所有控制帧**。倒车报警（控制帧 0x180）优先级低于转速数据（上报帧 0x100-0x104），这在汽车电子中是反直觉的——安全相关控制指令应优先于传感器数据。

另外，独立心跳帧 0x100/0x200 占用了全总线最高优先级却只是"我还活着"信号；诊断帧 0x400 不在 bit-field 方案内的任何已定义 node 范围。

### 3.2 新方案

```
bit[10:8] = node:  001=A, 010=B, 011=诊断/广播
bit[7]    = dir:   0=control(高优先级), 1=report(低优先级)
bit[6:0]  = signal id
```

| 范围 | 优先级 | 内容 |
|---|---|---|
| 0x080-0x0FF | **最高** | 主控→节点A 控制（蜂鸣器/LED） |
| 0x100-0x17F | 高 | 主控→节点B 控制（风扇/车窗/蜂鸣/LED） |
| 0x180-0x1FF | 中 | 节点A→主控 上报（超声波/档位/油量/转速） |
| 0x200-0x27F | 低 | 节点B→主控 上报（温湿度+执行器反馈） |
| 0x300-0x37F | 最低 | 诊断帧（两头都发，10s 周期） |

### 3.3 心跳机制简化

取消独立心跳帧（0x100/0x200）。Qt 端改为"任一上报帧到达即视为节点存活"——`isNodeAReport()`/`isNodeBReport()` 按 ID 范围判断，收到即 `m_lastRxA.restart()`。好处：不依赖单一帧类型，10Hz 数据帧自动充当 100ms 心跳，远优于原 1Hz 心跳。

### 3.4 档位编码防御性设计

P=0 而非 P=2。三档拨动开关接触不良或 ADC 悬空时读回 0 → 默认进入驻车档（最安全状态），避免"误读为空档导致溜车"。

---

## 第四阶段：代码缺陷修复

### 4.1 vehicle-terminal（11 处）

| # | 严重度 | 文件:行 | 问题 | 修复 |
|---|---|---|---|---|
| 1 | CRITICAL | `mapmanager.cpp:307` | `&&` 应为 `\|\|`——HTTP 4xx/5xx 不触发错误退出，被当作正常数据解析 | 改 `&&` 为 `\|\|` |
| 2 | CRITICAL | `rte.cpp:48-80` | Mock 油量发 0-100，homePage 按 ÷10 显示，值变成 1/10 | Mock 改为 `×10` 发送 |
| 3 | HIGH | `main.cpp:42` | `QTimer(0)` 在事件循环空闲时持续触发——等效于忙等 | 改为 500ms |
| 4 | HIGH | `ec20manager.cpp:865` | 串口缓冲区 `m_buffer` 无限增长 | 加 128KB 上限 |
| 5 | MEDIUM | `ec20manager.cpp:355-393` | `isDataConnected` / `getLocalIp` 双重循环完全重复 | 提取 `findActiveIPv4Address` 共享方法 |
| 6 | LOW | `weathermanager.cpp:41-70` | `onIpLocationFinished` 从未被调用（死代码） | 简化为空壳 |

### 4.2 stm32_can_node（19 处）

| # | 严重度 | 文件:行 | 问题 | 修复 |
|---|---|---|---|---|
| 1 | HIGH | `dht11_drv.c:113` | 校验和比较整数提升——合法数据误判为无效 | `(uint8_t)` 截断 |
| 2 | MEDIUM | `delay.c:34` | `delay_ms(0)` 回绕执行 42 亿次 | 加零值 guard |
| 3 | MEDIUM | `stm32f1xx_it.c:93` | HardFault 中调 `printf`（依赖 UART HAL + 堆栈） | 移除，仅保留 LED 闪烁 |
| 4 | MEDIUM | `oled_i2c_drv.c:229` | I2C 位延时 0→时钟超频到 10MHz+ | 设为 5μs |
| 5 | MEDIUM | `oled_i2c_drv.c:90` | 字符无范围检查→字体数组越界 | 加 `' '` ~ `'~'` 检查 |
| 6 | MEDIUM | `oled_i2c_drv.c:197` | UTF-8 跳过 3 字节可能越过 `\0` | 加 `text[1]`/`text[2]` 边界检查 |
| 7 | LOW | `monitor_task.c` | 全文件死代码 | 删除 |
| 8 | LOW | `usart.h:38` | 悬空 `extern DMA_HandleTypeDef` | 移除 |
| 9 | LOW | `config.h` | 硬编码 WiFi 密码/MQTT/门锁配置 | 清空为单行 `USE_EEPROM 0` |

### 4.3 预存编译问题修复

| 文件:行 | 问题 | 修复 |
|---|---|---|
| `stm32f1xx_hal_conf.h:45` | `HAL_DMA_MODULE_ENABLED` 被注释→`DMA_HandleTypeDef` 未定义（13 文件报错） | 取消注释 |
| `can.c:25` | 缺少 `#include "can_drv.h"`→`rx_callback` 未声明 | 添加 include |

---

## 第五阶段：P0 安全关键修复

### P0-A3：传感器有效标志位

**为什么**：DHT11 读失败时 STM32 发送上一次的旧值，Qt 端收到的是"2 秒前还有效的数据"而非"当前无效"。这在汽车电子中叫僵尸数据（Stale Data）。

**怎么做**：
1. 协议层：定义 `CAN_FLAG_VALID 0x01`，每个传感器帧 byte0 编码 flags
2. STM32：传感器任务成功时写 `g_xxx_valid=1`，失败时写 `0`；CAN 任务发送时写入 flags
3. Qt：收到帧先检查 `byte0 & CAN_FLAG_VALID`，无效则发 `sensorFault(canId)` 信号

**面试话术**："这个问题如果在车上发生——倒车雷达坏了但显示 150cm——可能造成事故。ISO 26262 对 ASIL-B 以上的系统有明确的数据新鲜度要求。我采用的方案是 AUTOSAR E2E Profile 1 的简化版：单字节编码 valid + counter，零额外帧开销。"

### P0-A1：心跳超时检测

**为什么**：STM32 断电后 Qt 端永远不知道。UI 上转速/温度等数据保持断电前最后一帧的值，用户看到的是假数据。

**怎么做**：`QElapsedTimer` 记录最后心跳时间，1Hz QTimer 检查是否超 3s，发 `nodeOnline`/`nodeOffline` 信号。

### P0-B1：can_send_safe 死循环

**为什么**：CAN 任务优先级最高(4)。CAN 控制器 BusOff 后邮箱永久不可用，`while(1) { delay_us(50); }` 不 yield——所有 prio≤4 的任务永久饿死，系统"死机"。

**怎么做**：限重试 100 次(≈5ms)，每 10 次 `taskYIELD()`。

### P0-B2：ISR 中 DLC>8 导致 memcpy 栈溢出

**为什么**：CAN DLC 字段可编码 9-15（CAN FD 模式）。恶意或噪声帧可产成 DLC>8 的值。`memcpy(msg.data, data, len)` 中 data 数组仅 8 字节 → 栈溢出。

**怎么做**：`if (len > 8) len = 8;`

### P0-B5：Qt 端 DLC 精确校验

**为什么**：原代码 `if (data.size() >= 2)` 是"最小长度检查"——对端发 4 字节但协议规定 2 字节时，静默读前 2 字节并忽略多余的。出错不告警，属于静默数据损坏。

**怎么做**：`kExpectedDlc` QHash 为每个 CAN ID 存精确期望 DLC，比对 `!=` 而非 `>=`。

---

## 第六阶段：P1/P2 可靠性提升

### P1-A2：滚动计数器

**为什么**：无序号或时间戳。接收方无法区分"最新的帧"和"因总线拥塞延迟的旧帧"。

**怎么做**：flags 字节高 4-bit 编码 4-bit rolling counter (0-15)。每个信号独立计数器，Qt 端 `verifyCounter()` 检测跳跃并打日志。

### P1-A6：CAN 错误监控

**为什么**：`AutoBusOff = DISABLE`→节点进 BusOff 后永不恢复；无错误回调→总线退化静默发生。

**怎么做**：
- `AutoBusOff = ENABLE`
- 注册 `HAL_CAN_ErrorCallback`→读 ESR 寄存器提取 TEC/REC→分类 error state
- 每 10s 通过 `CAN_ID_DIAGNOSTIC(0x300)` 上报

### P1-B5：精确滤波器

**为什么**：原过滤器接收总线所有 ID，ISR 中每次都要读 FIFO 再判断是否需要。STM32F103 的 CAN 只有 3 个邮箱+2 个 FIFO(各 3 帧)，多余的帧挤占缓冲区。

**怎么做**：
- 节点A：FilterId=0x080, MaskId=0x7FE（仅接收 0x080-0x081）
- 节点B：FilterId=0x100, MaskId=0x7FC（仅接收 0x100-0x103）

### P2：诊断帧

**为什么**：无 CAN 总线健康指标。排查问题时没有任何数据。

**怎么做**：`DiagReport_t`(8 字节) — node_id + error_state + TEC + REC + tx_msg_count + bus_load。Qt 端解析并 emit `canDiagReport` 信号。

---

## 面试经验总结

### 高频问题 + 回答模板

**Q: 你做过的项目中，最复杂的 bug 是什么？**

→ 讲 P0-B1（can_send_safe 死循环）："CAN 任务最高优先级，while(1) 里忙等 50μs 但不 yield。CAN 控制器 BusOff 后所有 ≤4 优先级的任务——包括 idle——全部饿死。现象是整个系统 OLED 停止刷新、按键无响应，看起来像硬件死机。根因是 FreeRTOS 优先级管理的边界 case——不是低优先级阻塞高优先级，而是最高优先级自己卡死自己。"

**Q: 你怎么保证分布式系统中节点间数据一致性？**

→ 讲 A1+A2+A3 组合："三层防护——第一，心跳超时检测，3s 收不到就标记节点离线；第二，滚动计数器，每个 CAN ID 独立递增，丢帧立即可检测；第三，传感器有效标志位，读失败时不发旧数据。这三层合起来就是 AUTOSAR E2E 的思想——data freshness + data integrity + node liveness。"

**Q: 你如何做协议设计？**

→ 讲第二阶段+第三阶段（协议统一+ID重构）："协议是分布式系统的合同。第一，我把所有 CAN ID 和数据结构提取到共享头文件，两端通过 include 引用，用 static_assert 做编译期校验。第二，CAN ID 分配用 bit-field 设计——bit[10:8]=node, bit[7]=direction, bit[6:0]=signal。特别把 bit[7] 设计为 0=control/1=report，这样控制帧的仲裁优先级永远高于传感器数据——倒车报警不会因为转速数据刷屏而被延迟。第三，取消了独立心跳帧，任一上报帧到达就是心跳，更鲁棒——不依赖单一帧类型。"

**Q: 企业级代码和原型代码的区别是什么？**

→ "原型代码假设一切正常——假设发送成功、假设传感器不坏、假设对端不死。企业级代码对每种已知故障模式都有显式的检测和处理路径。我做的 P0 五个修复就是五条故障路径：发送失败怎么办、传感器坏了怎么办、节点死了怎么办、收到畸形帧怎么办、数据长度不符怎么办。"

### 改造前后对比

| 维度 | 改造前 | 改造后 |
|---|---|---|
| CAN ID 一致性 | 3 份拷贝，命名不一致 | 1 份权威源 + 编译期验证 |
| CAN ID 优先级 | 上报优先(控制被延迟) | bit[7]=0控制优先(倒车报警>传感器) |
| 节点故障检测 | 无 | 上报帧到达=心跳(不依赖专用帧) |
| 数据新鲜度 | 无 | 4-bit rolling counter × 8 信号 |
| 传感器故障 | 发送旧值，静默 | flags 字段 + sensorFault 信号 |
| 执行器反馈 | 开环(主控不知执行结果) | CAN_ID_B_*_ACTUAL 反馈闭环 |
| CAN 错误监控 | 无 | TEC/REC + 诊断帧(10s, 0x300) |
| 发送鲁棒性 | 无限重试→可死锁 | 5ms 超时 + taskYIELD |
| 内存安全 | ISR memcpy 无保护 | DLC>8 截断 |
| 接收校验 | 最小长度检查 | 精确 DLC 匹配 |
| 滤波器 | 全接收 | 精确过滤(0x080/0x100 范围) |
| 工业工具链 | 无 | DBC 文件(SavvyCAN/cantools) |
| 档位编码 | P=2(悬空可能非驻车) | P=0(悬空默认驻车,防御性) |
| 死代码 | 20+ 处 | 已清理 |
