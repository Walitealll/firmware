/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2c.c
  * @brief   This file provides code for the configuration
  *          of the I2C instances.
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
#include "i2c.h"

/* USER CODE BEGIN 0 */
#include "fw_config.h"
#include "fw_hw.h"

/* Вызывается перед каждым обменом с датчиком. Отдаёт ту шину, которую назвали снаружи.
   Неизвестный номер — пустой указатель, обмен тогда не начинается. */
static I2C_HandleTypeDef *fw_i2c_handle(fw_hw_i2c_bus_t bus)
{
  if (bus == FW_HW_I2C1) {
    return &hi2c1;
  }
  if (bus == FW_HW_I2C2) {
    return &hi2c2;
  }
  return 0;
}

/* Переводит семибитный адрес датчика в тот вид, которого ждёт HAL.
   Бит чтения и записи HAL добавит сам, в это число его класть не надо. */
static uint16_t fw_i2c_devaddr(uint8_t addr7)
{
  return (uint16_t)((uint16_t)addr7 << 1);
}
/* USER CODE END 0 */

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

/* I2C1 init function */
void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}
/* I2C2 init function */
void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

void HAL_I2C_MspInit(I2C_HandleTypeDef* i2cHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(i2cHandle->Instance==I2C1)
  {
  /* USER CODE BEGIN I2C1_MspInit 0 */

  /* USER CODE END I2C1_MspInit 0 */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**I2C1 GPIO Configuration
    PB8     ------> I2C1_SCL
    PB9     ------> I2C1_SDA
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    __HAL_AFIO_REMAP_I2C1_ENABLE();

    /* I2C1 clock enable */
    __HAL_RCC_I2C1_CLK_ENABLE();
  /* USER CODE BEGIN I2C1_MspInit 1 */

  /* USER CODE END I2C1_MspInit 1 */
  }
  else if(i2cHandle->Instance==I2C2)
  {
  /* USER CODE BEGIN I2C2_MspInit 0 */

  /* USER CODE END I2C2_MspInit 0 */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**I2C2 GPIO Configuration
    PB10     ------> I2C2_SCL
    PB11     ------> I2C2_SDA
    */
    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* I2C2 clock enable */
    __HAL_RCC_I2C2_CLK_ENABLE();
  /* USER CODE BEGIN I2C2_MspInit 1 */

  /* USER CODE END I2C2_MspInit 1 */
  }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* i2cHandle)
{

  if(i2cHandle->Instance==I2C1)
  {
  /* USER CODE BEGIN I2C1_MspDeInit 0 */

  /* USER CODE END I2C1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C1_CLK_DISABLE();

    /**I2C1 GPIO Configuration
    PB8     ------> I2C1_SCL
    PB9     ------> I2C1_SDA
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_9);

  /* USER CODE BEGIN I2C1_MspDeInit 1 */

  /* USER CODE END I2C1_MspDeInit 1 */
  }
  else if(i2cHandle->Instance==I2C2)
  {
  /* USER CODE BEGIN I2C2_MspDeInit 0 */

  /* USER CODE END I2C2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C2_CLK_DISABLE();

    /**I2C2 GPIO Configuration
    PB10     ------> I2C2_SCL
    PB11     ------> I2C2_SDA
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_11);

  /* USER CODE BEGIN I2C2_MspDeInit 1 */

  /* USER CODE END I2C2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* Вызывается перед поиском датчиков угла.
   Обе шины должны быть теми, что настроил Cube: 100 кГц и датчику можно придержать такт, пока он готовит байт. */
int fw_hw_i2c_ready(void)
{
  return hi2c1.Instance == I2C1 &&
         hi2c2.Instance == I2C2 &&
         hi2c1.State == HAL_I2C_STATE_READY &&
         hi2c2.State == HAL_I2C_STATE_READY &&
         hi2c1.Init.ClockSpeed == FW_BNO_I2C_HZ &&
         hi2c2.Init.ClockSpeed == FW_BNO_I2C_HZ &&
         hi2c1.Init.NoStretchMode == I2C_NOSTRETCH_DISABLE &&
         hi2c2.Init.NoStretchMode == I2C_NOSTRETCH_DISABLE;
}

/* Вызывается, когда надо понять, сидит ли датчик на этом адресе. Регистры при этом не читаются. */
fw_hw_status_t fw_hw_i2c_probe(fw_hw_i2c_bus_t bus, uint8_t addr7)
{
  I2C_HandleTypeDef *handle = fw_i2c_handle(bus);
  HAL_StatusTypeDef status;

  /* Назвали шину, которой на плате нет. */
  if (handle == 0) {
    return FW_HW_ERR;
  }
  status = HAL_I2C_IsDeviceReady(handle, fw_i2c_devaddr(addr7), FW_I2C_PROBE_TRIALS, FW_IO_TIMEOUT_MS);
  /* Датчик ответил. */
  if (status == HAL_OK) {
    return FW_HW_OK;
  }
  /* Датчик явно отказался от адреса. Для опроса двух возможных адресов это обычный ответ. */
  if (status == HAL_ERROR) {
    return FW_HW_ABSENT;
  }
  /* Шина занята или не дождалась даже отказа. Это уже не «адреса нет». */
  return FW_HW_ERR;
}

/* Вызывается, когда в один регистр датчика надо записать один байт: режим, питание, единицы. */
fw_hw_status_t fw_hw_i2c_write_reg(fw_hw_i2c_bus_t bus, uint8_t addr7, uint8_t reg, uint8_t value)
{
  I2C_HandleTypeDef *handle = fw_i2c_handle(bus);
  /* HAL забирает указатель на байт, поэтому значение лежит в переменной, а не передаётся числом. */
  uint8_t byte = value;

  if (handle == 0) {
    return FW_HW_ERR;
  }
  /* Запись не прошла за отведённое время. Регистр считаем не изменённым. */
  if (HAL_I2C_Mem_Write(handle, fw_i2c_devaddr(addr7), reg, I2C_MEMADD_SIZE_8BIT, &byte, 1u, FW_IO_TIMEOUT_MS) != HAL_OK) {
    return FW_HW_ERR;
  }
  return FW_HW_OK;
}

/* Вызывается, когда нужен один байт обратно: проверить режим или единицы. */
fw_hw_status_t fw_hw_i2c_read_reg(fw_hw_i2c_bus_t bus, uint8_t addr7, uint8_t reg, uint8_t *value)
{
  /* Кладём байт вызывающему. Без указателя читать некуда. */
  if (value == 0) {
    return FW_HW_ERR;
  }
  return fw_hw_i2c_read(bus, addr7, reg, value, 1u);
}

/* Вызывается, когда байтов несколько подряд, в первую очередь для кватерниона.
   Адрес регистра после первого байта датчик увеличивает сам. */
fw_hw_status_t fw_hw_i2c_read(fw_hw_i2c_bus_t bus, uint8_t addr7, uint8_t reg, uint8_t *data, uint16_t len)
{
  I2C_HandleTypeDef *handle = fw_i2c_handle(bus);

  /* Нет шины, некуда писать или просят ноль байт. Обмен не начинаем. */
  if (handle == 0 || data == 0 || len == 0u) {
    return FW_HW_ERR;
  }
  if (HAL_I2C_Mem_Read(handle, fw_i2c_devaddr(addr7), reg, I2C_MEMADD_SIZE_8BIT, data, len, FW_IO_TIMEOUT_MS) != HAL_OK) {
    return FW_HW_ERR;
  }
  return FW_HW_OK;
}

/* USER CODE END 1 */
