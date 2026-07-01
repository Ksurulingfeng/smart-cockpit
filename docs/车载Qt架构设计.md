# 车载 Qt 架构设计

> 讲解 vehicle-terminal 的软件架构设计——Meyer's 单例模式、RTE 虚拟功能总线、Mock 模式、信号-槽解耦。

---

## 一、架构总览

```
┌─────────────────────────────────────────────┐
│  UI 层  (ui/pages/)                         │
│  HomePage / ToolsPage / NavigationPage ...   │
│  ← 只通过 Qt 信号获取数据, 不直接读 CAN        │
├─────────────────────────────────────────────┤
│  RTE 层  (rte.h/cpp)                        │
│  模式切换 (CAN / Mock), 9 种业务信号发射       │
├─────────────────────────────────────────────┤
│  COM 层  (comlayer.h/cpp)                   │
│  传感器帧 counter 校验, 丢帧检测              │
├─────────────────────────────────────────────┤
│  Driver 层  (canmanager.h/cpp)              │
│  SocketCAN 连接管理, 原始帧收发               │
└─────────────────────────────────────────────┘
```

---

## 二、Meyer's 单例 —— 全局管理器模式

### 2.1 为什么用单例

车载终端有 8 个"全局唯一的系统服务"——CAN 管理器、4G 管理器、WiFi 管理器……如果每个页面各自创建，就会出现多个 SocketCAN 连接、多个串口实例，导致设备冲突。

### 2.2 标准模板

```cpp
// rte.h
class Rte : public QObject
{
    Q_OBJECT
public:
    static Rte *instance();                    // 唯一入口
    Rte(const Rte &) = delete;                 // 禁止拷贝
    Rte &operator=(const Rte &) = delete;

signals:
    void speedChanged(int kmh);
    // ...

private:
    explicit Rte(QObject *parent = nullptr);   // 构造器私有
};
```

```cpp
// rte.cpp
Rte *Rte::instance()
{
    static Rte inst;  // C++11 保证线程安全的一次性初始化
    return &inst;
}
```

**为什么不用全局变量**：单例保证构造时序——`static Rte inst` 在首次调用 `instance()` 时才构造，不是 `main()` 之前。这避免了 C++ 的 static initialization order fiasco。

### 2.3 本项目所有单例

| 类 | 职责 |
|---|---|
| `Rte` | CAN/Mock 模式切换 + 信号分发 |
| `ComLayer` | counter 校验 |
| `CanManager` | SocketCAN 管理 |
| `EC20Manager` | 4G 模块 (AT 指令 + GPS) |
| `ConfigManager` | QSettings 持久化 |
| `WifiManager` | NetworkManager D-Bus |
| `WeatherManager` | 和风天气 API |
| `VolumeManager` | ALSA 音量 |

---

## 三、RTE —— 虚拟功能总线

### 3.1 信号定义

Rte 把 CAN 数据和模拟数据抽象为统一的 **Qt 信号**：

```cpp
signals:
    void speedChanged(int kmh);          // 车速
    void rpmChanged(int rpmX10);         // 转速 (×10)
    void oilChanged(int percentX10);     // 油量 (×10)
    void gearChanged(int gear);          // 档位
    void distanceChanged(int cm);        // 超声波距离
    void temperatureChanged(int celsiusX10);  // 温度 (×10)
    void humidityChanged(int percentX10);     // 湿度 (×10)
    void fanSpeedChanged(int percent);   // 风扇实际转速
    void windowPosChanged(int percent);  // 车窗实际位置
    void nodeOnline(int nodeId);         // 节点上线
    void nodeOffline(int nodeId);        // 节点离线
```

### 3.2 UI 层如何使用

```cpp
// mainwindow.cpp
Rte *rte = Rte::instance();
connect(rte, &Rte::speedChanged, m_homePage, &HomePage::updateSpeed);
connect(rte, &Rte::temperatureChanged, m_homePage, &HomePage::updateTemperature);
connect(rte, &Rte::fanSpeedChanged, m_toolsPage, &ToolsPage::onFanCurSpeed);
```

HomePage 和 ToolsPage **完全不知道 CAN 的存在**——它们只接收 Qt 信号。这就是 AUTOSAR 虚拟功能总线的核心思想：SWC 只看 RTE 接口，不关心数据来源。

### 3.3 模式切换——Mock 的威力

```cpp
// rte.h
enum Mode { REAL_CAN, MOCK };

void setMode(Mode m);

// rte.cpp
void Rte::setMode(Mode m)
{
    if (m == m_mode) return;
    m_mode = m;
    ComLayer::instance()->resetCounters();   // 清 counter 状态
    m_mockStep = 0;
    if (m == MOCK) m_mockTimer->start(50);   // 50ms 模拟帧
    else           m_mockTimer->stop();
}
```

一键切换数据源——不需要重编译、不需要修改 UI 代码、不需要接 CAN 硬件。这在开发调试阶段价值巨大：在 x86 PC 上就能跑完整 UI 测试。

### 3.4 Mock 数据——物理约束

```cpp
void Rte::mockTick()
{
    // 档位按 P→R→D 循环切换
    if (m_mockStep % 100 == 0)
        m_mockGear = (m_mockGear == CAN_GEAR_P) ? CAN_GEAR_R :
                      (m_mockGear == CAN_GEAR_R) ? CAN_GEAR_D : CAN_GEAR_P;

    int rpm  = (20 + (m_mockStep % 80)) * 10;
    int speed = 0;
    if (m_mockGear == CAN_GEAR_D)      speed = rpmToSpeed(rpm);
    else if (m_mockGear == CAN_GEAR_R) speed = rpmToSpeed(rpm) / 4;
    // P 档: speed = 0

    emit speedChanged(speed);
    emit rpmChanged(rpm);
    emit gearChanged(m_mockGear);
}
```

Mock 实现了"P 档车速 0、R 档限速 1/4"的物理约束——模拟真实变速箱行为，不是简单随机数。

---

## 四、COM 层 —— 计数器校验

### 4.1 职责分离

```
Rte::onCanFrame  →  解析 CAN 帧, emit Qt 信号   (信号分发)
ComLayer::processCanFrame  →  counter 校验      (安全检测)
```

为什么要分开：Rte 处理"数据是什么"，ComLayer 处理"数据是否可信"。如果混在一起，Mock 模式也要走 counter 校验逻辑（Mock 没有 counter）。

### 4.2 校验逻辑

```cpp
void ComLayer::processCanFrame(uint32_t canId, const uint8_t *data, uint8_t len)
{
    // 仅传感器帧有 flags + counter
    bool isSensorFrame = (canId >= 0x180 && canId <= 0x183)
                      || (canId >= 0x200 && canId <= 0x203);
    if (!isSensorFrame) return;  // 控制帧/诊断帧跳过

    uint8_t flags = data[0];
    if (!m_counterInit[canId]) {
        m_counterInit[canId] = true;             // 首次: 初始化基准值
        m_lastCounter[canId] = CAN_GET_COUNTER(flags);
        return;
    }
    uint8_t counter = CAN_GET_COUNTER(flags);
    uint8_t expected = (m_lastCounter[canId] + 1) & 0x0F;
    m_lastCounter[canId] = counter;
    if (counter != expected)
        qDebug() << "[CAN] lost frame id=0x" << Qt::hex << canId;
}
```

---

## 五、CanManager —— SocketCAN 抽象

### 5.1 为什么用 SocketCAN

Linux 内核主线支持 CAN 总线，SocketCAN 是标准的用户态接口：

```cpp
// CanManager 内部
QCanBusDevice *device = QCanBus::instance()->createDevice("socketcan", "can0");
device->connectDevice();
```

上层代码不感知底层是物理 CAN 还是虚拟 vcan0——vcan0 可以在无硬件时做开发测试。

### 5.2 vcan0 快速搭建

```bash
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

之后所有 CAN 工具（candump、cansend）都可以操作 vcan0。

---

## 六、信号-槽解耦的完整数据流

```
CAN 帧到达 (SocketCAN)
  ↓ QCanBusDevice::framesReceived
CanManager::frameReceived(id, data, len)
  ↓ emit frameReceived
Rte::onCanFrame(id, data, len)
  ├─→ ComLayer::processCanFrame()     (counter 校验)
  ├─→ m_nodeTimerA/B.restart()       (心跳)
  └─→ switch(canId):
        emit speedChanged(int)         ← HomePage::updateSpeed
        emit temperatureChanged(int)   ← HomePage::updateTemperature
        emit fanSpeedChanged(int)      ← ToolsPage::onFanCurSpeed
        ...
```

每一个环节都是**单向信号流**——发送方不关心接收方是谁，接收方不关心中间处理过程。

---

## 七、总结

1. **Meyer's 单例** —— 全局唯一服务，C++11 线程安全，构造时序确定
2. **RTE 信号抽象** —— UI 层不碰 CAN，Mock 模式可一键切换数据源
3. **模式切换** —— `setMode(CAN/MOCK)` 零修改成本切换物理/模拟
4. **COM 层独立** —— counter 校验与信号解析职责分离
5. **SocketCAN** —— Linux 内核标准接口，vcan0 无硬件可开发

## 参考资料

- [rte.h](../vehicle-terminal/src/core/rte.h) / [rte.cpp](../vehicle-terminal/src/core/rte.cpp)
- [comlayer.h](../vehicle-terminal/src/core/comlayer.h) / [comlayer.cpp](../vehicle-terminal/src/core/comlayer.cpp)
- [Qt SerialBus - SocketCAN Plugin](https://doc.qt.io/qt-5/qtcanbus-backends.html#socketcan-plugin)
