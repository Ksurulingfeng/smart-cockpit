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
static_assert(CAN_ID_PCM_BUZZERCMD == 0x080, "CAN ID mismatch");
static_assert(CAN_ID_PCM_LEDSTATECMD    == 0x081, "CAN ID mismatch");
static_assert(CAN_ID_PCM_PARKINGDISTANCE  == 0x180, "CAN ID mismatch");
static_assert(CAN_ID_PCM_GEARPOSITION        == 0x181, "CAN ID mismatch");
static_assert(CAN_ID_PCM_FUELLEVEL        == 0x182, "CAN ID mismatch");
static_assert(CAN_ID_PCM_ENGINESPEED         == 0x183, "CAN ID mismatch");
static_assert(CAN_ID_BCM_FANTARGET  == 0x100, "CAN ID mismatch");
static_assert(CAN_ID_BCM_WINDOWTARGET == 0x101, "CAN ID mismatch");
static_assert(CAN_ID_BCM_BUZZERCMD == 0x102, "CAN ID mismatch");
static_assert(CAN_ID_BCM_LEDSTATECMD    == 0x103, "CAN ID mismatch");
static_assert(CAN_ID_BCM_CABINTEMPERATURE       == 0x200, "CAN ID mismatch");
static_assert(CAN_ID_BCM_CABINHUMIDITY          == 0x201, "CAN ID mismatch");
static_assert(CAN_ID_BCM_FANACTUALSPEED  == 0x202, "CAN ID mismatch");
static_assert(CAN_ID_BCM_WINDOWACTUALPOS == 0x203, "CAN ID mismatch");
static_assert(CAN_ID_ECU_DIAGREPORT   == 0x300, "CAN ID mismatch");

#endif // CONFIG_H
