#include "main.h"
#include "i2c.h"
#include "bno_debug.h"
#include "fw_config.h"
#include "fw_hw.h"

#define BNO_RESET_PINS (GPIO_OUT_INT_BNO_RST_Pin | GPIO_OUT_EXT_BNO_RST_Pin)

/* Useful even if the board fails before I2C is ready. */
volatile enum {
  BOARD_BOOT, BOARD_CLOCK, BOARD_I2C1, BOARD_I2C2, BOARD_BNO, BOARD_RUNNING
} g_board_stage;

static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};

  osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  osc.HSEState = RCC_HSE_ON;
  osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  osc.HSIState = RCC_HSI_ON;
  osc.PLL.PLLState = RCC_PLL_ON;
  osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  osc.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
    Error_Handler();
  }

  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV2;
  clk.APB2CLKDivider = RCC_HCLK_DIV4;
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }
}

static void Board_GPIO_Init(void)
{
  GPIO_InitTypeDef pins = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  HAL_GPIO_WritePin(GPIOA, GPIO_OUT_DP_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOB, GPIO_OUT_BLE_MOD_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOB, BNO_RESET_PINS | GPIO_OUT_BLE_RST_Pin | GPIO_OUT_WKP_Pin |
                           GPIO_OUT_CE_Pin, GPIO_PIN_RESET);

  pins.Mode = GPIO_MODE_OUTPUT_PP;
  pins.Speed = GPIO_SPEED_FREQ_LOW;
  pins.Pin = GPIO_OUT_DP_CS_Pin;
  HAL_GPIO_Init(GPIOA, &pins);
  pins.Pin = BNO_RESET_PINS | GPIO_OUT_BLE_MOD_Pin | GPIO_OUT_BLE_RST_Pin |
             GPIO_OUT_WKP_Pin | GPIO_OUT_CE_Pin;
  HAL_GPIO_Init(GPIOB, &pins);
}

void fw_hw_delay_ms(uint32_t delay_ms)
{
  HAL_Delay(delay_ms);
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}

int main(void)
{
  HAL_Init();
  g_board_stage = BOARD_CLOCK;
  SystemClock_Config();
  Board_GPIO_Init();
  g_board_stage = BOARD_I2C1;
  MX_I2C1_Init();
  g_board_stage = BOARD_I2C2;
  MX_I2C2_Init();
  g_board_stage = BOARD_BNO;
  /* Reset the sensors even when Debug only resets the STM32. */
  HAL_Delay(20u);
  HAL_GPIO_WritePin(GPIOB, BNO_RESET_PINS, GPIO_PIN_SET);
  HAL_Delay(1000u);
  bno_debug_init();
  g_board_stage = BOARD_RUNNING;

  for (;;) {
    bno_debug_sample();
    HAL_Delay(BNO_SAMPLE_PERIOD_MS);
  }
}

void Error_Handler(void)
{
  __disable_irq();
  for (;;) {}
}
