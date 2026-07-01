# 车载 Linux 终端应用——集中式人机交互与管控层

基于 Qt 5.15 的车载智能座舱终端，运行在 ARM Linux (Yocto/Poky) 平台，通过 SocketCAN 与下位机通信。集成导航、4G 联网、WiFi、天气、音乐、倒车影像、CAN 调试功能。具备企业级 CAN 数据处理能力。

## 功能

| 页面 | 功能 |
|---|---|
| **主页** | 车速/转速/油量仪表盘；温湿度/天气；节点在线状态 |
| **导航** | 百度地图静态图 + 路径规划；GPS 定位；WGS84→BD09 |
| **音乐** | 本地 MP3 播放 |
| **倒车影像** | V4L2 摄像头；超声波距离叠加；低距离蜂鸣器+LED 报警 |
| **设置** | WiFi 管理；CAN/串口助手；GPIO 测试 |
| **工具** | CAN 协议调试；风扇/车窗/蜂鸣/LED 手动控制 |

## CAN 数据处理

| 层 | 组件 | 职责 |
|---|---|---|
| RTE | `Rte` 单例, 9 种业务信号 | 模式切换 CAN/Mock, CAN帧解析→Qt信号发射 |
| COM | `ComLayer` 单例 | 传感器帧 counter 校验, 丢帧日志 |
| Driver | `CanManager` | SocketCAN 收发 |

| 能力 | 实现 |
|---|---|
| 心跳检测 | 任一上报帧到达=节点存活, 3s 超时→nodeOffline 信号 |
| 有效标志 | Rte::onCanFrame 检查 byte0 flags[0]=valid |
| 丢帧检测 | ComLayer 4-bit rolling counter 独立跟踪, 跳跃时日志告警 |
| 执行器反馈 | 接收 0x202/0x203 实际值, 对比目标值形成闭环 |
| 诊断监控 | 0x300 DiagReport 解析, emit canDiagReport 信号 |
| Mock 模式 | Rte::setMode(MOCK), 模拟档位-车速关系: D档=正常, R档=限速1/4, P档=0 |

## 构建

| 环境 | 命令 |
|---|---|
| x86 编译 | `cmake --preset x86-release && cmake --build build/x86-release -- -j$(nproc)` |
| ARM 交叉编译 | `cmake --preset arm-release && cmake --build build/arm-release --target vehicle-terminal -- -j$(nproc)` |
| 部署 | `./tools/deploy.sh` (SCP→192.168.26.48) |

依赖: Qt 5.15+ (Widgets/VirtualKeyboard/Network/SerialPort/SerialBus/Multimedia), CMake 3.10+, Yocto Poky SDK

## 架构

### 单例管理器层 (src/core/)

| 管理器 | 职责 |
|---|---|
| `CanManager` | SocketCAN/USB-CAN 收发 |
| `Rte` | CAN/Mock 模式切换+信号分发+节点存活检测 |
| `ComLayer` | 传感器帧 counter 校验+丢帧检测 |
| `EC20Manager` | 4G 模块 (NMEA GPS + Mock 回放) |
| `MapManager` | 百度地图 API (静态图/路径规划/地理编码) |
| `WifiManager` | WiFi 扫描/连接 (NetworkManager D-Bus) |
| `WeatherManager` | 和风天气 API |
| `VolumeManager` | ALSA 音量控制 |
| `BacklightControl` | sysfs PWM 背光 |

### 主窗口

`QStackedWidget` 管理页面。HomePage/MusicPage 构造时创建, ToolsPage/NavigationPage/SettingsPage 懒加载。

### GPS→地图管线

```
EC20Manager::parseNmea() → emit positionUpdated(lon,lat)
  → NavigationPage → MapManager::requestCoordConv() (本地 WGS84→BD09)
    → 百度 staticimage/v2 → paintEvent 渲染
```

## 目录结构

```
vehicle-terminal/
├── src/
│   ├── config/           # 平台配置 (config.h), QSettings 持久化
│   ├── core/             # 单例管理器
│   ├── ui/forms/         # Qt Designer .ui
│   ├── ui/pages/         # Home/Music/Nav/Settings/Tools/RearView
│   ├── ui/widgets/       # SwitchButton 等
│   ├── resources/        # 字体/图片/音乐/视频/样式
│   └── utils/            # IconHelper
├── tests/                # test_ec20manager
├── tools/                # deploy.sh, toolchain.cmake
├── CMakeLists.txt
└── CMakePresets.json
```

## 关键约定

- 百度 AK: 优先环境变量 `BAIDU_AK`, 回退 `Config::BAIDU_AK`
- `MapManager` 内部 `lon,lat`, 对外 `lat,lon`
- ARM: CAN=`can0` GPS=`/dev/ttymxc2` Camera=`/dev/video1`
- x86: CAN=`vcan0` GPS=`/dev/ttyUSB0` Camera=`/dev/video1`

## CAN 协议

参见 [shared/can_protocol_common.h](../shared/can_protocol_common.h) 和 [DBC](../shared/smart_cockpit.dbc)。
