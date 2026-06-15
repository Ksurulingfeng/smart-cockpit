#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

namespace Config
{
    //--------------- 档位编码 ---------------
    constexpr int GEAR_D = 0x01; // D 档（前进）
    constexpr int GEAR_P = 0x02; // P 档（空档）
    constexpr int GEAR_R = 0x03; // R 档（倒车）

    //--------------- CAN 配置 ---------------
#if __arm__
    constexpr const char *CAN_DEVICE = "can0"; // CAN 设备
#else
    constexpr const char *CAN_DEVICE = "vcan0"; // CAN 设备
#endif
    constexpr int CAN_BITRATE = 250000; // CAN 波特率

    // ============ PT-CAN（节点A - 动力域）============
    // 节点A → 主控（上报）
    constexpr uint32_t CAN_ID_A_STATUS     = 0x100; // 心跳+按键
    constexpr uint32_t CAN_ID_A_ULTRASONIC = 0x101; // 超声波距离 (uint16 cm)
    constexpr uint32_t CAN_ID_A_GEAR       = 0x102; // 档位 (1=D,2=P,3=R)
    constexpr uint32_t CAN_ID_A_FUEL       = 0x103; // 油量 0-100%
    constexpr uint32_t CAN_ID_A_RPM        = 0x104; // 转速 0-100%
    // 主控 → 节点A（控制）
    constexpr uint32_t CAN_ID_A_BUZZER = 0x180; // 蜂鸣器模式
    constexpr uint32_t CAN_ID_A_LED    = 0x181; // LED 状态

    constexpr uint8_t CAN_BUZZER_OFF   = 0x00; // 蜂鸣器关
    constexpr uint8_t CAN_BUZZER_ALERT = 0x03; // 蜂鸣器报警
    constexpr uint8_t CAN_LED_OFF      = 0x00; // LED 关
    constexpr uint8_t CAN_LED_ALERT    = 0x03; // LED 报警闪烁

    // ============ Body-CAN（节点B - 车身域）============
    // 节点B → 主控（上报）
    constexpr uint32_t CAN_ID_B_STATUS         = 0x200; // 心跳+按键
    constexpr uint32_t CAN_ID_B_TEMPERATURE    = 0x201; // 温度×10 (int16, 例:255=25.5°C)
    constexpr uint32_t CAN_ID_B_HUMIDITY       = 0x202; // 湿度×10 (uint8, 例:655=65.5%)
    // 主控 → 节点B（控制）
    constexpr uint32_t CAN_ID_B_FAN_TARGET    = 0x280; // 风扇目标转速 0-100%
    constexpr uint32_t CAN_ID_B_WINDOW_TARGET = 0x281; // 车窗目标位置 0-100%
    constexpr uint32_t CAN_ID_B_BUZZER        = 0x282; // 蜂鸣器模式
    constexpr uint32_t CAN_ID_B_LED           = 0x283; // LED 状态

    //--------------- GPS 配置 ---------------
#if __arm__
    constexpr const char *GPS_DEVICE = "/dev/ttymxc2";
#else
    constexpr const char *GPS_DEVICE = "/dev/ttyUSB0";
#endif

    //--------------- 倒车影像配置 ---------------
    constexpr const char *RVC_DEVICE = "/dev/video1"; // 倒车摄像头设备
    constexpr int RVC_WIDTH          = 640;           // 倒车摄像头宽度
    constexpr int RVC_HEIGHT         = 480;           // 倒车摄像头高度
    constexpr int RVC_WARN_DIST_CM   = 10;            // 倒车预警距离阈值（厘米）

    //--------------- 多媒体配置 ---------------
#if __arm__
    constexpr const char *MUSIC_DIR_SUFFIX = "/media/music";  // 车载音乐播放器目录路径后缀
    constexpr const char *VIDEO_DIR_SUFFIX = "/media/videos"; // 车载视频播放器目录路径后缀
#else
    constexpr const char *MUSIC_DIR_SUFFIX = "/../../src/resources/media/music";  // 车载音乐播放器目录路径后缀
    constexpr const char *VIDEO_DIR_SUFFIX = "/../../src/resources/media/videos"; // 车载视频播放器目录路径后缀
#endif

    //--------------- 联网配置 ---------------
    constexpr const char *BAIDU_AK = "efGxC8pObnTryR2pd9s9qVR6s5NjUDsc";
}

#endif // CONFIG_H