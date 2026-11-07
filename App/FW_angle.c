#include "FW_angle.h"

#include "FW_states.h"
#include "bno055_port.h"
#include "fw_config.h"
#include "fw_hw.h"

#include <math.h>

typedef struct {
  fw_hw_i2c_bus_t bus;
  uint8_t addr7;
} fw_bno_hit_t;

static volatile fw_bno_hit_t g_bno_internal;
static volatile fw_bno_hit_t g_bno_external;
static fw_bno_fact_t s_fact_int;
static fw_bno_fact_t s_fact_ext;

volatile fw_bno_eul_t g_eul_internal;
volatile fw_bno_eul_t g_eul_external;
volatile fw_bno_calib_t g_calib_internal;
volatile fw_bno_calib_t g_calib_external;

static uint32_t s_live_ms;

static int find_addr(fw_hw_i2c_bus_t bus, uint8_t *addr, fw_bno_fact_t *fact, const char **code)
{
  static const uint8_t addrs[FW_BNO_ADDR_COUNT] = { FW_BNO_ADDR_COM3_LOW, FW_BNO_ADDR_COM3_HIGH };
  uint32_t started = fw_hw_tick_ms();

  for (;;) {
    int n = 0;
    uint8_t found = 0u;
    int ai;

    fact->addr_probe_mask = 0u;
    for (ai = 0; ai < FW_BNO_ADDR_COUNT; ai++) {
      fw_hw_status_t st = fw_hw_i2c_probe(bus, addrs[ai]);

      if (st == FW_HW_ABSENT) {
        continue;
      }
      if (st != FW_HW_OK) {
        *code = FW_DIAG_CODE_I2C;
        return -2;
      }
      fact->addr_probe_mask |= (uint8_t)(1u << ai);
      found = addrs[ai];
      n++;
    }
    if (n == 1) {
      *addr = found;
      return 0;
    }
    if (n > 1 || (fw_hw_tick_ms() - started) >= FW_BNO_BOOT_TIMEOUT_MS) {
      *code = FW_DIAG_CODE_MAP;
      return -1;
    }
    /* После отпускания nRESET первый опрос может прийти до ответа датчика. */
    fw_hw_delay_ms(FW_BNO_BOOT_POLL_MS);
  }
}

static int write_masked(fw_hw_i2c_bus_t bus, uint8_t addr, uint8_t reg, uint8_t mask, uint8_t value,
                        const char **code)
{
  uint8_t cur = 0;

  if (fw_hw_i2c_read_reg(bus, addr, reg, &cur) != FW_HW_OK) {
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }
  cur = (uint8_t)((cur & (uint8_t)~mask) | (value & mask));
  if (fw_hw_i2c_write_reg(bus, addr, reg, cur) != FW_HW_OK) {
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }
  return 0;
}

static int deg_to_x10(float deg, uint16_t *angle_x10)
{
  float scaled;

  if (angle_x10 == 0) {
    return -1;
  }
  if (deg < 0.0f) {
    deg = 0.0f;
  }
  if (deg > 180.0f) {
    deg = 180.0f;
  }
  scaled = deg * 10.0f + 0.5f;
  if (scaled > 1800.0f) {
    scaled = 1800.0f;
  }
  *angle_x10 = (uint16_t)scaled;
  return 0;
}

static int calib_store(fw_hw_i2c_bus_t bus, uint8_t addr, volatile fw_bno_calib_t *out)
{
  bno055_calibration_state_t cal;

  if (bus == 0 || out == 0) {
    return -1;
  }
  bno055_select(bus, addr);
  cal = bno055_getCalibrationState();
  if (!bno055_io_ok()) {
    return -1;
  }
  out->sys = cal.sys;
  out->gyro = cal.gyro;
  out->accel = cal.accel;
  out->mag = cal.mag;
  out->raw = (uint8_t)((cal.sys << 6) | (cal.gyro << 4) | (cal.accel << 2) | cal.mag);
  return 0;
}

/* ivyknob: x=heading, y=roll, z=pitch. */
static int euler_store(fw_hw_i2c_bus_t bus, uint8_t addr, volatile fw_bno_eul_t *out)
{
  bno055_vector_t v;

  if (bus == 0 || out == 0) {
    return -1;
  }
  bno055_select(bus, addr);
  v = bno055_getVectorEuler();
  if (!bno055_io_ok()) {
    return -1;
  }
  out->heading_z = (float)v.x;
  out->roll_y = (float)v.y;
  out->pitch_x = (float)v.z;
  return 0;
}

/* |heading_ext − heading_int| → 0…180. Обновляет Live Watch обоих. */
static int heading_diff_deg(float *deg)
{
  float diff;

  if (deg == 0) {
    return -1;
  }
  if (euler_store(g_bno_internal.bus, g_bno_internal.addr7, &g_eul_internal) != 0 ||
      euler_store(g_bno_external.bus, g_bno_external.addr7, &g_eul_external) != 0) {
    return -1;
  }
  (void)calib_store(g_bno_internal.bus, g_bno_internal.addr7, &g_calib_internal);
  (void)calib_store(g_bno_external.bus, g_bno_external.addr7, &g_calib_external);

  diff = g_eul_external.heading_z - g_eul_internal.heading_z;
  while (diff > 180.0f) {
    diff -= 360.0f;
  }
  while (diff < -180.0f) {
    diff += 360.0f;
  }
  *deg = fabsf(diff);
  return 0;
}

static int reset_sensor(fw_hw_i2c_bus_t bus, uint8_t addr, fw_bno_fact_t *fact, const char **code)
{
  uint32_t started;
  uint8_t opr;

  if (write_masked(bus, addr, FW_BNO_REG_OPR_MODE, FW_BNO_OPR_MODE_MASK, FW_BNO_OPR_CONFIG, code) != 0) {
    fact->fail = FW_BNO_FAIL_I2C;
    return -1;
  }
  fw_hw_delay_ms(FW_BNO_CONFIG_SWITCH_MS);

  /* RST_SYS перезапускает интерфейс I2C: команда может пройти даже без ACK. */
  fact->reset_write_status = (int8_t)fw_hw_i2c_write_reg(bus, addr, FW_BNO_REG_SYS_TRIGGER, FW_BNO_RST_SYS);
  fw_hw_delay_ms(FW_BNO_RESET_DELAY_MS);
  started = fw_hw_tick_ms();
  for (;;) {
    if (fw_hw_i2c_read_reg(bus, addr, FW_BNO_REG_CHIP_ID, &fact->chip_id) == FW_HW_OK &&
        fact->chip_id == FW_BNO_CHIP_ID &&
        fw_hw_i2c_read_reg(bus, addr, FW_BNO_REG_OPR_MODE, &opr) == FW_HW_OK &&
        (opr & FW_BNO_OPR_MODE_MASK) == FW_BNO_OPR_CONFIG) {
      fw_hw_delay_ms(FW_BNO_BOOT_SETTLE_MS);
      return 0;
    }
    if ((fw_hw_tick_ms() - started) >= FW_BNO_BOOT_TIMEOUT_MS) {
      fact->fail = FW_BNO_FAIL_RESET;
      *code = FW_DIAG_CODE_RESET;
      return -1;
    }
    fw_hw_delay_ms(FW_BNO_BOOT_POLL_MS);
  }
}

static int start_fusion(fw_hw_i2c_bus_t bus, uint8_t addr, fw_bno_fact_t *fact, const char **code)
{
  uint8_t unit = 0;
  uint8_t opr = 0;
  uint8_t pwr = 0;
  uint8_t trigger = 0;
  uint8_t use_ext_crystal;
  uint8_t expected_clock;
  volatile fw_bno_calib_t *calib;
  volatile fw_bno_eul_t *eul;
  uint32_t started;

  if (bus == (fw_hw_i2c_bus_t)FW_BNO_BUS_INT) {
    calib = &g_calib_internal;
    eul = &g_eul_internal;
    use_ext_crystal = FW_BNO_USE_EXT_CRYSTAL_INT;
  } else {
    calib = &g_calib_external;
    eul = &g_eul_external;
    use_ext_crystal = FW_BNO_USE_EXT_CRYSTAL_EXT;
  }

  if (fw_hw_i2c_write_reg(bus, addr, FW_BNO_REG_PAGE_ID, 0u) != FW_HW_OK) {
    fact->fail = FW_BNO_FAIL_I2C;
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }
  if (write_masked(bus, addr, FW_BNO_REG_OPR_MODE, FW_BNO_OPR_MODE_MASK, FW_BNO_OPR_CONFIG, code) != 0) {
    fact->fail = FW_BNO_FAIL_I2C;
    return -1;
  }
  fw_hw_delay_ms(FW_BNO_CONFIG_SWITCH_MS);

  expected_clock = use_ext_crystal != 0u ? FW_BNO_CLK_EXT_MASK : 0u;
  started = fw_hw_tick_ms();
  if (fw_hw_i2c_write_reg(bus, addr, FW_BNO_REG_SYS_TRIGGER, 0u) != FW_HW_OK) {
    fact->fail = FW_BNO_FAIL_CLOCK;
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }
  fw_hw_delay_ms(FW_BNO_CONFIG_WRITE_MS);
  if (use_ext_crystal != 0u) {
    bno055_select(bus, addr);
    bno055_enableExternalCrystal();
    if (!bno055_io_ok()) {
      fact->clock_wait_ms = (uint16_t)(fw_hw_tick_ms() - started);
      fact->fail = FW_BNO_FAIL_CLOCK;
      *code = FW_DIAG_CODE_I2C;
      return -1;
    }
  }
  fact->clock_wait_ms = (uint16_t)(fw_hw_tick_ms() - started);
  if (fw_hw_i2c_read_reg(bus, addr, FW_BNO_REG_SYS_TRIGGER, &trigger) != FW_HW_OK) {
    fact->fail = FW_BNO_FAIL_CLOCK;
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }
  fact->sys_trigger = trigger;
  if ((trigger & FW_BNO_CLK_EXT_MASK) != expected_clock) {
    fact->fail = FW_BNO_FAIL_CLOCK;
    *code = FW_DIAG_CODE_CLOCK;
    return -1;
  }

  if (write_masked(bus, addr, FW_BNO_REG_PWR_MODE, FW_BNO_PWR_MODE_MASK, FW_BNO_PWR_NORMAL, code) != 0) {
    fact->fail = FW_BNO_FAIL_PWR;
    return -1;
  }
  fw_hw_delay_ms(FW_BNO_CONFIG_WRITE_MS);
  if (fw_hw_i2c_read_reg(bus, addr, FW_BNO_REG_PWR_MODE, &pwr) != FW_HW_OK) {
    fact->fail = FW_BNO_FAIL_PWR;
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }
  fact->pwr_mode = pwr;
  if ((pwr & FW_BNO_PWR_MODE_MASK) != FW_BNO_PWR_NORMAL) {
    fact->fail = FW_BNO_FAIL_PWR;
    *code = FW_DIAG_CODE_MODE;
    return -1;
  }
  if (fw_hw_i2c_write_reg(bus, addr, FW_BNO_REG_UNIT_SEL, FW_BNO_UNIT_SEL) != FW_HW_OK ||
      fw_hw_i2c_read_reg(bus, addr, FW_BNO_REG_UNIT_SEL, &unit) != FW_HW_OK) {
    fact->fail = FW_BNO_FAIL_UNIT;
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }
  fact->unit = unit;
  fact->unit_known = 1u;
  if ((unit & FW_BNO_UNIT_MASK) != FW_BNO_UNIT_SEL) {
    fact->fail = FW_BNO_FAIL_UNIT;
    *code = FW_DIAG_CODE_UNIT;
    return -1;
  }

  if (write_masked(bus, addr, FW_BNO_REG_OPR_MODE, FW_BNO_OPR_MODE_MASK, FW_BNO_OPR_ACTIVE, code) != 0) {
    fact->fail = FW_BNO_FAIL_I2C;
    return -1;
  }
  fw_hw_delay_ms(FW_BNO_OPR_SWITCH_MS);

  started = fw_hw_tick_ms();
  while ((fw_hw_tick_ms() - started) < FW_BNO_FUSION_WAIT_MS) {
    uint8_t st = 0;
    uint8_t er = 0;

    /* Статус читается независимо от значений вектора. Нули в Euler допустимы,
       а раннее успешное чтение OPR_MODE не доказывает сохранение режима. */
    if (fw_hw_i2c_read_reg(bus, addr, FW_BNO_REG_OPR_MODE, &opr) != FW_HW_OK ||
        fw_hw_i2c_read_reg(bus, addr, FW_BNO_REG_SYS_STATUS, &st) != FW_HW_OK ||
        fw_hw_i2c_read_reg(bus, addr, FW_BNO_REG_SYS_ERR, &er) != FW_HW_OK) {
      fact->fail = FW_BNO_FAIL_STATUS_IO;
      *code = FW_DIAG_CODE_I2C;
      return -1;
    }
    fact->opr_mode = opr;
    fact->sys_status = st;
    fact->sys_err = er;
    if (st == FW_BNO_SYS_ERROR || er != 0u) {
      fact->fail = FW_BNO_FAIL_STATUS;
      *code = FW_DIAG_CODE_STATUS;
      return -1;
    }
    if ((opr & FW_BNO_OPR_MODE_MASK) != FW_BNO_OPR_ACTIVE) {
      fact->fail = FW_BNO_FAIL_MODE;
      *code = FW_DIAG_CODE_MODE;
      return -1;
    }
    if (st != FW_BNO_SYS_FUSION) {
      fw_hw_delay_ms(FW_BNO_FUSION_PERIOD_MS);
      continue;
    }
    /* Проверять тот же выход, который читается при измерениях, как в простом
       примере библиотеки. Дополнительного условия «кватернион не нулевой» нет. */
    if (euler_store(bus, addr, eul) != 0) {
      fact->fail = FW_BNO_FAIL_I2C;
      *code = FW_DIAG_CODE_I2C;
      return -1;
    }
    (void)calib_store(bus, addr, calib);
    fact->fail = FW_BNO_FAIL_NONE;
    return 0;
  }
  fact->fail = FW_BNO_FAIL_STATUS;
  *code = FW_DIAG_CODE_STATUS;
  return -1;
}

static int config_sensor(fw_hw_i2c_bus_t bus, uint8_t addr, fw_bno_fact_t *fact, const char **code)
{
  unsigned attempt;

  if (fw_hw_i2c_probe(bus, addr) != FW_HW_OK ||
      fw_hw_i2c_write_reg(bus, addr, FW_BNO_REG_PAGE_ID, 0u) != FW_HW_OK ||
      fw_hw_i2c_read_reg(bus, addr, FW_BNO_REG_CHIP_ID, &fact->chip_id) != FW_HW_OK) {
    fact->fail = FW_BNO_FAIL_I2C;
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }
  if (fact->chip_id != FW_BNO_CHIP_ID) {
    fact->fail = FW_BNO_FAIL_ID;
    *code = FW_DIAG_CODE_ID;
    return -1;
  }
  if (reset_sensor(bus, addr, fact, code) != 0) {
    return -1;
  }

  for (attempt = 0u; attempt <= FW_BNO_MODE_RETRIES; attempt++) {
    fact->mode_retries = (uint8_t)attempt;
    if (start_fusion(bus, addr, fact, code) == 0) {
      return 0;
    }
    /* На плате наблюдалось 0x0C -> 0x10, SYS_STATUS=0; поздняя запись NDOF
       запускала fusion. Только этот возврат в CONFIG допускает повторную
       настройку. Ошибки I2C, SYS_ERR и другие режимы не скрываются повтором. */
    if (fact->fail != FW_BNO_FAIL_MODE ||
        (fact->opr_mode & FW_BNO_OPR_MODE_MASK) != FW_BNO_OPR_CONFIG) {
      return -1;
    }
  }
  return -1;
}

static void note_bus_fail(fw_bno_fact_t *fact, fw_hw_i2c_bus_t bus, int how)
{
  fact->bus = (uint8_t)bus;
  fact->addr7 = 0u;
  if (how == -2) {
    fact->fail = FW_BNO_FAIL_I2C;
    return;
  }
  fact->fail = FW_BNO_FAIL_COUNT;
}

int FW_angle_init(const char **module, const char **code)
{
  uint8_t addr_i = 0u;
  uint8_t addr_e = FW_BNO_ADDR_EXT;
  int found;

  *module = FW_DIAG_MODULE_ANGLE_INT;
  *code = FW_DIAG_CODE_MAP;
  g_bno_internal = (fw_bno_hit_t){0};
  g_bno_external = (fw_bno_hit_t){0};
  g_eul_internal = (fw_bno_eul_t){0};
  g_eul_external = (fw_bno_eul_t){0};
  g_calib_internal = (fw_bno_calib_t){0};
  g_calib_external = (fw_bno_calib_t){0};
  s_live_ms = fw_hw_tick_ms();
  s_fact_int = (fw_bno_fact_t){0};
  s_fact_ext = (fw_bno_fact_t){0};
  s_fact_int.fail = FW_BNO_FAIL_MAP;
  s_fact_ext.fail = FW_BNO_FAIL_MAP;

  if (!fw_hw_i2c_ready()) {
    s_fact_int.fail = FW_BNO_FAIL_I2C;
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }

  /* Дать обоим BNO загрузиться перед адресным программным сбросом. */
  fw_hw_bno_reset_release(FW_HW_BNO_INTERNAL);
  fw_hw_bno_reset_release(FW_HW_BNO_EXTERNAL);
  fw_hw_delay_ms(FW_BNO_TPOR_MS);

  found = find_addr((fw_hw_i2c_bus_t)FW_BNO_BUS_INT, &addr_i, &s_fact_int, code);
  if (found != 0) {
    note_bus_fail(&s_fact_int, (fw_hw_i2c_bus_t)FW_BNO_BUS_INT, found);
    return -1;
  }
  s_fact_int.bus = FW_BNO_BUS_INT;
  s_fact_int.addr7 = addr_i;
  s_fact_int.fail = FW_BNO_FAIL_I2C;
  if (config_sensor((fw_hw_i2c_bus_t)FW_BNO_BUS_INT, addr_i, &s_fact_int, code) != 0) {
    return -1;
  }

  *module = FW_DIAG_MODULE_ANGLE_EXT;
  {
    fw_hw_status_t st = fw_hw_i2c_probe((fw_hw_i2c_bus_t)FW_BNO_BUS_EXT, addr_e);

    if (st == FW_HW_ABSENT) {
      *code = FW_DIAG_CODE_MAP;
      note_bus_fail(&s_fact_ext, (fw_hw_i2c_bus_t)FW_BNO_BUS_EXT, -1);
      return -1;
    }
    if (st != FW_HW_OK) {
      *code = FW_DIAG_CODE_I2C;
      note_bus_fail(&s_fact_ext, (fw_hw_i2c_bus_t)FW_BNO_BUS_EXT, -2);
      return -1;
    }
  }
  s_fact_ext.bus = FW_BNO_BUS_EXT;
  s_fact_ext.addr7 = addr_e;
  s_fact_ext.fail = FW_BNO_FAIL_I2C;
  if (config_sensor((fw_hw_i2c_bus_t)FW_BNO_BUS_EXT, addr_e, &s_fact_ext, code) != 0) {
    return -1;
  }

  g_bno_internal.bus = (fw_hw_i2c_bus_t)FW_BNO_BUS_INT;
  g_bno_internal.addr7 = addr_i;
  g_bno_external.bus = (fw_hw_i2c_bus_t)FW_BNO_BUS_EXT;
  g_bno_external.addr7 = addr_e;
  return 0;
}

void FW_angle_facts(fw_bno_fact_t *internal, fw_bno_fact_t *external)
{
  if (internal != 0) {
    *internal = s_fact_int;
  }
  if (external != 0) {
    *external = s_fact_ext;
  }
}

void FW_angle_meas_begin(void)
{
}

void FW_angle_meas_end(void)
{
}

int FW_angle_window(uint16_t *angle_x10)
{
  float deg;

  if (angle_x10 == 0) {
    return -1;
  }
  if (heading_diff_deg(&deg) != 0) {
    return -1;
  }
  return deg_to_x10(deg, angle_x10);
}

void FW_angle_poll(void)
{
  uint32_t now = fw_hw_tick_ms();

  if (FW_states_mode() == FW_MODE_MEASUREMENT) {
    return;
  }
  if ((now - s_live_ms) < FW_BNO_LIVE_MS) {
    return;
  }
  s_live_ms = now;

  (void)euler_store(g_bno_internal.bus, g_bno_internal.addr7, &g_eul_internal);
  (void)calib_store(g_bno_internal.bus, g_bno_internal.addr7, &g_calib_internal);
  (void)euler_store(g_bno_external.bus, g_bno_external.addr7, &g_eul_external);
  (void)calib_store(g_bno_external.bus, g_bno_external.addr7, &g_calib_external);
}
