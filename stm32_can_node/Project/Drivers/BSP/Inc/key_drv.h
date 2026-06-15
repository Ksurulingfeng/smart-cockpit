#ifndef __KEY_DRV_H__
#define __KEY_DRV_H__

#include "sys.h"

#define KEY_NONE  0
#define KEY_NEXT  1
#define KEY_PREV  2
#define KEY_ENTER 3

#define GEAR_NONE 0
#define GEAR_D    1
#define GEAR_P    2
#define GEAR_R    3

uint8_t Key_GetState(void);
uint8_t Gear_GetState(void);

#endif /* __KEY_DRV_H__ */
