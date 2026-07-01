/**
 * @file    config.h
 * @brief   vehicle-terminal 平台配置
 *
 * CAN 协议定义（CAN ID、数据结构）统一来自 shared/can_protocol_common.h。
 * 本文件仅包含平台相关配置（设备路径等）。
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include "can_protocol_common.h"

namespace Config
{
    //--------------- CAN 配置 ---------------
    // CAN ID 等协议常量统一由 shared/can_protocol_common.h 以宏形式提供
#if __arm__
    constexpr const char *CAN_DEVICE = "can0";
#else
    constexpr const char *CAN_DEVICE = "vcan0";
#endif

    //--------------- GPS 配置 ---------------
#if __arm__
    constexpr const char *GPS_DEVICE = "/dev/ttymxc2";
#else
    constexpr const char *GPS_DEVICE = "/dev/ttyUSB0";
#endif

    //--------------- 倒车影像配置 ---------------
    constexpr const char *RVC_DEVICE = "/dev/video1";
    constexpr int RVC_WIDTH          = 640;
    constexpr int RVC_HEIGHT         = 480;
    constexpr int RVC_WARN_DIST_CM   = 10;

    //--------------- 多媒体配置 ---------------
#if __arm__
    constexpr const char *MUSIC_DIR_SUFFIX = "/media/music";
    constexpr const char *VIDEO_DIR_SUFFIX = "/media/videos";
#else
    constexpr const char *MUSIC_DIR_SUFFIX = "/../../src/resources/media/music";
    constexpr const char *VIDEO_DIR_SUFFIX = "/../../src/resources/media/videos";
#endif

    //--------------- 联网配置 ---------------
    constexpr const char *BAIDU_AK = "efGxC8pObnTryR2pd9s9qVR6s5NjUDsc";
}

// ============ 编译期验证：确保与 shared/can_protocol_common.h 一致 ============
// bit[10:8]=node, bit[7]=dir(0=control优先), bit[6:0]=signal
static_assert(CAN_ID_A_BUZZER_CTRL == 0x080, "CAN ID mismatch");
static_assert(CAN_ID_A_LED_CTRL    == 0x081, "CAN ID mismatch");
static_assert(CAN_ID_A_ULTRASONIC  == 0x180, "CAN ID mismatch");
static_assert(CAN_ID_A_GEAR        == 0x181, "CAN ID mismatch");
static_assert(CAN_ID_A_FUEL        == 0x182, "CAN ID mismatch");
static_assert(CAN_ID_A_RPM         == 0x183, "CAN ID mismatch");
static_assert(CAN_ID_B_FAN_TARGET_SPEED  == 0x100, "CAN ID mismatch");
static_assert(CAN_ID_B_WINDOW_TARGET_POS == 0x101, "CAN ID mismatch");
static_assert(CAN_ID_B_BUZZER_CTRL == 0x102, "CAN ID mismatch");
static_assert(CAN_ID_B_LED_CTRL    == 0x103, "CAN ID mismatch");
static_assert(CAN_ID_B_TEMPERATURE       == 0x200, "CAN ID mismatch");
static_assert(CAN_ID_B_HUMIDITY          == 0x201, "CAN ID mismatch");
static_assert(CAN_ID_B_FAN_SPEED_ACTUAL  == 0x202, "CAN ID mismatch");
static_assert(CAN_ID_B_WINDOW_POS_ACTUAL == 0x203, "CAN ID mismatch");
static_assert(CAN_ID_DIAGNOSTIC   == 0x300, "CAN ID mismatch");

#endif // CONFIG_H
