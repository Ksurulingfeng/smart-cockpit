/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    can.c
 * @brief   This file provides code for the configuration
 *          of the CAN instances.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "can.h"

/* USER CODE BEGIN 0 */
#include "task_headfile.h"
#include "can_drv.h"
/* USER CODE END 0 */

CAN_HandleTypeDef hcan;

/* CAN init function */
void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 9;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_12TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_3TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = ENABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = ENABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */
    HAL_CAN_Start(&hcan);
    
    // 精确滤波器: 仅接收本节点关心的ID
    CAN_FilterTypeDef filter = {0};
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation = ENABLE;
#ifdef NODE_A
    // 节点A只接收 0x080-0x081 (主控→A, bit7=0→控制优先)
    filter.FilterIdHigh = (0x080 << 5);
    filter.FilterMaskIdHigh = (0x7FE << 5);
#else
    // 节点B只接收 0x100-0x103 (主控→B)
    filter.FilterIdHigh = (0x100 << 5);
    filter.FilterMaskIdHigh = (0x7FC << 5);
#endif
    filter.FilterIdLow = 0;
    filter.FilterMaskIdLow = 0;
    if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK) {
        Error_Handler();
    }

    // UDS 滤波器: 精确匹配 0x7E0 (诊断请求)
    CAN_FilterTypeDef uds_filter = {0};
    uds_filter.FilterBank           = 1;
    uds_filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    uds_filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    uds_filter.FilterIdHigh         = (0x7E0 << 5);
    uds_filter.FilterIdLow          = 0x0000;
    uds_filter.FilterMaskIdHigh     = (0x7FF << 5);
    uds_filter.FilterMaskIdLow      = 0x0000;
    uds_filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    uds_filter.FilterActivation     = ENABLE;
    uds_filter.SlaveStartFilterBank = 14;
    if (HAL_CAN_ConfigFilter(&hcan, &uds_filter) != HAL_OK) {
        Error_Handler();
    }

    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_ERROR);
  /* USER CODE END CAN_Init 2 */

}

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspInit 0 */

  /* USER CODE END CAN1_MspInit 0 */
    /* CAN1 clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* CAN1 interrupt Init */
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 12, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{

  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspDeInit 0 */

  /* USER CODE END CAN1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11|GPIO_PIN_12);

    /* CAN1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
  /* USER CODE BEGIN CAN1_MspDeInit 1 */

  /* USER CODE END CAN1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1) {
        CAN_RxHeaderTypeDef rx_header;
        uint8_t rx_data[8];
        HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data);
        if (rx_callback != NULL) {
            rx_callback(rx_header.StdId, rx_data, rx_header.DLC);
        }
    }
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance != CAN1) return;
    uint32_t esr = hcan->ErrorCode;
    g_can_tec = (uint16_t)((hcan->Instance->ESR >> 16) & 0xFF);
    g_can_rec = (uint16_t)((hcan->Instance->ESR >> 24) & 0xFF);

    if (hcan->Instance->ESR & CAN_ESR_BOFF) {
        g_can_error_state = 3; // busoff
    } else if (g_can_tec >= 128 || g_can_rec >= 128) {
        g_can_error_state = 2; // error passive
    } else if (g_can_tec >= 96 || g_can_rec >= 96) {
        g_can_error_state = 1; // warning
    } else {
        g_can_error_state = 0;
    }
    (void)esr;
}
/* USER CODE END 1 */
