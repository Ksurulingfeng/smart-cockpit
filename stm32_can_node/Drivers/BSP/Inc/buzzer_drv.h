#ifndef __BUZZER_DRV_H__
#define __BUZZER_DRV_H__

#include "sys.h"

void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_Toggle(void);
void Buzzer_Beep(uint16_t time);

#endif /* __BUZZER_DRV_H__ */