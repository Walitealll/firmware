#ifndef FW_STATES_H
#define FW_STATES_H

/* Хранит, в каком режиме сейчас устройство.
   NONE нужен только до первой проверки батареи: это ещё не рабочий режим. */
typedef enum {
  FW_MODE_NONE = 0,
  FW_MODE_LOW_BATTERY,
  FW_MODE_INITIALIZATION,
  FW_MODE_ADVERTISING,
  FW_MODE_CONNECTED,
  FW_MODE_MEASUREMENT
} fw_mode_t;

/* Читают, когда надо решить, можно ли запускать инициализацию ещё раз. */
fw_mode_t FW_states_mode(void);
/* Берут при сборке строки лога, чтобы в ней было имя текущего режима. */
const char *FW_states_mode_name(void);
/* Из главного цикла. Раз в FW_USB_MODE_PERIOD_MS пишет имя режима в USB. */
void FW_states_usb_mode(void);

/* Кабель вставлен или нет. Это не режим: он может стоять рядом с INITIALIZATION и LOW_BATTERY. */
int FW_states_charging(void);
/* Вызывает FW_battery сразу после чтения PGOOD. */
void FW_states_set_charging(int on);

/* Вызывает FW_init, когда напряжение позволяет продолжать запуск.
   Возвращает 1, если переход разрешён, и 0, если устройство уже ушло дальше инициализации. */
int FW_states_to_initialization(void);
/* Вызывает FW_init, когда батарея ниже 3,4 В. Разрешён из любого режима. */
int FW_states_to_low_battery(void);
/* Вызывает FW_init только после успешного прохода. После ошибки шага вернёт 0. */
int FW_states_to_advertising(void);
/* Вызывает FW_ble, когда модуль прислал строку соединения. Пускает только из ADVERTISING. */
int FW_states_to_connected(void);
/* Вызывает FW_ble по START без кабеля. Пускает только из CONNECTED. */
int FW_states_to_measurement(void);
/* Вызывает FW_ble по STOP. Из MEASUREMENT возвращает в CONNECTED. Из других режимов не пускает. */
int FW_states_stop_measurement(void);
/* Вызывает FW_ble по строке разрыва. Из CONNECTED и MEASUREMENT возвращает в ADVERTISING. */
int FW_states_link_down(void);

/* Вызывает FW_init в момент ошибки обязательного шага. Снимается только перезапуском. */
void FW_states_block_advertising(void);
/* Проверяет FW_init в начале прохода, чтобы не запускать сломанную инициализацию заново. */
int FW_states_advertising_blocked(void);

#endif /* FW_STATES_H */
