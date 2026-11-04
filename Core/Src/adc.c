/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.c
  * @brief   This file provides code for the configuration
  *          of the ADC instances.
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
#include "adc.h"

/* USER CODE BEGIN 0 */
#include "fw_config.h"
#include "fw_hw.h"
#include "tim.h" /* TIM3 запускает АЦП ЭМГ, сам файл таймера этот запуск не включает */
/* USER CODE END 0 */

ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

/* ADC1 init function */
void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */
  fw_hw_adc_emg_bind();
  /* USER CODE END ADC1_Init 2 */

}
/* ADC2 init function */
void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspInit 0 */

  /* USER CODE END ADC1_MspInit 0 */
    /* ADC1 clock enable */
    __HAL_RCC_ADC1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**ADC1 GPIO Configuration
    PA0-WKUP     ------> ADC1_IN0
    PB0     ------> ADC1_IN8
    */
    GPIO_InitStruct.Pin = ADC2_IN0_MEASURE_CAP_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(ADC2_IN0_MEASURE_CAP_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = ADC1_IN4_OWN_EMG_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(ADC1_IN4_OWN_EMG_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN ADC1_MspInit 1 */

  /* USER CODE END ADC1_MspInit 1 */
  }
  else if(adcHandle->Instance==ADC2)
  {
  /* USER CODE BEGIN ADC2_MspInit 0 */

  /* USER CODE END ADC2_MspInit 0 */
    /* ADC2 clock enable */
    __HAL_RCC_ADC2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**ADC2 GPIO Configuration
    PA0-WKUP     ------> ADC2_IN0
    */
    GPIO_InitStruct.Pin = ADC2_IN0_MEASURE_CAP_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(ADC2_IN0_MEASURE_CAP_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN ADC2_MspInit 1 */

  /* USER CODE END ADC2_MspInit 1 */
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle)
{

  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspDeInit 0 */

  /* USER CODE END ADC1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_ADC1_CLK_DISABLE();

    /**ADC1 GPIO Configuration
    PA0-WKUP     ------> ADC1_IN0
    PB0     ------> ADC1_IN8
    */
    HAL_GPIO_DeInit(ADC2_IN0_MEASURE_CAP_GPIO_Port, ADC2_IN0_MEASURE_CAP_Pin);

    HAL_GPIO_DeInit(ADC1_IN4_OWN_EMG_GPIO_Port, ADC1_IN4_OWN_EMG_Pin);

  /* USER CODE BEGIN ADC1_MspDeInit 1 */

  /* USER CODE END ADC1_MspDeInit 1 */
  }
  else if(adcHandle->Instance==ADC2)
  {
  /* USER CODE BEGIN ADC2_MspDeInit 0 */

  /* USER CODE END ADC2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_ADC2_CLK_DISABLE();

    /**ADC2 GPIO Configuration
    PA0-WKUP     ------> ADC2_IN0
    */
    HAL_GPIO_DeInit(ADC2_IN0_MEASURE_CAP_GPIO_Port, ADC2_IN0_MEASURE_CAP_Pin);

  /* USER CODE BEGIN ADC2_MspDeInit 1 */

  /* USER CODE END ADC2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* Единственный вход ЭМГ: PB0, канал FW_EMG_ADC_CH. Время выборки то же, что у Cube. */
void fw_hw_adc_emg_bind(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  sConfig.Channel = FW_EMG_ADC_CH;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = FW_EMG_ADC_SAMPLETIME;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/* Вызывается перед измерением батареи. ADC2 поднимает main раньше проверки. */
int fw_hw_adc_battery_ready(void)
{
  return 1;
}

/* 1 после первой удачной калибровки ADC2. Повторно калибровать не нужно: данные живут, пока есть питание. */
static uint8_t s_bat_cal;

/* Вызывается из проверки батареи и из снимка в ADVERTISING.
   Один запуск ADC2, канал уже выбран в Cube: PA0. Код сырой, делитель сюда не входит. */
fw_hw_status_t fw_hw_adc_battery_raw(uint16_t *raw)
{
  if (raw == 0) {
    return FW_HW_ERR;
  }
  /* Без калибровки код на этом кристалле смещён. Её делают при выключенном преобразовании. */
  if (s_bat_cal == 0u) {
    if (HAL_ADCEx_Calibration_Start(&hadc2) != HAL_OK) {
      return FW_HW_ERR;
    }
    s_bat_cal = 1u;
  }
  if (HAL_ADC_Start(&hadc2) != HAL_OK) {
    return FW_HW_ERR;
  }
  if (HAL_ADC_PollForConversion(&hadc2, FW_IO_TIMEOUT_MS) != HAL_OK) {
    (void)HAL_ADC_Stop(&hadc2);
    return FW_HW_ERR;
  }
  *raw = (uint16_t)HAL_ADC_GetValue(&hadc2);
  (void)HAL_ADC_Stop(&hadc2);
  return FW_HW_OK;
}

/* Кольцо ЭМГ. DMA пишет его по кругу, половина — одно окно из FW_EMG_FRAME_COUNT отсчётов. */
static uint16_t s_emg_dma[FW_EMG_DMA_SAMPLES];
static DMA_HandleTypeDef hdma_adc1;
/* Сколько половин DMA уже заполнил. Главный цикл забирает это число, в прерывании только плюс один. */
static volatile uint8_t s_emg_seq;
/* Какую половину цикл уже забрал. Отставание больше чем на одну — окно затёрто. */
static uint8_t s_emg_taken;
/* 1 после первой калибровки ADC1. Повторный старт измерения её не делает. */
static uint8_t s_emg_cal;
/* 1, когда канал DMA уже связан с ADC1. */
static uint8_t s_emg_dma_ready;
/* 1, пока TIM3 и DMA ЭМГ запущены. */
static uint8_t s_emg_on;

/* Прерывание DMA1 канал 1. На этом кристалле канал 1 закреплён за ADC1. Здесь только HAL. */
void DMA1_Channel1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_adc1);
}

/* Половина кольца заполнена. Кадр отсюда не собирается. */
static void emg_half_done(ADC_HandleTypeDef *hadc)
{
  if (hadc != 0 && hadc->Instance == ADC1) {
    s_emg_seq++;
  }
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  emg_half_done(hadc);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  emg_half_done(hadc);
}

/* Один раз связывает DMA1 канал 1 с ADC1. Кольцо круговое, полуслово — код АЦП 12 бит. */
static int emg_dma_bind(void)
{
  if (s_emg_dma_ready != 0u) {
    return 0;
  }
  __HAL_RCC_DMA1_CLK_ENABLE();
  hdma_adc1.Instance = DMA1_Channel1;
  hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
  hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  hdma_adc1.Init.Mode = DMA_CIRCULAR;
  hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
  if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) {
    return -1;
  }
  __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, FW_EMG_DMA_IRQ_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  s_emg_dma_ready = 1u;
  return 0;
}

/* Снимает с АЦП залипший признак ошибки. Иначе следующий HAL_ADC_Init сразу отказывается. */
static void emg_clear_adc_error(void)
{
  CLEAR_BIT(hadc1.State, HAL_ADC_STATE_ERROR_INTERNAL | HAL_ADC_STATE_ERROR_DMA);
  hadc1.ErrorCode = HAL_ADC_ERROR_NONE;
  __HAL_UNLOCK(&hadc1);
}

/* 1 — канал DMA включён и в нём ещё все FW_EMG_DMA_SAMPLES полуслов.
   Таймер в этот момент ещё стоит, поэтому длина не должна успеть уменьшиться. */
static int emg_dma_is_armed(void)
{
  if (hdma_adc1.Instance == 0) {
    return 0;
  }
  if ((hdma_adc1.Instance->CCR & DMA_CCR_EN) == 0u) {
    return 0;
  }
  return hdma_adc1.Instance->CNDTR == (uint32_t)FW_EMG_DMA_SAMPLES;
}

/* 1 — прошлый стоп оставил включённый канал, не READY или ошибку АЦП. */
static int emg_dma_needs_release(void)
{
  if ((hadc1.State & (HAL_ADC_STATE_ERROR_INTERNAL | HAL_ADC_STATE_ERROR_DMA)) != 0u) {
    return 1;
  }
  if (s_emg_dma_ready == 0u || hdma_adc1.Instance == 0) {
    return 0;
  }
  if ((hdma_adc1.Instance->CCR & DMA_CCR_EN) != 0u) {
    return 1;
  }
  return hdma_adc1.State != HAL_DMA_STATE_READY;
}

/* Гасит TIM3, АЦП и канал DMA до нуля в регистре. Состояние HAL выставляет READY сам,
   не дожидаясь, что Stop_DMA это сделал. */
static void emg_dma_force_idle(void)
{
  uint32_t t0;

  (void)HAL_TIM_Base_Stop(&htim3);
  if (s_emg_dma_ready != 0u && hadc1.DMA_Handle != 0) {
    (void)HAL_ADC_Stop_DMA(&hadc1);
  }
  if (s_emg_dma_ready != 0u && hdma_adc1.Instance != 0) {
    __HAL_DMA_DISABLE_IT(&hdma_adc1, DMA_IT_TC | DMA_IT_HT | DMA_IT_TE);
    __HAL_DMA_DISABLE(&hdma_adc1);
    t0 = fw_hw_tick_ms();
    while ((hdma_adc1.Instance->CCR & DMA_CCR_EN) != 0u) {
      if ((fw_hw_tick_ms() - t0) >= FW_IO_TIMEOUT_MS) {
        break;
      }
    }
    if (hdma_adc1.DmaBaseAddress != 0) {
      hdma_adc1.DmaBaseAddress->IFCR = (DMA_ISR_GIF1 << hdma_adc1.ChannelIndex);
    }
    hdma_adc1.State = HAL_DMA_STATE_READY;
    hdma_adc1.ErrorCode = HAL_DMA_ERROR_NONE;
    __HAL_UNLOCK(&hdma_adc1);
  }
  emg_clear_adc_error();
}

/* Вызывается при входе в MEASUREMENT. TIM3 уже настроен на FW_EMG_HZ и TRGO.
   Запуск АЦП — от этого TRGO, не программный. Калибровка один раз до первого старта.
   Если после Start_DMA канал не включился или длина не 40, гасим его и пробуем ещё раз. */
fw_hw_status_t fw_hw_emg_start(void)
{
  uint32_t prim;
  unsigned attempt;

  if (s_emg_on != 0u) {
    return FW_HW_OK;
  }
  if (emg_dma_needs_release() != 0) {
    emg_dma_force_idle();
  } else {
    emg_clear_adc_error();
  }
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.ContinuousConvMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) {
    emg_clear_adc_error();
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
      return FW_HW_ERR;
    }
  }
  /* Повторный Init не выбирает канал заново. ЭМГ остаётся на PB0, канал 8. */
  fw_hw_adc_emg_bind();
  if (s_emg_cal == 0u) {
    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK) {
      return FW_HW_ERR;
    }
    s_emg_cal = 1u;
  }
  if (emg_dma_bind() != 0) {
    return FW_HW_ERR;
  }
  for (attempt = 0u; attempt < 2u; attempt++) {
    if (attempt != 0u) {
      emg_dma_force_idle();
    }
    prim = __get_PRIMASK();
    __disable_irq();
    s_emg_seq = 0u;
    s_emg_taken = 0u;
    if (prim == 0u) {
      __enable_irq();
    }
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_emg_dma, FW_EMG_DMA_SAMPLES) != HAL_OK) {
      continue;
    }
    if (emg_dma_is_armed() != 0) {
      break;
    }
  }
  if (emg_dma_is_armed() == 0) {
    emg_dma_force_idle();
    return FW_HW_ERR;
  }
  if (HAL_TIM_Base_Start(&htim3) != HAL_OK) {
    emg_dma_force_idle();
    return FW_HW_ERR;
  }
  s_emg_on = 1u;
  return FW_HW_OK;
}

/* Вызывается при выходе из MEASUREMENT. Недособранное окно остаётся в кольце и наружу не идёт. */
void fw_hw_emg_stop(void)
{
  if (s_emg_on == 0u) {
    return;
  }
  emg_dma_force_idle();
  s_emg_on = 0u;
}

/* Забирает одну готовую половину: 20 отсчётов от старого к новому.
   1 — dst заполнен. 0 — новой половины нет. -1 — половина уже затёрта, dst не использовать. */
int fw_hw_emg_take(uint16_t *dst)
{
  uint8_t seq;
  uint8_t prev;
  uint8_t delta;
  uint32_t prim;
  const uint16_t *src;
  unsigned i;

  if (dst == 0 || s_emg_on == 0u) {
    return -1;
  }
  prim = __get_PRIMASK();
  __disable_irq();
  seq = s_emg_seq;
  prev = s_emg_taken;
  if (seq != prev) {
    s_emg_taken = seq;
  }
  if (prim == 0u) {
    __enable_irq();
  }
  delta = (uint8_t)(seq - prev);
  if (delta == 0u) {
    return 0;
  }
  /* Больше одной половины с прошлого забора: старое окно DMA уже переписал. */
  if (delta != 1u) {
    return -1;
  }
  /* Нечётный номер — первая половина после половинного прерывания. Чётный — вторая, после полного. */
  src = (seq & 1u) != 0u ? s_emg_dma : (s_emg_dma + FW_EMG_FRAME_COUNT);
  for (i = 0u; i < FW_EMG_FRAME_COUNT; i++) {
    dst[i] = src[i];
  }
  return 1;
}

/* USER CODE END 1 */
