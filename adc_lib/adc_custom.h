/*
 * adc.h
 *
 *  Created on: 10 окт. 2026 г.
 *      Author: Stepan
 */

#ifndef INC_ADC_CUSTOM_H_
#define INC_ADC_CUSTOM_H_

#include "main.h"

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim3;

#define F_SAMPLE_EMG 4000 // частота дискретизации ЕМГ

#define TIME_RECORD 10 // временной интервал снятия сигнала ЕМГ в миллисекундах
#define ADC_BUFFER_SIZE ((F_SAMPLE_EMG * TIME_RECORD) / 1000) // размер массива под данные от ЕМГ в uint16_t
#define ADC_EMG hadc1
#define TIMER_EMG htim3

extern uint8_t FLAG_ADC_COMPLETE;

HAL_StatusTypeDef start_emg(uint16_t *buffer, ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim);
HAL_StatusTypeDef stop_emg (TIM_HandleTypeDef *htim, ADC_HandleTypeDef *hadc);
void choose_emg_values(uint16_t *input_buffer, uint16_t *output_buffer);



#endif /* INC_ADC_CUSTOM_H_ */
