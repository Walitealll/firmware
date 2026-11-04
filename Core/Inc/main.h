/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define GPIO_IN_PGOOD_Pin GPIO_PIN_13
#define GPIO_IN_PGOOD_GPIO_Port GPIOC
#define ADC2_IN0_MEASURE_CAP_Pin GPIO_PIN_0
#define ADC2_IN0_MEASURE_CAP_GPIO_Port GPIOA
#define GPIO_OUT_DP_CS_Pin GPIO_PIN_4
#define GPIO_OUT_DP_CS_GPIO_Port GPIOA
#define DP_SPI_SCK_Pin GPIO_PIN_5
#define DP_SPI_SCK_GPIO_Port GPIOA
#define DP_SPI_MOSI_Pin GPIO_PIN_7
#define DP_SPI_MOSI_GPIO_Port GPIOA
#define ADC1_IN4_OWN_EMG_Pin GPIO_PIN_0
#define ADC1_IN4_OWN_EMG_GPIO_Port GPIOB
#define GPIO_OUT_EXT_BNO_RST_Pin GPIO_PIN_2
#define GPIO_OUT_EXT_BNO_RST_GPIO_Port GPIOB
#define GPIO_OUT_INT_BNO_RST_Pin GPIO_PIN_12
#define GPIO_OUT_INT_BNO_RST_GPIO_Port GPIOB
#define GPIO_OUT_BLE_MOD_Pin GPIO_PIN_13
#define GPIO_OUT_BLE_MOD_GPIO_Port GPIOB
#define GPIO_OUT_BLE_RST_Pin GPIO_PIN_14
#define GPIO_OUT_BLE_RST_GPIO_Port GPIOB
#define GPIO_OUT_WKP_Pin GPIO_PIN_15
#define GPIO_OUT_WKP_GPIO_Port GPIOB
#define MC_UART1_TX_Pin GPIO_PIN_9
#define MC_UART1_TX_GPIO_Port GPIOA
#define MC_UART1_RX_Pin GPIO_PIN_10
#define MC_UART1_RX_GPIO_Port GPIOA
#define GPIO_OUT_CE_Pin GPIO_PIN_3
#define GPIO_OUT_CE_GPIO_Port GPIOB
#define GPIO_IN_CHG_Pin GPIO_PIN_4
#define GPIO_IN_CHG_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */
/* Ножка 38, PA15. В Cube метки нет: JTAG снят, нога свободна. */
#define GPIO_IN_USB_SENSE_Pin GPIO_PIN_15
#define GPIO_IN_USB_SENSE_GPIO_Port GPIOA
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
