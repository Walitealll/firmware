#ifndef FW_ANGLE_H
#define FW_ANGLE_H

#include <stdint.h>

/* Чем кончилась проверка одного BNO055. 0 — датчик дошёл до статуса 5. */
#define FW_BNO_FAIL_NONE 0u
#define FW_BNO_FAIL_I2C 1u
#define FW_BNO_FAIL_MODE 2u
#define FW_BNO_FAIL_PWR 3u
#define FW_BNO_FAIL_UNIT 4u
#define FW_BNO_FAIL_QUAT 5u /* ориентация так и не появилась */
#define FW_BNO_FAIL_STATUS 6u
#define FW_BNO_FAIL_STATUS_IO 7u
#define FW_BNO_FAIL_COUNT 8u
#define FW_BNO_FAIL_MAP 9u
#define FW_BNO_FAIL_ID 10u
#define FW_BNO_FAIL_RESET 11u
#define FW_BNO_FAIL_CLOCK 12u

/* Что инициализация узнала об одном датчике. Нужно тексту на ПК, не расчёту угла. */
typedef struct {
  uint8_t bus; /* 1 или 2. 0 — пара шина/адрес так и не появилась. */
  uint8_t addr7;
  uint8_t unit; /* прочитанный UNIT_SEL, если unit_known равен 1. */
  uint8_t unit_known;
  uint8_t sys_status;
  uint8_t sys_err;
  uint8_t fail;
  uint8_t chip_id;
  uint8_t opr_mode;
  uint8_t pwr_mode;
  uint8_t sys_trigger; /* бит 7: выбран внешний кварц. */
  uint16_t clock_wait_ms; /* Выдержка после команды выбора часов. */
  int8_t reset_write_status; /* ответ транспорта на RST_SYS; готовность проверяется отдельно. */
  uint8_t mode_retries; /* повторная настройка после возврата NDOF -> CONFIG при запуске. */
  uint8_t addr_probe_mask; /* Последний поиск на I2C1. Биты 0 и 1 соответствуют 0x28 и 0x29. */
} fw_bno_fact_t;

/* Углы Эйлера одной платы, градусы. Live Watch, в кадр не входят. */
typedef struct {
  float heading_z;
  float roll_y;
  float pitch_x;
} fw_bno_eul_t;

/* CALIB_STAT одной платы. Каждое поле 0…3. Live Watch, в кадр не входят. */
typedef struct {
  uint8_t sys;
  uint8_t gyro;
  uint8_t accel;
  uint8_t mag;
  uint8_t raw;
} fw_bno_calib_t;

extern volatile fw_bno_eul_t g_eul_internal;
extern volatile fw_bno_eul_t g_eul_external;
extern volatile fw_bno_calib_t g_calib_internal;
extern volatile fw_bno_calib_t g_calib_external;

/* После BLE: оба BNO в FW_BNO_OPR_ACTIVE (NDOF), UNIT_SEL и SYS_STATUS=5.
   module/code при ошибке. 0 — fusion запущен; калибровка продолжается в фоне. */
int FW_angle_init(const char **module, const char **code);
void FW_angle_facts(fw_bno_fact_t *internal, fw_bno_fact_t *external);

void FW_angle_meas_begin(void);
void FW_angle_meas_end(void);

/* Окно 10 мс. Кратчайшая разность heading двух BNO в десятых градуса (0…1800).
   Возвращает 0 после чтения обоих Euler, при ошибке чтения оставляет угол нетронутым. */
int FW_angle_window(uint16_t *angle_x10);

/* Вне MEASUREMENT обновляет Euler и калибровку раз в FW_BNO_LIVE_MS. */
void FW_angle_poll(void);

#endif /* FW_ANGLE_H */
