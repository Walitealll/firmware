/* Standalone comparison image: same board clock, GPIO, I2C and FW_angle
   initialization as the application. BLE is held in reset; UART, USB, ADC,
   SPI and application timers are not started. This is not a product image. */
#include "main.h"
#include "gpio.h"
#include "i2c.h"
#include "FW_angle.h"
#include "bno055_port.h"
#include "fw_config.h"
#include "fw_hw.h"

void SystemClock_Config(void);

typedef struct {
  bno055_vector_t gyro_dps;
  bno055_vector_t accel_ms2;
  uint32_t sampled_ms;
  uint32_t io_errors;
  uint8_t io_ok;
  uint8_t gyro_id;
  uint8_t self_test;
  uint8_t calib_raw;
  uint8_t unit;
  uint8_t opr_mode;
  uint8_t pwr_mode;
  uint8_t sys_trigger;
  uint8_t sys_status;
  uint8_t sys_err;
} bno_reference_sensor_t;

volatile bno_reference_sensor_t g_bno_reference_internal;
volatile bno_reference_sensor_t g_bno_reference_external;
volatile int g_bno_reference_init_result = -2;
const char * volatile g_bno_reference_module;
const char * volatile g_bno_reference_code;

static void read_reg(fw_hw_i2c_bus_t bus, uint8_t addr, uint8_t reg,
                     volatile uint8_t *out, uint8_t *ok)
{
  uint8_t value;
  if (fw_hw_i2c_read_reg(bus, addr, reg, &value) != FW_HW_OK) {
    *ok = 0u;
    return;
  }
  *out = value;
}

static void sample(fw_hw_i2c_bus_t bus, uint8_t addr,
                   volatile bno_reference_sensor_t *out)
{
  bno055_vector_t gyro, accel;
  uint8_t ok;

  bno055_select(bus, addr);
  gyro = bno055_getVectorGyroscope();
  accel = bno055_getVectorAccelerometer();
  ok = (uint8_t)bno055_io_ok();
  if (ok != 0u) {
    out->gyro_dps = gyro;
    out->accel_ms2 = accel;
  }
  read_reg(bus, addr, BNO055_GYRO_ID, &out->gyro_id, &ok);
  read_reg(bus, addr, BNO055_ST_RESULT, &out->self_test, &ok);
  read_reg(bus, addr, FW_BNO_REG_CALIB_STAT, &out->calib_raw, &ok);
  read_reg(bus, addr, FW_BNO_REG_UNIT_SEL, &out->unit, &ok);
  read_reg(bus, addr, FW_BNO_REG_OPR_MODE, &out->opr_mode, &ok);
  read_reg(bus, addr, FW_BNO_REG_PWR_MODE, &out->pwr_mode, &ok);
  read_reg(bus, addr, FW_BNO_REG_SYS_TRIGGER, &out->sys_trigger, &ok);
  read_reg(bus, addr, FW_BNO_REG_SYS_STATUS, &out->sys_status, &ok);
  read_reg(bus, addr, FW_BNO_REG_SYS_ERR, &out->sys_err, &ok);
  out->io_ok = ok;
  if (ok == 0u) {
    out->io_errors++;
  }
  out->sampled_ms = fw_hw_tick_ms();
}

int main(void)
{
  const char *module, *code;
  fw_bno_fact_t internal, external;
  uint32_t last_sample = 0u;

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  HAL_GPIO_WritePin(FW_BLE_RST_PORT, FW_BLE_RST_PIN, GPIO_PIN_RESET);
  MX_I2C1_Init();
  MX_I2C2_Init();

  g_bno_reference_init_result = FW_angle_init(&module, &code);
  g_bno_reference_module = module;
  g_bno_reference_code = code;
  FW_angle_facts(&internal, &external);

  for (;;) {
    if (g_bno_reference_init_result == 0) {
      FW_angle_poll();
      if ((fw_hw_tick_ms() - last_sample) >= FW_BNO_LIVE_MS) {
        last_sample = fw_hw_tick_ms();
        sample((fw_hw_i2c_bus_t)internal.bus, internal.addr7, &g_bno_reference_internal);
        sample((fw_hw_i2c_bus_t)external.bus, external.addr7, &g_bno_reference_external);
      }
    }
    HAL_Delay(10u);
  }
}
