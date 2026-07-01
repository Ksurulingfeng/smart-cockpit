#include "debug.h"

/* debug.c — 调试功能实现文件
 * 当前所有调试功能通过 debug.h 中的宏 debug_printf 实现，
 * 该宏在 DEBUG_ENABLE=1 时映射到 printf，DEBUG_ENABLE=0 时编译为空操作。
 * 如有需要，可在此文件添加额外的调试辅助函数。
 */
