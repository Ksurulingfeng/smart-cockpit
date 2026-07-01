<!-- zh-helper:start -->

## 语言设置

- **始终使用简体中文回复。**
- 代码标识符（变量名、函数名、类名）保持英文原样。
- 代码注释：解释性注释使用中文。
- Git commit message：使用中文，格式为 `<type>: <中文描述>`。

<!-- zh-helper:end -->

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

基于 CAN 总线的分布式智能座舱系统，由两个独立子项目和一个硬件目录组成：

| 目录 | 定位 | 平台 | 构建系统 |
|---|---|---|---|
| [stm32_can_node/](stm32_can_node/) | CAN 总线仿真固件（双节点） | STM32F103C8Tx, FreeRTOS | EIDE (Keil AC6) |
| [vehicle-terminal/](vehicle-terminal/) | 车载 Linux 终端应用 | ARM Linux (Yocto/Poky), Qt 5.15 | CMake + Ninja |
| [docs/stm32_pcb/](docs/stm32_pcb/) | STM32 CAN 节点 PCB 设计文件 | 立创 EDA | — |

两个子项目各自有独立的 `.claude/CLAUDE.md`，包含详细的构建命令、架构说明和代码风格。根目录的 CLAUDE.md 仅描述跨项目关系和系统级架构。

## 系统架构

```
┌────────────────────────────────────────────────────────┐
│  vehicle-terminal (ARM Linux, Qt 5.15)                  │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────────┐    │
│  │ 导航  │ │ 音乐  │ │ 倒车  │ │ 设置  │ │ 调试工具  │    │
│  ├──────┴─┴──────┴─┴──────┴─┴──────┴─┴──────────┤    │
│  │           单例管理器层 (core/)                     │    │
│  │  CanManager │ EC20Manager │ WifiManager │ ...       │    │
│  │  MapManager │ WeatherManager │ VolumeManager  │    │
│  └──────────────────────┬───────────────────────────┘    │
│                         │ CAN (SocketCAN / USB-CAN)      │
└─────────────────────────┼──────────────────────────────┘
                          │
┌─────────────────────────┼──────────────────────────────┐
│  stm32_can_node (FreeRTOS, CAN 250kbps)                │
│  ┌─────────┐                    ┌─────────┐             │
│  │ 节点 A   │ ←── CAN Bus ──→  │ 节点 B   │             │
│  │ 动力域   │                    │ 车身域   │             │
│  │ 发 0x180 │                    │ 发 0x200 │             │
│  │ 收 0x080 │                    │ 收 0x100 │             │
│  │ +0x7E0  │                    │ +0x7E0  │             │
│  └─────────┘                    └─────────┘             │
└─────────────────────────────────────────────────────────┘
```

- **vehicle-terminal** 是主控端，运行在 ARM Linux 车机上，通过 `CanManager` 经 SocketCAN 或 USB-CAN 与 **stm32_can_node** 通信
- **stm32_can_node** 通过 `#ifdef NODE_A` / `#ifdef NODE_B` 编译为两套固件，在两块 STM32F103 板子上运行，模拟动力域（转速、油量、档位、超声波）和车身域（温湿度、风扇、车窗）。均支持 UDS 诊断协议（0x7E0/0x7E8，ISO 14229-1）
- CAN 协议以 [tools/can_signals.json](tools/can_signals.json) 为单一数据源，运行 `python tools/dbc_generate_header.py` 自动生成 [shared/can_protocol_common.h](shared/can_protocol_common.h) 和 DBC 文件，双方共享

## 跨项目开发流程

### 修改 CAN 协议
1. 编辑 [tools/can_signals.json](tools/can_signals.json) 定义新的 CAN ID 和信号布局
2. 运行 `python tools/dbc_generate_header.py` 重新生成共享头文件和 DBC
3. 更新 stm32_can_node 对应任务的发送/接收逻辑（调用 Com_SendSignal/Com_ReceiveSignal）
4. 同步更新 vehicle-terminal 端 Rte::onCanFrame 中的解析代码
5. 两端各自编译部署

### 部署流程
- **stm32_can_node**: VS Code EIDE 任务 `eide.project.build` → `eide.project.uploadToDevice`（CMSIS-DAP 烧录）
- **vehicle-terminal**: ARM 交叉编译后 `./tools/deploy.sh` 通过 SCP 部署到 192.168.26.48，并自动运行

## 源码换行符规范

见 [.gitattributes](.gitattributes)：
- 所有 C/C++、脚本、配置、Qt 文件统一 **LF** 入库
- **仅** Keil MDK 工程文件（`.uvprojx`、`.uvoptx`）使用 **CRLF**，因为 Keil IDE 依赖 Windows 换行符
