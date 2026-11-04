#ifndef FW_BATTERY_H
#define FW_BATTERY_H

#include <stdint.h>

/* Чем кончилось одно чтение батареи на старте. Режим по этому значению меняет FW_init, не сама батарея. */
typedef enum {
  FW_BATTERY_NOT_LOW = 0,   /* напряжение посчитано и оно не ниже 3,4 В */
  FW_BATTERY_LOW,           /* напряжение посчитано и оно ниже 3,4 В */
  FW_BATTERY_ADC_ERROR,     /* АЦП не отдал код, про порог сказать нельзя */
  FW_BATTERY_SCALE_UNKNOWN  /* код АЦП есть, но делитель неизвестен, в вольты его не переводим */
} fw_battery_status_t;

/* Вызывается в начале инициализации и ещё раз после BLE и после датчиков угла.
   Заодно обновляет флаг кабеля. */
fw_battery_status_t FW_battery_check(void);
/* Напряжение аккумулятора в милливольтах, после делителя. Для кадра питания. */
uint16_t FW_battery_millivolts(void);
/* Из главного цикла. Обновляет флаг «кабель вставлен», режим не меняет. */
void FW_battery_poll(void);

/* Снимок для отладчика. USB на макете не нужен: смотреть fw_battery_dbg.
   mv — милливольты на аккумуляторе, уже после делителя 8/5. 3800 значит 3,800 В.
   raw — сырой код PA0, до перевода в вольты.
   adc_ok — 1, если АЦП ответил.
   cable — 1, если USB_SENSE видит кабель. Пока это и есть «идёт зарядка».
   seq растёт раз в секунду. Если стоит на месте, главный цикл до снимка не дошёл. */
typedef struct {
  uint16_t mv;
  uint16_t raw;
  uint8_t adc_ok;
  uint8_t cable;
  uint8_t pgood_low;
  uint8_t chg_low;
  uint32_t seq;
} fw_battery_dbg_t;

/* Пишет FW_battery_debug_poll раз в секунду, в любом режиме после выхода в главный цикл. */
extern volatile fw_battery_dbg_t fw_battery_dbg;

/* Вызывается из главного цикла. Раз в секунду обновляет снимок.
   Режим не меняет. */
void FW_battery_debug_poll(void);

#endif /* FW_BATTERY_H */
