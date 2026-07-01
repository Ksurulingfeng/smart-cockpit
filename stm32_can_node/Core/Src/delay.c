#include "delay.h"

// DWT 控制寄存器地址定义
#define DWT_CTRL           (*((volatile uint32_t *)0xE0001000))
#define DWT_CYCCNT         (*((volatile uint32_t *)0xE0001004))
#define DEM_CR             (*((volatile uint32_t *)0xE000EDFC))

#define DEM_CR_TRCENA      (1 << 24)
#define DWT_CTRL_CYCCNTENA (1 << 0)

static uint32_t us_ticks = 0;

void delay_init(void)
{
    // 1. 启用调试监视器跟踪
    DEM_CR |= DEM_CR_TRCENA;
    // 2. 复位计数器
    DWT_CYCCNT = 0;
    // 3. 启用计数器
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;

    // 预计算每微秒的时钟周期数
    us_ticks = HAL_RCC_GetHCLKFreq() / 1000000;
}

void delay_us(uint32_t us)
{
    uint32_t cycles = us_ticks * us;
    uint32_t start  = DWT_CYCCNT;

    while ((DWT_CYCCNT - start) < cycles);
}

void delay_ms(uint32_t ms)
{
    while (ms--) {
        delay_us(1000);
    }
}

void delay_s(uint32_t s)
{
    while (s--) {
        delay_ms(1000);
    }
}