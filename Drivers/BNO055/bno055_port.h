#ifndef BNO055_PORT_H_
#define BNO055_PORT_H_

#include "bno055.h"
#include "fw_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Выбрать шину и 7-битный адрес перед вызовом API библиотеки.
   Библиотека рассчитана на один датчик — для пары BNO переключаем перед каждым чтением. */
void bno055_select(fw_hw_i2c_bus_t bus, uint8_t addr7);

/* 1 — последняя запись/чтение I2C в библиотеке прошли без ошибки. 0 — сбой. */
int bno055_io_ok(void);

#ifdef __cplusplus
}
#endif

#endif /* BNO055_PORT_H_ */
