# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

请始终使用简体中文思考和回答，并在回答时保持专业、简洁，除非代码或专有名词等特定需要。

## 构建命令

```bash
# x86 本地编译
LANG=zh_CN.UTF-8 cmake --preset x86-release
LANG=zh_CN.UTF-8 cmake --build build/x86-release -- -j$(nproc)

# ARM 交叉编译
LANG=zh_CN.UTF-8 cmake --preset arm-release -DBUILD_TESTS=ON
LANG=zh_CN.UTF-8 cmake --build build/arm-release --target vehicle-terminal -- -j$(nproc)

# 编译 EC20 测试程序
LANG=zh_CN.UTF-8 cmake --build build/x86-release --target test_ec20manager

# 运行（x86）
./build/x86-release/vehicle-terminal

# 运行 Mock GPS 测试（x86 室内调试）
./build/x86-release/test_ec20manager --mock

# 部署到 ARM 板（192.168.26.48）
./tools/deploy.sh
```

## 架构概览

### 单例管理器
`core/` 下所有 Manager 类（`VolumeManager`、`ConfigManager`、`WifiManager`、`WeatherManager`、`VehicleDataProcessor`、`CanManager`、`EC20Manager`）均使用 Meyer's 单例。`MapManager` 不是单例，由使用方按需创建。

### 主窗口页面切换
`MainWindow::setupPages()` 中一个 `QStackedWidget` 管理所有页面。`HomePage` 和 `MusicPage` 在构造时创建，`ToolsPage`、`NavigationPage`、`SettingsPage` 首次点击对应按钮时才懒加载。

### GPS → 地图坐标转换管线
```
EC20Manager::parseNmea()               // 解析 $GPRMC/$GPGGA (WGS84)
  → emit positionUpdated(lon, lat)
    → NavigationPage::onPositionUpdated
      → MapManager::requestCoordConv()  // 本地 WGS84→GCJ02→BD09，零 API 消耗
        → MapManager::requestStaticMapForCoords()
          → 百度 staticimage/v2 API    // 获取地图 PNG
            → NavigationPage::paintEvent() 渲染
```
默认位置：广州 (23.105210, 113.276236)。

### 百度地图 API（MapManager）
统一回调 `onReplyFinished()` 按 URL 关键词分发到 4 种 API 的解析逻辑：
- `staticimage/v2` — 获取 712×480 静态地图 PNG（检测 PNG 魔数校验有效性）
- `geocoding/v3` — 地址→坐标，tag="START"/"END" 串联导航流程
- `directionlite/v1/driving` — 驾车路径规划，每 5 点采样路径
- `geoconv/v2` — 坐标转换（已不用，改为本地 `wgs84ToBd09()` 算法）

### 静态图冷却
`STATIC_MAP_INTERVAL_MS = 5000`（5秒）。GPS 自动更新受冷却限制（`immediate=false`），用户操作（缩放/重置）传 `immediate=true` 绕过。冷却期间暂存最新坐标到 `m_pendingCenter`，定时器触发后发一次。

### GPS Mock 系统（EC20Manager）
- `loadNmeaFile(path)` — 加载预录 NMEA 日志
- `startMockGps(200)` — 每 200ms 逐行回放，循环输出
- 复用 `parseNmea()` 管道，消费者无需改动

### ConfigManager 持久化
使用 `QSettings`（IniFormat），存于可执行文件同目录的 `config.ini`。每次 `setValue()` 后立即 `sync()`。

## 代码风格

### 类结构
```cpp
class XxxManager : public QObject
{
    Q_OBJECT
public:
    static XxxManager *instance();                    // 单例入口
    XxxManager(const XxxManager &)            = delete;
    XxxManager &operator=(const XxxManager &) = delete;

    // 公开方法 ...

signals:
    void someSignal(...);                             // signals 在 public slots 之前
    void errorOccurred(const QString &error);         // 统一错误信号

private slots:
    void onInternalEvent();

private:
    explicit XxxManager(QObject *parent = nullptr);   // 构造器私有
    ~XxxManager() override;                           // 析构 override（空体用 = default）

    // 成员用 in-class 默认值，不在初始化列表中重复
    bool m_flag = false;
    QTimer *m_timer = nullptr;
};
```

要点：
- **单例**：构造器私有 + `= delete` 拷贝/赋值 + `instance()` 返回引用
- **信号排序**：`signals:` 在 `public slots:` 之前
- **错误信号**：统一用 `errorOccurred(const QString &error)`
- **析构函数**：所有 `QObject` 子类析构加 `override`，空体直接 `= default`
- **成员初始化**：in-class 默认值优先，避免初始器列表冗余
- **格式化**：使用 `/home/hajimi/tools/.clang-format`（BasedOnStyle: Microsoft, 4空格, Linux大括号, 等号对齐）

## 关键约定
- 坐标转换 WGS84→BD09 使用本地算法（`wgs84ToGcj02` + `gcj02ToBd09`），不调百度 API
- 百度 AK 优先读环境变量 `BAIDU_AK`，回退到 `Config::BAIDU_AK`
- 模拟和真实 GPS 都通过 `EC20Manager::positionUpdated(lon, lat)` 信号输出，坐标格式为 (经度, 纬度)
- `MapManager` 内部存储坐标为 `lon,lat` 格式；`m_startLatLon` 等对外的坐标字符串为 `lat,lon` 格式，需注意转换
- `NavigationPage` 的默认 GPS 坐标定义在文件顶部 `DEFAULT_LAT/DEFAULT_LON` 常量
