# Plan: Lu-03t 语音识别模组集成

## Context
为车载终端项目添加离线语音交互，使用 Lu-03t（VC-02 芯片）+ 安信可语音开放平台 (voice.ai-thinker.com)。ASR + TTS，离线优先。

---

## 一、平台声学配置

| 配置项 | 选择 | 理由 |
|--------|------|------|
| 识别距离 | **近场 0-1m** | 座舱内驾驶员距 MIC ≤80cm，近场可降低路噪/风噪/乘客交谈干扰 |
| AEC 回声消除 | **开启** | 音乐播放和 TTS 应答语会通过喇叭输出，不开会导致误唤醒死循环 |
| 稳态降噪 | **开启** | 引擎声、胎噪、风噪、空调风扇声均为稳态噪声，不开严重影响高速行驶时识别率 |

---

## 二、唤醒词 & 唤醒回复

### 唤醒词：`小驰小驰`

### 唤醒回复（平台端配置）

| 场景 | 应答语 |
|------|--------|
| 首次唤醒 | 你好，有什么可以帮你？ |
| 短时再唤醒 | 请讲 |
| 超时退出 | 有需要随时叫我 |

### 唤醒超时：8 秒

---

## 三、免唤醒命令词（10 条）

以下命令无需先说"小驰小驰"，直接说出即可触发：

| 序号 | 命令词 | 英文指令 | ID | 免唤醒理由 |
|------|--------|----------|-----|------------|
| 1 | 增大音量 | `CMD_VOL_UP` | 0x0C | 驾驶最高频操作 |
| 2 | 减小音量 | `CMD_VOL_DOWN` | 0x0D | 同上 |
| 3 | 静音 | `CMD_VOL_MUTE` | 0x0E | 突发需立即静音 |
| 4 | 取消静音 | `CMD_VOL_UNMUTE` | 0x0F | 恢复音频 |
| 5 | 下一首 | `CMD_MUSIC_NEXT` | 0x0A | 高频切歌，短语独特不易误触发 |
| 6 | 上一首 | `CMD_MUSIC_PREV` | 0x0B | 同上 |
| 7 | 暂停音乐 | `CMD_MUSIC_PAUSE` | 0x09 | 需集中驾驶时快速静音 |
| 8 | 播放音乐 | `CMD_MUSIC_PLAY` | 0x08 | 恢复播放 |
| 9 | 放到最大 | `CMD_ZOOM_MAX` | 0x04 | 快速查看地图细节 |
| 10 | 放到最小 | `CMD_ZOOM_MIN` | 0x05 | 查看全局路线 |

---

## 四、完整命令词配置表（平台导入格式）

### 1. 导航类 (0x01-0x07)

| ID | 英文指令 | 命令词（\| 分隔多说法） | 应答语 | 触发动作 |
|----|----------|-------------------------|--------|----------|
| 0x01 | `CMD_NAV_START` | 导航到目的地 \| 去目的地 | 正在规划路线 | startNavigate() |
| 0x02 | `CMD_NAV_HOME` | 回家 \| 导航回家 \| 导航到家 | 正在规划回家路线 | 导航到预设家地址 |
| 0x03 | `CMD_NAV_COMPANY` | 去公司 \| 导航去公司 \| 回公司 | 正在规划去公司的路线 | 导航到预设公司地址 |
| 0x04 | `CMD_ZOOM_MAX` | 放到最大 | 好的 | setZoom(19) |
| 0x05 | `CMD_ZOOM_MIN` | 放到最小 | 好的 | setZoom(3) |
| 0x06 | `CMD_MAP_RESET` | 重置地图 \| 重置 | 已重置地图 | resetView() |
| 0x07 | `CMD_OPEN_NAV` | 查看导航 \| 打开导航 | 已打开导航 | 切换到 NavigationPage |

### 2. 音乐类 (0x08-0x0F)

| ID | 英文指令 | 命令词（\| 分隔多说法） | 应答语 | 触发动作 |
|----|----------|-------------------------|--------|----------|
| 0x08 | `CMD_MUSIC_PLAY` | 播放音乐 | 好的 | onPlayPause() |
| 0x09 | `CMD_MUSIC_PAUSE` | 暂停音乐 | 已暂停 | onPlayPause() |
| 0x0A | `CMD_MUSIC_NEXT` | 下一首 | — | onNext() |
| 0x0B | `CMD_MUSIC_PREV` | 上一首 | — | onPrevious() |
| 0x0C | `CMD_VOL_UP` | 增大音量 | 好的 | volume +10 |
| 0x0D | `CMD_VOL_DOWN` | 减小音量 | 好的 | volume -10 |
| 0x0E | `CMD_VOL_MUTE` | 静音 | 已静音 | volume → 0 |
| 0x0F | `CMD_VOL_UNMUTE` | 取消静音 | 已恢复 | volume 恢复 |

### 3. 系统控制类 (0x10-0x13)

| ID | 英文指令 | 命令词（\| 分隔多说法） | 应答语 | 触发动作 |
|----|----------|-------------------------|--------|----------|
| 0x10 | `CMD_OPEN_MUSIC` | 打开音乐 | 已打开音乐 | 切换到 MusicPage |
| 0x11 | `CMD_SETTINGS` | 打开设置 | 已打开设置 | 切换到 SettingsPage |
| 0x12 | `CMD_OPEN_CAMERA` | 打开倒车影像 | 已打开倒车影像 | 切换到 RearViewCamera |
| 0x13 | `CMD_GO_HOME` | 返回首页 | 好的 | 切换到 HomePage |

### 4. 天气类 (0x14-0x15) — 动态播报待实现

| ID | 英文指令 | 命令词 | 应答语 | 触发动作 |
|----|----------|--------|--------|----------|
| 0x14 | `CMD_WEA_TODAY` | 今天天气 | (动态合成) | WeatherManager → TTS |
| 0x15 | `CMD_WEA_TOMOR` | 明天天气 | (动态合成) | WeatherManager → TTS |

### 5. 车控类 (0x16-0x19) — 稍后在 ToolsPage 实现

| ID | 英文指令 | 命令词（\| 分隔多说法） | 应答语 | 硬件 |
|----|----------|-------------------------|--------|------|
| 0x16 | `CMD_AC_ON` | 打开空调 \| 开空调 | 已打开空调 | 电机正转 |
| 0x17 | `CMD_AC_OFF` | 关闭空调 \| 关空调 | 已关闭空调 | 电机停转 |
| 0x18 | `CMD_WIN_OPEN` | 打开车窗 \| 开窗 | 已打开车窗 | 舵机开窗 |
| 0x19 | `CMD_WIN_CLOSE` | 关闭车窗 \| 关窗 | 已关闭车窗 | 舵机关窗 |

---

## 五、应答语分层策略

| 层级 | 实现方式 | 适用场景 | 占比 |
|------|----------|----------|------|
| **A 层·模组本地** | 平台为每个命令词配置 MP3 应答，模组直接喇叭播放 | "好的""已暂停""已打开空调" 等固定短语 | ~80% |
| **B 层·主控动态** | VoiceManager 触发 → Qt 端查询数据 → 拼接文本 → TTS | "今天[天气]，温度[XX]度" 等动态内容 | ~20% |

---

## 六、VoiceCommand 枚举（voicemanager.h）

```cpp
enum class VoiceCommand {
    // 导航类 (0x01-0x07)
    CMD_NAV_START   = 0x01,
    CMD_NAV_HOME    = 0x02,
    CMD_NAV_COMPANY = 0x03,
    CMD_ZOOM_MAX    = 0x04,
    CMD_ZOOM_MIN    = 0x05,
    CMD_MAP_RESET   = 0x06,
    CMD_OPEN_NAV    = 0x07,
    // 音乐类 (0x08-0x0F)
    CMD_MUSIC_PLAY  = 0x08,
    CMD_MUSIC_PAUSE = 0x09,
    CMD_MUSIC_NEXT  = 0x0A,
    CMD_MUSIC_PREV  = 0x0B,
    CMD_VOL_UP      = 0x0C,
    CMD_VOL_DOWN    = 0x0D,
    CMD_VOL_MUTE    = 0x0E,
    CMD_VOL_UNMUTE  = 0x0F,
    // 系统控制类 (0x10-0x13)
    CMD_OPEN_MUSIC  = 0x10,
    CMD_SETTINGS    = 0x11,
    CMD_OPEN_CAMERA = 0x12,
    CMD_GO_HOME     = 0x13,
    // 天气类 (0x14-0x15)
    CMD_WEA_TODAY   = 0x14,
    CMD_WEA_TOMOR   = 0x15,
    // 车控类 (0x16-0x19)
    CMD_AC_ON       = 0x16,
    CMD_AC_OFF      = 0x17,
    CMD_WIN_OPEN    = 0x18,
    CMD_WIN_CLOSE   = 0x19,
};
```

---

## 七、修改文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/config/config.h` | 已改 | VOICE_DEVICE / VOICE_BAUDRATE 常量 |
| `src/core/voicemanager.h` | 已创建 | VoiceCommand 枚举 + VoiceManager 单例 |
| `src/core/voicemanager.cpp` | 已创建 | 串口通信 + 协议帧解析 + 唤醒超时 |
| `src/ui/pages/mainwindow.cpp` | 已改 | 初始化 + 全局命令注册 + 导航命令懒注册 |
| `src/ui/pages/navigationPage.h` | 已改 | zoomMax/zoomMin/resetView/startNavigate 公开方法 |
| `src/ui/pages/navigationPage.cpp` | 已改 | zoomMax/zoomMin 实现 |
| `CMakeLists.txt` | 无需改 | Qt5::SerialPort 已链接 |

---

## 八、硬件接线

```
i.MX6ULL              Lu-03t
─────────────────────────────
UART3_TXD (J12-7)  →  B6 (UART1_RXD)
UART3_RXD (J12-8)  ←  B7 (UART1_TXD)
5V                  →  VCC
GND                 →  GND
                    →  MIC+ / MIC- → 驻极体麦克风
                    →  SPK+ / SPK- → 4Ω 3W 喇叭
```

## 九、验证步骤

1. 在安信可语音开放平台创建产品 → 按第四节的命令词配置表逐条录入 → 标记第三节的 10 条为免唤醒 → 生成 .bin 固件并烧录
2. 编译：`cmake --preset arm-release && cmake --build build/arm-release -- -j$(nproc)`
3. 上板验证：唤醒 → 命令识别 → 串口帧 → VoiceManager 解析 → 业务动作 → 应答
4. 根据 Lu-03t 实际串口协议微调 `parseFrame()` 帧解析逻辑
