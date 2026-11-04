/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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
#include "gpio.h"

/* USER CODE BEGIN 0 */
#include "fw_config.h"
#include "fw_hw.h"
/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIO_OUT_DP_CS_GPIO_Port, GPIO_OUT_DP_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_OUT_EXT_BNO_RST_Pin|GPIO_OUT_INT_BNO_RST_Pin|GPIO_OUT_BLE_MOD_Pin|GPIO_OUT_BLE_RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_OUT_WKP_Pin|GPIO_OUT_CE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PtPin */
  GPIO_InitStruct.Pin = GPIO_IN_PGOOD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIO_IN_PGOOD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PtPin */
  GPIO_InitStruct.Pin = GPIO_OUT_DP_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIO_OUT_DP_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PBPin PBPin PBPin PBPin
                           PBPin PBPin */
  GPIO_InitStruct.Pin = GPIO_OUT_EXT_BNO_RST_Pin|GPIO_OUT_INT_BNO_RST_Pin|GPIO_OUT_BLE_MOD_Pin|GPIO_OUT_BLE_RST_Pin
                          |GPIO_OUT_WKP_Pin|GPIO_OUT_CE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PtPin */
  GPIO_InitStruct.Pin = GPIO_IN_CHG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIO_IN_CHG_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init 2 */
  /* Ножка 38. Подтяжка против уровня «кабель есть», чтобы висящий вход не выглядел как зарядка. */
  GPIO_InitStruct.Pin = GPIO_IN_USB_SENSE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = FW_USB_SENSE_PRESENT_HIGH ? GPIO_PULLDOWN : GPIO_PULLUP;
  HAL_GPIO_Init(GPIO_IN_USB_SENSE_GPIO_Port, &GPIO_InitStruct);
  /* USER CODE END MX_GPIO_Init 2 */

}

/* USER CODE BEGIN 2 */

/* Вызывается перед настройкой BLE. Сверяет, что ядро правда на 72 МГц из схемы. */
int fw_hw_clock_ready(void)
{
  return SystemCoreClock == FW_SYSCLK_HZ;
}

/* Вызывается, когда надо поставить время в строку лога или понять, прошли ли уже 200 мс.
   Это обычный тик после HAL_Init. Таймер измерений на 5 мс отсюда не идёт. */
uint32_t fw_hw_tick_ms(void)
{
  return HAL_GetTick();
}

/* Вызывается на инициализации, когда время ожидания уже известно: пробуждение модуля, выход датчика из сброса. */
void fw_hw_delay_ms(uint32_t delay_ms)
{
  HAL_Delay(delay_ms);
}

/* Вызывается при проверке батареи. Ноль на PGOOD значит, что зарядник видит кабель. */
int fw_hw_pgood_is_low(void)
{
  return HAL_GPIO_ReadPin(FW_PGOOD_PORT, FW_PGOOD_PIN) == GPIO_PIN_RESET;
}

/* 1 — кабель вставлен. Сейчас это USB_SENSE. Источник переключается константой FW_CABLE_SRC. */
int fw_hw_cable_present(void)
{
#if FW_CABLE_SRC == FW_CABLE_SRC_PGOOD
  return fw_hw_pgood_is_low();
#else
  int high = HAL_GPIO_ReadPin(FW_USB_SENSE_PORT, FW_USB_SENSE_PIN) == GPIO_PIN_SET;

  if (FW_USB_SENSE_PRESENT_HIGH) {
    return high;
  }
  return high == 0;
#endif
}

/* Вызывается при кадре питания. Ноль на CHG значит, что ток в батарею идёт. */
int fw_hw_chg_is_low(void)
{
  return HAL_GPIO_ReadPin(FW_CHG_PORT, FW_CHG_PIN) == GPIO_PIN_RESET;
}

/* Вызывается в начале настройки BLE. Сброс модуля активен нулём, поэтому здесь держим единицу.
   Короткий импульс не даём: сколько его держать, в мануале не написано. */
void fw_hw_ble_rst_idle(void)
{
  HAL_GPIO_WritePin(FW_BLE_RST_PORT, FW_BLE_RST_PIN, GPIO_PIN_SET);
}

/* Низкий уровень защёлки MOD. Его держат 200 мс: более короткую смену модуль не принимает. */
void fw_hw_ble_mod_low(void)
{
  HAL_GPIO_WritePin(FW_BLE_MOD_PORT, FW_BLE_MOD_PIN, GPIO_PIN_RESET);
}

/* Высокий уровень защёлки MOD. После соединения с ним байты уходят на телефон. */
void fw_hw_ble_mod_high(void)
{
  HAL_GPIO_WritePin(FW_BLE_MOD_PORT, FW_BLE_MOD_PIN, GPIO_PIN_SET);
}

/* Вызывается в конце защёлки. 1 — ножка осталась в единице. */
int fw_hw_ble_mod_is_high(void)
{
  return HAL_GPIO_ReadPin(FW_BLE_MOD_PORT, FW_BLE_MOD_PIN) == GPIO_PIN_SET;
}

/* Вызывается один раз, если модуль промолчал на AT.
   Будит спадом: пин должен упасть и побыть в нуле 200 мс.
   После этого вверх его не поднимаем, иначе модуль снова уснёт. */
void fw_hw_ble_wake(void)
{
  /* Пин уже в нуле, спада не получится. Сначала держим единицу те же 200 мс, что и для сна, потом роняем. */
  if (HAL_GPIO_ReadPin(FW_BLE_WKP_PORT, FW_BLE_WKP_PIN) == GPIO_PIN_RESET) {
    HAL_GPIO_WritePin(FW_BLE_WKP_PORT, FW_BLE_WKP_PIN, GPIO_PIN_SET);
    fw_hw_delay_ms(FW_BLE_WKP_LEVEL_MS);
  }
  HAL_GPIO_WritePin(FW_BLE_WKP_PORT, FW_BLE_WKP_PIN, GPIO_PIN_RESET);
  fw_hw_delay_ms(FW_BLE_WKP_LEVEL_MS);
}

/* Макет. На Blue Pill вывода PB2 нет, сброс выносного BNO сидит на PB1.
   Сначала единица, потом выход: ноль на этой ноге держит датчик в сбросе. */
void fw_hw_ext_bno_rst_demo_init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  HAL_GPIO_WritePin(EXT_BNO_RST_DEMO_PORT, EXT_BNO_RST_DEMO_PIN, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = EXT_BNO_RST_DEMO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(EXT_BNO_RST_DEMO_PORT, &GPIO_InitStruct);
}

/* Пишет уровень на сброс одного датчика угла. Вызывается, когда датчик надо удержать в сбросе или отпустить. */
static void bno_reset_write(fw_hw_bno_id_t id, GPIO_PinState level)
{
  /* Внутренний датчик сидит на своей ноге, внешний на своей. Общего сброса у них нет. */
  if (id == FW_HW_BNO_INTERNAL) {
    HAL_GPIO_WritePin(FW_BNO_INT_RST_PORT, FW_BNO_INT_RST_PIN, level);
    return;
  }
  /* Плата: сброс внешнего на PB2. Макетный PB1 не пишем. */
  HAL_GPIO_WritePin(FW_BNO_EXT_RST_PORT, FW_BNO_EXT_RST_PIN, level);
  // HAL_GPIO_WritePin(EXT_BNO_RST_DEMO_PORT, EXT_BNO_RST_DEMO_PIN, level);
}

/* Ноль на nRESET держит датчик в сбросе. Пока так, он пропадает с шины, и мы понимаем, кто это был. */
void fw_hw_bno_reset_hold(fw_hw_bno_id_t id)
{
  bno_reset_write(id, GPIO_PIN_RESET);
}

/* Единица отпускает сброс. После этого надо подождать, пока датчик сам выйдет в режим настройки. */
void fw_hw_bno_reset_release(fw_hw_bno_id_t id)
{
  bno_reset_write(id, GPIO_PIN_SET);
}

/* USER CODE END 2 */
