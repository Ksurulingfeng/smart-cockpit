#include "init.h"
#include "task_headfile.h"
#include "task_config.h"
#include "can_protocol.h"
#include "can_drv.h"
#include "dht11_drv.h"
#include "led_drv.h"
#include "motor_drv.h"
#include "oled_i2c_drv.h"
#include "servo_drv.h"
#include "ultrasonic_drv.h"
#include "debug.h"

/* 全局任务句柄定义 */
TaskHandle_t xActuatorTaskHandle   = NULL;
TaskHandle_t xADCTaskHandle        = NULL;
TaskHandle_t xCanTaskHandle        = NULL;
TaskHandle_t xDisplayTaskHandle    = NULL;
TaskHandle_t xHumitureTaskHandle   = NULL;
TaskHandle_t xKeyTaskHandle        = NULL;
TaskHandle_t xUltrasonicTaskHandle = NULL;

/* 全局队列定义 */
QueueHandle_t xBuzzerQueue = NULL;
QueueHandle_t xCanRxQueue  = NULL;
QueueHandle_t xFanQueue    = NULL;
QueueHandle_t xKeyQueue    = NULL;
QueueHandle_t xLEDQueue    = NULL;
QueueHandle_t xWindowQueue = NULL;

void System_Init(void)
{
    /* ========== 硬件初始化 ========== */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_USART1_UART_Init();
    MX_CAN_Init();
    MX_TIM1_Init();
    MX_ADC1_Init();
    MX_TIM3_Init();

    /* ========== 驱动初始化 ========== */
    delay_init();
    delay_ms(100); // 确保外设稳定
    OLED_Init();
    DHT11_Init();
    Motor_Init();
    Servo_Init();
    Ultrasonic_Init();

    debug_printf("系统初始化完成！\r\n");

    /* ========== 创建队列 ========== */
    xCanRxQueue  = xQueueCreate(10, sizeof(CanRxMsg_t));
    xBuzzerQueue = xQueueCreate(5, sizeof(uint8_t));
    xFanQueue    = xQueueCreate(5, sizeof(uint8_t));
    xKeyQueue    = xQueueCreate(5, sizeof(uint8_t));
    xLEDQueue    = xQueueCreate(5, sizeof(uint8_t));
    xWindowQueue = xQueueCreate(5, sizeof(uint8_t));

    /* ========== 创建任务 ========== */
    xTaskCreate(vActuatorTask, "Actuator_Task", TASK_STACK_ACTUATOR, NULL, TASK_PRIO_ACTUATOR, &xActuatorTaskHandle);
    xTaskCreate(vADCTask, "ADC_Task", TASK_STACK_ADC, NULL, TASK_PRIO_ADC, &xADCTaskHandle);
    xTaskCreate(vCanTask, "CAN_Task", TASK_STACK_CAN, NULL, TASK_PRIO_CAN, &xCanTaskHandle);
    xTaskCreate(vDisplayTask, "Display_Task", TASK_STACK_DISPLAY, NULL, TASK_PRIO_DISPLAY, &xDisplayTaskHandle);
    xTaskCreate(vHumitureTask, "Humiture_Task", TASK_STACK_HUMITURE, NULL, TASK_PRIO_HUMITURE, &xHumitureTaskHandle);
    xTaskCreate(vKeyTask, "Key_Task", TASK_STACK_KEY, NULL, TASK_PRIO_KEY, &xKeyTaskHandle);
    xTaskCreate(vUltrasonicTask, "Ultrasonic_Task", TASK_STACK_ULTRASONIC, NULL, TASK_PRIO_ULTRASONIC, &xUltrasonicTaskHandle);

    /* ========== 启动调度器 ========== */
    vTaskStartScheduler();
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState       = RCC_HSI_ON;
    RCC_OscInitStruct.LSIState       = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL     = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}
