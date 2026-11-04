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

/* Что инициализация узнала об одном датчике. Нужно тексту на ПК, не расчёту угла. */
typedef struct {
  uint8_t bus; /* 1 или 2. 0 — пара шина/адрес так и не появилась. */
  uint8_t addr7;
  uint8_t unit; /* прочитанный UNIT_SEL, если unit_known равен 1. */
  uint8_t unit_known;
  uint8_t sys_status;
  uint8_t sys_err;
  uint8_t fail;
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
   module/code при ошибке. 0 — оба готовы. */
int FW_angle_init(const char **module, const char **code);
void FW_angle_facts(fw_bno_fact_t *internal, fw_bno_fact_t *external);

void FW_angle_meas_begin(void);
void FW_angle_meas_end(void);

/* Окно 10 мс: |heading_ext − heading_int| → десятые градуса 0…1800. В кадр пишет sync.
   0 — угол записан. -1 — датчик не ответил. */
int FW_angle_window(uint16_t *angle_x10);

/* Вне MEASUREMENT раз в FW_BNO_LIVE_MS обновляет Live Watch (эйлер + calib). */
void FW_angle_poll(void);

#endif /* FW_ANGLE_H */
