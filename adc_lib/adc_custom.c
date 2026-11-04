/*
 * adc.c
 *
 *  Created on: 10 окт. 2026 г.
 *      Author: Stepan
 */

#include "adc_custom.h"

uint8_t FLAG_ADC_COMPLETE = 0; // флаг DMA буффер заполнился и можно работать с данными с АЦП

// функция начала опроса АЦП ЕМГ
HAL_StatusTypeDef start_emg(uint16_t *buffer, ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim) {

	HAL_ADCEx_Calibration_Start(hadc);
	HAL_StatusTypeDef adc_start_status = HAL_ADC_Start_DMA(hadc, (uint32_t*)buffer, ADC_BUFFER_SIZE);
	if ( adc_start_status != HAL_OK)
		return adc_start_status;

	return HAL_TIM_Base_Start(htim);
}

//функция остановки опроса АЦП ЕМГ
HAL_StatusTypeDef stop_emg (TIM_HandleTypeDef *htim, ADC_HandleTypeDef *hadc) {
	HAL_TIM_Base_Stop(htim);
	return HAL_ADC_Stop_DMA(hadc);
}

// алгоритм выбора значений из массива данных АЦП
// на данный момент заглушка (пустой алгоритм)
void choose_emg_values(uint16_t *input_buffer, uint16_t *output_buffer) {

	for (int i = 0; i < ADC_BUFFER_SIZE; i++){
		output_buffer[i] = input_buffer[i];
	}
}

// коллбэк функция, которая вызывается по заполнению буффера ДМА с АЦП
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {

	if (hadc == &ADC_EMG) {
		FLAG_ADC_COMPLETE = 1;
	}
}


