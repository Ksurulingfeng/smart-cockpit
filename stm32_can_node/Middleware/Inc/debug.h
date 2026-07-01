#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>

#define DEBUG_ENABLE 1

#if DEBUG_ENABLE
#define debug_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...) ((void)0)
#endif

#endif /* __DEBUG_H__ */
