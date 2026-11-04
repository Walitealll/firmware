#include "bno055_port.h"

/* Активный датчик: библиотека ivyknob держит один порт и один адрес. */
static fw_hw_i2c_bus_t s_bus;
static uint8_t s_addr7;
static int s_io_ok = 1;

void bno055_select(fw_hw_i2c_bus_t bus, uint8_t addr7)
{
  s_bus = bus;
  s_addr7 = addr7;
  s_io_ok = 1;
}

int bno055_io_ok(void)
{
  return s_io_ok;
}

void bno055_delay(int time)
{
  if (time <= 0) {
    return;
  }
  fw_hw_delay_ms((uint32_t)time);
}

void bno055_writeData(uint8_t reg, uint8_t data)
{
  if (s_bus == 0u || s_addr7 == 0u) {
    s_io_ok = 0;
    return;
  }
  if (fw_hw_i2c_write_reg(s_bus, s_addr7, reg, data) != FW_HW_OK) {
    s_io_ok = 0;
  }
}

void bno055_readData(uint8_t reg, uint8_t *data, uint8_t len)
{
  if (s_bus == 0u || s_addr7 == 0u || data == 0 || len == 0u) {
    s_io_ok = 0;
    return;
  }
  if (fw_hw_i2c_read(s_bus, s_addr7, reg, data, len) != FW_HW_OK) {
    s_io_ok = 0;
  }
}
