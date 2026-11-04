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

static int find_addr(fw_hw_i2c_bus_t bus, uint8_t *addr, const char **code)
{
  static const uint8_t addrs[FW_BNO_ADDR_COUNT] = { FW_BNO_ADDR_COM3_LOW, FW_BNO_ADDR_COM3_HIGH };
  int n = 0;
  uint8_t found = 0u;
  int ai;

  for (ai = 0; ai < FW_BNO_ADDR_COUNT; ai++) {
    fw_hw_status_t st = fw_hw_i2c_probe(bus, addrs[ai]);

    if (st == FW_HW_ABSENT) {
      continue;
    }
    if (st != FW_HW_OK) {
      *code = FW_DIAG_CODE_I2C;
      return -2;
    }
    found = addrs[ai];
    n++;
  }
  if (n != 1) {
    *code = FW_DIAG_CODE_MAP;
    return -1;
  }
  *addr = found;
  return 0;
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

static int orientation_present(fw_hw_i2c_bus_t bus, uint8_t addr)
{
  bno055_vector_t v;

  bno055_select(bus, addr);
  v = bno055_getVectorEuler();
  if (!bno055_io_ok()) {
    return 0;
  }
  /* После CONFIG нули; любой ненулевой Эйлер — fusion уже отдаёт ориентацию. */
  return (v.x != 0.0 || v.y != 0.0 || v.z != 0.0) ? 1 : 0;
}

static int config_sensor(fw_hw_i2c_bus_t bus, uint8_t addr, fw_bno_fact_t *fact, const char **code)
{
  uint8_t unit = 0;
  uint8_t opr = 0;
  volatile fw_bno_calib_t *calib;
  uint32_t waited = 0;

  if (bus == (fw_hw_i2c_bus_t)FW_BNO_BUS_INT) {
    calib = &g_calib_internal;
  } else {
    calib = &g_calib_external;
  }

  if (fw_hw_i2c_probe(bus, addr) != FW_HW_OK) {
    fact->fail = FW_BNO_FAIL_I2C;
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }
  if (write_masked(bus, addr, FW_BNO_REG_OPR_MODE, FW_BNO_OPR_MODE_MASK, FW_BNO_OPR_CONFIG, code) != 0) {
    fact->fail = FW_BNO_FAIL_MODE;
    return -1;
  }
  fw_hw_delay_ms(FW_BNO_CONFIG_SWITCH_MS);

  if (write_masked(bus, addr, FW_BNO_REG_PWR_MODE, FW_BNO_PWR_MODE_MASK, FW_BNO_PWR_NORMAL, code) != 0) {
    fact->fail = FW_BNO_FAIL_PWR;
    return -1;
  }
  if (fw_hw_i2c_write_reg(bus, addr, FW_BNO_REG_UNIT_SEL, FW_BNO_UNIT_SEL) != FW_HW_OK) {
    fact->fail = FW_BNO_FAIL_UNIT;
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }
  if (fw_hw_i2c_read_reg(bus, addr, FW_BNO_REG_UNIT_SEL, &unit) != FW_HW_OK) {
    fact->fail = FW_BNO_FAIL_UNIT;
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }
  fact->unit = unit;
  fact->unit_known = 1u;
  if (unit != FW_BNO_UNIT_SEL) {
    fact->fail = FW_BNO_FAIL_UNIT;
    *code = FW_DIAG_CODE_UNIT;
    return -1;
  }

  if (write_masked(bus, addr, FW_BNO_REG_OPR_MODE, FW_BNO_OPR_MODE_MASK, FW_BNO_OPR_ACTIVE, code) != 0) {
    fact->fail = FW_BNO_FAIL_MODE;
    return -1;
  }
  fw_hw_delay_ms(FW_BNO_OPR_SWITCH_MS);
  if (fw_hw_i2c_read_reg(bus, addr, FW_BNO_REG_OPR_MODE, &opr) != FW_HW_OK) {
    fact->fail = FW_BNO_FAIL_MODE;
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }
  if ((opr & FW_BNO_OPR_MODE_MASK) != FW_BNO_OPR_ACTIVE) {
    fact->fail = FW_BNO_FAIL_MODE;
    *code = FW_DIAG_CODE_MODE;
    return -1;
  }

  while (waited < FW_BNO_QUAT_WAIT_MS) {
    uint8_t st = 0;
    uint8_t er = 0;

    fw_hw_delay_ms(FW_BNO_FUSION_PERIOD_MS);
    waited += FW_BNO_FUSION_PERIOD_MS;
    if (!orientation_present(bus, addr)) {
      continue;
    }
    if (fw_hw_i2c_read_reg(bus, addr, FW_BNO_REG_SYS_STATUS, &st) != FW_HW_OK ||
        fw_hw_i2c_read_reg(bus, addr, FW_BNO_REG_SYS_ERR, &er) != FW_HW_OK) {
      fact->fail = FW_BNO_FAIL_STATUS_IO;
      *code = FW_DIAG_CODE_I2C;
      return -1;
    }
    fact->sys_status = st;
    fact->sys_err = er;
    if (st != FW_BNO_SYS_FUSION) {
      fact->fail = FW_BNO_FAIL_STATUS;
      *code = FW_DIAG_CODE_STATUS;
      return -1;
    }
    (void)calib_store(bus, addr, calib);
    fact->fail = FW_BNO_FAIL_NONE;
    return 0;
  }
  fact->fail = FW_BNO_FAIL_QUAT;
  *code = FW_DIAG_CODE_QUAT;
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
  s_fact_int.bus = 0u;
  s_fact_int.addr7 = 0u;
  s_fact_int.unit_known = 0u;
  s_fact_int.fail = FW_BNO_FAIL_MAP;
  s_fact_ext.bus = 0u;
  s_fact_ext.addr7 = 0u;
  s_fact_ext.unit_known = 0u;
  s_fact_ext.fail = FW_BNO_FAIL_MAP;

  if (!fw_hw_i2c_ready()) {
    s_fact_int.fail = FW_BNO_FAIL_I2C;
    *code = FW_DIAG_CODE_I2C;
    return -1;
  }

  fw_hw_bno_reset_release(FW_HW_BNO_INTERNAL);
  fw_hw_bno_reset_release(FW_HW_BNO_EXTERNAL);
  fw_hw_delay_ms(FW_BNO_TPOR_MS);

  found = find_addr((fw_hw_i2c_bus_t)FW_BNO_BUS_INT, &addr_i, code);
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
