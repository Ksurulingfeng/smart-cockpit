#ifndef __OLED_I2C_DRV_H__
#define __OLED_I2C_DRV_H__

#include "sys.h"

#define SIZE       8
#define XLevelL    0x00
#define XLevelH    0x10
#define Max_Column 128
#define Max_Row    32
#define X_WIDTH    128
#define Y_WIDTH    32

// OLED 寄存器地址
#define OLED_ADDR     (0x78 >> 1) // 设备地址
#define OLED_REG_CMD  0x00        // 命令寄存器
#define OLED_REG_DATA 0x40        // 数据寄存器

// 显示模式
#define OLED_MODE_NORMAL  1
#define OLED_MODE_INVERSE 0

// 字体大小
#define OLED_SIZE_8x16 16
#define OLED_SIZE_6x8  12

// 写命令/数据标志
#define OLED_CMD  0
#define OLED_DATA 1

// 外部接口函数
void OLED_Init(void);
void OLED_WR_Byte(uint8_t data, uint8_t mode);
void OLED_WR_Byte_Inverse(uint8_t data, uint8_t mode);
void OLED_Set_Pos(uint8_t x, uint8_t y);
void OLED_Display_On(void);
void OLED_Display_Off(void);
void OLED_Clear(void);
void OLED_Clear_Area(uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2);
void OLED_On(void);
void OLED_ShowChar(uint8_t x, uint8_t y, const char chr, uint8_t Char_Size, uint8_t mode);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode);
void OLED_ShowString(uint8_t x, uint8_t y, const char *p, uint8_t Char_Size, uint8_t mode);
void OLED_ShowCHinese(uint8_t x, uint8_t y, const char *s, uint8_t mode);
void OLED_ShowText(uint8_t x, uint8_t y, const char *text, uint8_t Char_Size, uint8_t mode);
void OLED_DrawBMP(uint8_t x0, uint8_t x1, uint8_t page0, uint8_t page1, const uint8_t BMP[]);

#endif /* __OLED_I2C_DRV_H__ */