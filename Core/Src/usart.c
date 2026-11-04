/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
#include "usart.h"

/* USER CODE BEGIN 0 */
#include "fw_config.h"
#include "fw_hw.h"

/* Кольцо принятых байт. Пишет его только прерывание, читает только главный цикл. */
static uint8_t s_rx_ring[FW_BLE_RX_RING_BYTES];
static volatile unsigned s_rx_head;
static volatile unsigned s_rx_tail;
/* Сколько байт не влезло в кольцо. Забирает главный цикл, не прерывание. */
static volatile unsigned s_rx_drop;
/* Сюда HAL кладёт один байт перед колбэком. */
static uint8_t s_rx_byte;
/* 1 после первого успешного включения приёма. Повторно прерывание не поднимаем. */
static uint8_t s_rx_listen;
/* 1, если колбэк не смог сразу ждать следующий байт: HAL в этот момент занят передачей.
   Следующий забор из кольца поднимает приём сам, иначе +OK после команды теряется. */
static uint8_t s_rx_rearm;

/* USART1 TX на STM32F103 идёт через DMA1 канал 4. В Cube канал не заведён, поднимаем его здесь. */
DMA_HandleTypeDef hdma_usart1_tx;
/* 1 после конца передачи. Главный цикл забирает этот флаг и не ждёт UART. */
static volatile uint8_t s_tx_done;
/* 1, пока DMA ещё пишет этот кадр. Ошибка приёма не должна снимать чужую передачу. */
static uint8_t s_tx_active;
static uint8_t s_tx_dma_ready;
/* USER CODE END 0 */

UART_HandleTypeDef huart1;

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */
    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = MC_UART1_TX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MC_UART1_TX_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = MC_UART1_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(MC_UART1_RX_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOA, MC_UART1_TX_Pin|MC_UART1_RX_Pin);

  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* Вызывается перед первой командой AT.
   Проверяет, что Cube уже поставил кадр модуля: 115200, 8 бит, без чётности и без потока.
   Очередь байтов и приём в прерывании здесь не включаются. */
int fw_hw_uart_ready(void)
{
  return huart1.Instance == USART1 &&
         huart1.gState == HAL_UART_STATE_READY &&
         huart1.Init.BaudRate == FW_BLE_UART_BAUD_BPS &&
         huart1.Init.WordLength == UART_WORDLENGTH_8B &&
         huart1.Init.StopBits == UART_STOPBITS_1 &&
         huart1.Init.Parity == UART_PARITY_NONE &&
         huart1.Init.HwFlowCtl == UART_HWCONTROL_NONE;
}

/* Вызывается, когда надо отправить одну команду AT. Конец строки сам не дописывает. */
fw_hw_status_t fw_hw_uart_tx(const uint8_t *data, uint16_t len)
{
  /* Пустая посылка или нет буфера. В порт такое слать незачем. */
  if (data == 0 || len == 0u) {
    return FW_HW_ERR;
  }
  /* За отведённое время байты не ушли. Команду считаем неотправленной. */
  if (HAL_UART_Transmit(&huart1, data, len, FW_IO_TIMEOUT_MS) != HAL_OK) {
    return FW_HW_ERR;
  }
  return FW_HW_OK;
}

/* Вызывается по одному байту, пока собирается ответ модуля.
   Если timeout_ms равен нулю, байт забирается только когда он уже лежит в приёмнике. */
fw_hw_status_t fw_hw_uart_rx(uint8_t *byte, uint32_t timeout_ms)
{
  HAL_StatusTypeDef status;

  /* Кладём принятый байт вызывающему. Без указателя читать некуда. */
  if (byte == 0) {
    return FW_HW_ERR;
  }
  status = HAL_UART_Receive(&huart1, byte, 1u, timeout_ms);
  if (status == HAL_OK) {
    return FW_HW_OK;
  }
  /* Байт не пришёл вовремя. Это тишина, не поломка порта. */
  if (status == HAL_TIMEOUT) {
    return FW_HW_TIMEOUT;
  }
  return FW_HW_ERR;
}

/* Вызывается из колбэка приёма. Кладёт байт в кольцо. Если места нет, байт считается потерянным. */
static void rx_push(uint8_t byte)
{
  unsigned next = (s_rx_head + 1u) % FW_BLE_RX_RING_BYTES;

  if (next == s_rx_tail) {
    s_rx_drop++;
    return;
  }
  s_rx_ring[s_rx_head] = byte;
  /* Байт уже в кольце, только потом двигаем голову. �?наче цикл прочитает пустую ячейку. */
  __DMB();
  s_rx_head = next;
}

/* Снова ждём один байт. Если HAL занят передачей, откладываем до главного цикла. */
static void rx_arm_next(void)
{
  if (HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1u) != HAL_OK) {
    s_rx_rearm = 1u;
    return;
  }
  s_rx_rearm = 0u;
}

/* Вызывается из HAL, когда один байт USART1 уже лежит в s_rx_byte. Сразу ждём следующий. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART1) {
    return;
  }
  rx_push(s_rx_byte);
  rx_arm_next();
}

/* Вызывается из HAL, если приём споткнулся. Снова включаем ожидание байта, иначе кольцо остановится. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART1) {
    return;
  }
  /* �?наче слот так и останется «в передаче», и очередь встанет. */
  if (s_tx_active != 0u) {
    s_tx_active = 0u;
    s_tx_done = 1u;
  }
  if (s_rx_listen == 0u) {
    return;
  }
  rx_arm_next();
}

int fw_hw_uart_rx_listen(void)
{
  if (s_rx_listen != 0u) {
    return 1;
  }
  HAL_NVIC_SetPriority(USART1_IRQn, FW_BLE_UART_IRQ_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
  rx_arm_next();
  if (s_rx_rearm != 0u) {
    return 0;
  }
  s_rx_listen = 1u;
  return 1;
}

unsigned fw_hw_uart_rx_drop_take(void)
{
  unsigned n;
  uint32_t primask;

  primask = __get_PRIMASK();
  __disable_irq();
  n = s_rx_drop;
  s_rx_drop = 0u;
  if (primask == 0u) {
    __enable_irq();
  }
  return n;
}

fw_hw_status_t fw_hw_uart_take(uint8_t *byte)
{
  if (byte == 0) {
    return FW_HW_ERR;
  }
  /* Приём мог остановиться на время передачи команды. Поднимаем его до чтения кольца. */
  if (s_rx_rearm != 0u) {
    rx_arm_next();
  }
  if (s_rx_tail == s_rx_head) {
    return FW_HW_TIMEOUT;
  }
  *byte = s_rx_ring[s_rx_tail];
  __DMB();
  s_rx_tail = (s_rx_tail + 1u) % FW_BLE_RX_RING_BYTES;
  return FW_HW_OK;
}

/* Вызывается один раз перед первой передачей DMA. */
static void tx_dma_ready(void)
{
  if (s_tx_dma_ready != 0u) {
    return;
  }
  __HAL_RCC_DMA1_CLK_ENABLE();
  hdma_usart1_tx.Instance = DMA1_Channel4;
  hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
  hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
  hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_usart1_tx.Init.Mode = DMA_NORMAL;
  hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
  if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK) {
    return;
  }
  __HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, FW_BLE_UART_IRQ_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  s_tx_dma_ready = 1u;
}

/* Вызывается из HAL, когда DMA дописал байты в USART. Очередь здесь не трогаем. */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART1) {
    return;
  }
  s_tx_active = 0u;
  s_tx_done = 1u;
}

fw_hw_status_t fw_hw_uart_tx_dma(const uint8_t *data, uint16_t len)
{
  if (data == 0 || len == 0u) {
    return FW_HW_ERR;
  }
  tx_dma_ready();
  if (s_tx_dma_ready == 0u) {
    return FW_HW_ERR;
  }
  s_tx_done = 0u;
  if (HAL_UART_Transmit_DMA(&huart1, (uint8_t *)data, len) != HAL_OK) {
    return FW_HW_ERR;
  }
  s_tx_active = 1u;
  return FW_HW_OK;
}

int fw_hw_uart_tx_done(void)
{
  if (s_tx_done == 0u) {
    return 0;
  }
  s_tx_done = 0u;
  return 1;
}

int fw_hw_uart_tx_line_idle(void)
{
  if (s_tx_active != 0u) {
    return 0;
  }
  /* TC — последний бит уже выдвинут. Пока он в регистре, следующий кадр подождёт. */
  if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET) {
    return 0;
  }
  return 1;
}

/* USER CODE END 1 */
