#ifndef FW_HW_H
#define FW_HW_H

#include <stdint.h>

typedef enum { FW_HW_OK = 0, FW_HW_ERR = -1, FW_HW_ABSENT = -3 } fw_hw_status_t;
typedef enum { FW_HW_I2C1 = 1, FW_HW_I2C2 = 2 } fw_hw_i2c_bus_t;

void fw_hw_delay_ms(uint32_t delay_ms);
int fw_hw_i2c_ready(void);
fw_hw_status_t fw_hw_i2c_probe(fw_hw_i2c_bus_t bus, uint8_t addr7);
fw_hw_status_t fw_hw_i2c_write_reg(fw_hw_i2c_bus_t bus, uint8_t addr7, uint8_t reg, uint8_t value);
fw_hw_status_t fw_hw_i2c_read_reg(fw_hw_i2c_bus_t bus, uint8_t addr7, uint8_t reg, uint8_t *value);
fw_hw_status_t fw_hw_i2c_read(fw_hw_i2c_bus_t bus, uint8_t addr7, uint8_t reg, uint8_t *data, uint16_t len);

#endif
