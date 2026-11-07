#include "bno_debug.h"
#include "bno055_port.h"
#include "fw_config.h"
#include "main.h"
#include <math.h>

#define MODE_MASK 0x0Fu
#define CLOCK_EXTERNAL 0x80u
#define CLOCK_SWITCHING 0x01u
/* Orientation convention and the Euler, gyro and acceleration units. */
#define ORIENTATION_UNIT_MASK 0x87u
#define UNIT_ANDROID_DEG_MS2 0x80u
#define RAD_TO_DEG 57.2957795f

typedef struct {
  fw_hw_i2c_bus_t bus;
  uint8_t use_crystal, axis_map, axis_sign;
} sensor_config_t;

static const sensor_config_t internal = {
  FW_HW_I2C1, BNO_INTERNAL_USE_CRYSTAL, BNO_INTERNAL_AXIS_MAP, BNO_INTERNAL_AXIS_SIGN
};
static const sensor_config_t external = {
  FW_HW_I2C2, BNO_EXTERNAL_USE_CRYSTAL, BNO_EXTERNAL_AXIS_MAP, BNO_EXTERNAL_AXIS_SIGN
};

volatile bno_debug_sensor_t g_bno_internal, g_bno_external;
volatile bno_debug_pair_t g_bno_pair = {
  .heading_delta_deg = NAN, .orientation_delta_deg = NAN, .segment_angle_deg = NAN,
  .reported_heading_delta_deg = NAN
};

static int read_reg(const sensor_config_t *s, const bno_debug_sensor_t *d,
                    uint8_t reg, uint8_t *value)
{
  return fw_hw_i2c_read_reg(s->bus, d->addr7, reg, value) == FW_HW_OK;
}

static int write_reg(const sensor_config_t *s, const bno_debug_sensor_t *d,
                     uint8_t reg, uint8_t value)
{
  return fw_hw_i2c_write_reg(s->bus, d->addr7, reg, value) == FW_HW_OK;
}

/* Keep upper bits of OPR_MODE; they are not part of the mode field. */
static int write_mode(const sensor_config_t *s, bno_debug_sensor_t *d, uint8_t mode)
{
  return read_reg(s, d, BNO055_OPR_MODE, &d->opr_mode) &&
         write_reg(s, d, BNO055_OPR_MODE, (d->opr_mode & ~MODE_MASK) | mode);
}

static bno_init_result_t detect(const sensor_config_t *s, bno_debug_sensor_t *d)
{
  uint32_t start = HAL_GetTick();
  do {
    d->address_mask = 0;
    for (uint8_t addr = 0x28u; addr <= 0x29u; ++addr) {
      fw_hw_status_t result = fw_hw_i2c_probe(s->bus, addr);
      if (result == FW_HW_OK) {
        d->address_mask |= (uint8_t)(1u << (addr - 0x28u));
      } else if (result != FW_HW_ABSENT) {
        return BNO_INIT_IO;
      }
    }
    if (d->address_mask == 1u || d->address_mask == 2u) {
      uint8_t page;
      d->addr7 = d->address_mask == 1u ? 0x28u : 0x29u;
      if (!read_reg(s, d, BNO055_PAGE_ID, &page)) {
        return BNO_INIT_IO;
      }
      if (page != 0u) {
        return BNO_INIT_CONFIG;
      }
      if (!read_reg(s, d, BNO055_CHIP_ID, &d->chip_id)) {
        return BNO_INIT_IO;
      }
      return d->chip_id == 0xA0u ? BNO_INIT_OK : BNO_INIT_CHIP_ID;
    }
    if (d->address_mask == 3u) {
      break;
    }
    HAL_Delay(20);
  } while ((HAL_GetTick() - start) < 1000u);
  return BNO_INIT_ADDRESS;
}

static bno_init_result_t select_clock(const sensor_config_t *s, bno_debug_sensor_t *d,
                                      uint8_t external_clock)
{
  const uint8_t value = external_clock ? CLOCK_EXTERNAL : 0u;
  const uint32_t start = HAL_GetTick();
  /* Only CLK_SEL is written; in particular RST_SYS is always zero. */
  if (!write_reg(s, d, BNO055_SYS_TRIGGER, value)) {
    return BNO_INIT_IO;
  }
  HAL_Delay(external_clock ? 700u : 10u);
  do {
    if (!read_reg(s, d, BNO055_SYS_CLK_STATUS, &d->sys_clk_status) ||
        !read_reg(s, d, BNO055_SYS_TRIGGER, &d->sys_trigger)) {
      return BNO_INIT_IO;
    }
    d->crystal_active = (d->sys_trigger & CLOCK_EXTERNAL) != 0;
    if (!(d->sys_clk_status & CLOCK_SWITCHING) && d->crystal_active == external_clock) {
      return BNO_INIT_OK;
    }
    HAL_Delay(10);
  } while ((HAL_GetTick() - start) < 2000u);
  return BNO_INIT_CLOCK;
}

static bno_init_result_t setup(const sensor_config_t *s, bno_debug_sensor_t *d)
{
  bno_init_result_t result = detect(s, d);
  if (result != BNO_INIT_OK) {
    return result;
  }
  if (!read_reg(s, d, BNO055_OPR_MODE, &d->opr_mode)) {
    return BNO_INIT_IO;
  }
  if ((d->opr_mode & MODE_MASK) != BNO055_OPERATION_MODE_CONFIG) {
    return BNO_INIT_CONFIG;
  }

  if (!read_reg(s, d, BNO055_SYS_CLK_STATUS, &d->sys_clk_status) ||
      !read_reg(s, d, BNO055_SYS_TRIGGER, &d->sys_trigger)) {
    return BNO_INIT_IO;
  }
  d->crystal_active = (d->sys_trigger & CLOCK_EXTERNAL) != 0;
  if ((d->sys_clk_status & CLOCK_SWITCHING) || d->crystal_active) {
    return BNO_INIT_CLOCK;
  }
  if (s->use_crystal) {
    result = select_clock(s, d, 1);
    if (result != BNO_INIT_OK) {
      d->clock_fallback = 1;
      result = select_clock(s, d, 0);
    }
    if (result != BNO_INIT_OK) {
      return result;
    }
  }
  if (!read_reg(s, d, BNO055_PWR_MODE, &d->pwr_mode) ||
      !read_reg(s, d, BNO055_UNIT_SEL, &d->unit_sel)) {
    return BNO_INIT_IO;
  }
  if ((d->pwr_mode & 0x03u) != 0) {
    return BNO_INIT_POWER;
  }
  if ((d->unit_sel & ORIENTATION_UNIT_MASK) != UNIT_ANDROID_DEG_MS2) {
    if (!write_reg(s, d, BNO055_UNIT_SEL, UNIT_ANDROID_DEG_MS2)) {
      return BNO_INIT_IO;
    }
    HAL_Delay(10u);
    if (!read_reg(s, d, BNO055_UNIT_SEL, &d->unit_sel)) {
      return BNO_INIT_IO;
    }
    if ((d->unit_sel & ORIENTATION_UNIT_MASK) != UNIT_ANDROID_DEG_MS2) {
      return BNO_INIT_UNITS;
    }
  }
  if (!read_reg(s, d, BNO055_AXIS_MAP_CONFIG, &d->axis_map) ||
      !read_reg(s, d, BNO055_AXIS_MAP_SIGN, &d->axis_sign)) {
    return BNO_INIT_IO;
  }
  if ((d->axis_map & 0x3Fu) != s->axis_map || (d->axis_sign & 0x07u) != s->axis_sign) {
    if (!write_reg(s, d, BNO055_AXIS_MAP_CONFIG, s->axis_map) ||
        !write_reg(s, d, BNO055_AXIS_MAP_SIGN, s->axis_sign) ||
        !read_reg(s, d, BNO055_AXIS_MAP_CONFIG, &d->axis_map) ||
        !read_reg(s, d, BNO055_AXIS_MAP_SIGN, &d->axis_sign)) {
      return BNO_INIT_IO;
    }
    if ((d->axis_map & 0x3Fu) != s->axis_map || (d->axis_sign & 0x07u) != s->axis_sign) {
      return BNO_INIT_AXIS_MAP;
    }
  }

  /* The previous firmware sometimes read CONFIG back after the first NDOF write.
     Wait for the mode to settle and retry once if it reverted to CONFIG. */
  for (unsigned attempt = 0; attempt < 2; ++attempt) {
    uint32_t start;
    if (!write_mode(s, d, BNO055_OPERATION_MODE_NDOF)) {
      return BNO_INIT_IO;
    }
    HAL_Delay(1000u);
    start = HAL_GetTick();
    do {
      if (!read_reg(s, d, BNO055_OPR_MODE, &d->opr_mode) ||
          !read_reg(s, d, BNO055_SYS_STATUS, &d->sys_status) ||
          !read_reg(s, d, BNO055_SYS_ERR, &d->sys_err)) {
        return BNO_INIT_IO;
      }
      if (d->sys_err != 0 || d->sys_status == BNO055_SYSTEM_STATUS_SYSTEM_ERROR) {
        return BNO_INIT_FUSION;
      }
      if ((d->opr_mode & MODE_MASK) != BNO055_OPERATION_MODE_NDOF) {
        break;
      }
      if (d->sys_status == BNO055_SYSTEM_STATUS_FUSION_ALGO_RUNNING) {
        return BNO_INIT_OK;
      }
      HAL_Delay(10);
    } while ((HAL_GetTick() - start) < 1000u);

    if ((d->opr_mode & MODE_MASK) == BNO055_OPERATION_MODE_NDOF) {
      return BNO_INIT_FUSION;
    }
    if ((d->opr_mode & MODE_MASK) != BNO055_OPERATION_MODE_CONFIG) {
      break;
    }
  }
  return BNO_INIT_MODE;
}

static void init_one(const sensor_config_t *s, volatile bno_debug_sensor_t *watch)
{
  bno_debug_sensor_t d = { .crystal_requested = s->use_crystal };
  d.init_result = setup(s, &d);
  *watch = d;
}

void bno_debug_init(void)
{
  init_one(&internal, &g_bno_internal);
  init_one(&external, &g_bno_external);
}

static int read_sample(const sensor_config_t *s, bno_debug_sensor_t *d)
{
  uint8_t status[BNO055_SYS_TRIGGER - BNO055_CALIB_STAT + 1];
  bno055_vector_t euler;

  bno055_select(s->bus, d->addr7);
  bno055_getOrientation(&euler, &d->quaternion);
  d->accel_ms2 = bno055_getVectorAccelerometer();
  d->gyro_dps = bno055_getVectorGyroscope();
  d->mag_ut = bno055_getVectorMagnetometer();
  bno055_readData(BNO055_CALIB_STAT, status, sizeof status);
  if (!bno055_io_ok()) {
    return 0;
  }

  d->euler_deg = (bno_euler_deg_t){euler.x, euler.y, euler.z};
  const float x = d->accel_ms2.x, y = d->accel_ms2.y, z = d->accel_ms2.z;
  d->accel_norm_ms2 = sqrtf(x*x + y*y + z*z);
  d->calib_raw = status[0];
  d->calib.sys = (d->calib_raw >> 6) & 3u;
  d->calib.gyro = (d->calib_raw >> 4) & 3u;
  d->calib.accel = (d->calib_raw >> 2) & 3u;
  d->calib.mag = d->calib_raw & 3u;
  d->self_test = status[BNO055_ST_RESULT - BNO055_CALIB_STAT];
  d->sys_clk_status = status[BNO055_SYS_CLK_STATUS - BNO055_CALIB_STAT];
  d->sys_status = status[BNO055_SYS_STATUS - BNO055_CALIB_STAT];
  d->sys_err = status[BNO055_SYS_ERR - BNO055_CALIB_STAT];
  d->unit_sel = status[BNO055_UNIT_SEL - BNO055_CALIB_STAT];
  d->opr_mode = status[BNO055_OPR_MODE - BNO055_CALIB_STAT];
  d->pwr_mode = status[BNO055_PWR_MODE - BNO055_CALIB_STAT];
  d->sys_trigger = status[BNO055_SYS_TRIGGER - BNO055_CALIB_STAT];
  d->crystal_active = (d->sys_trigger & CLOCK_EXTERNAL) != 0;
  d->fusion_running = (d->opr_mode & MODE_MASK) == BNO055_OPERATION_MODE_NDOF &&
                      d->sys_status == BNO055_SYSTEM_STATUS_FUSION_ALGO_RUNNING &&
                      d->sys_err == 0 &&
                      (d->unit_sel & ORIENTATION_UNIT_MASK) == UNIT_ANDROID_DEG_MS2;
  d->north_reference = d->fusion_running && d->calib.sys != 0;
  d->fully_calibrated = d->fusion_running && d->calib_raw == 0xFFu;
  return 1;
}

static void sample_one(const sensor_config_t *s, volatile bno_debug_sensor_t *watch)
{
  if (watch->init_result != BNO_INIT_OK) {
    return;
  }
  bno_debug_sensor_t next = *watch;
  if (read_sample(s, &next)) {
    next.sample_ok = 1;
    next.sampled_ms = HAL_GetTick();
    *watch = next;
  } else {
    /* Keep the previous numbers and timestamp; invalidate their status. */
    watch->sample_ok = 0;
    watch->north_reference = 0;
    watch->fusion_running = 0;
    watch->fully_calibrated = 0;
    ++watch->io_errors;
  }
}

typedef struct { float x, y, z; } vec3_t;
typedef struct { float w, x, y, z; } quat_t;

static float norm(vec3_t a)
{
  return sqrtf(a.x*a.x + a.y*a.y + a.z*a.z);
}

static vec3_t cross(vec3_t a, vec3_t b)
{
  return (vec3_t){a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}

static int usable_quat(quat_t a)
{
  return a.w*a.w + a.x*a.x + a.y*a.y + a.z*a.z > 0.25f;
}

static quat_t quat_float(bno055_vector_t q)
{
  return (quat_t){q.w, q.x, q.y, q.z};
}

static vec3_t leg_direction(quat_t q, int signed_axis)
{
  float a[3] = {0, 0, 0};
  const int sign = signed_axis < 0 ? -1 : 1;
  a[sign * signed_axis - 1] = (float)sign;
  const vec3_t u = {q.x, q.y, q.z};
  const vec3_t v = {a[0], a[1], a[2]};
  const vec3_t c = cross(u, v);
  const float along = 2.0f * (u.x*v.x + u.y*v.y + u.z*v.z);
  const float diag = q.w*q.w - (u.x*u.x + u.y*u.y + u.z*u.z);
  return (vec3_t){diag*v.x + along*u.x + 2.0f*q.w*c.x,
                  diag*v.y + along*u.y + 2.0f*q.w*c.y,
                  diag*v.z + along*u.z + 2.0f*q.w*c.z};
}

static int axis_valid(int a)
{
  return a >= -3 && a <= 3 && a != 0;
}

static void update_pair(void)
{
  bno_debug_pair_t p = {
    .sampled_ms = HAL_GetTick(),
    .heading_delta_deg = NAN, .orientation_delta_deg = NAN, .segment_angle_deg = NAN,
    .reported_heading_delta_deg = NAN
  };
  p.north_ready = g_bno_internal.north_reference && g_bno_external.north_reference;
  p.both_fully_calibrated = g_bno_internal.fully_calibrated && g_bno_external.fully_calibrated;

  if (g_bno_internal.fusion_running && g_bno_external.fusion_running) {
    float delta = g_bno_external.euler_deg.heading - g_bno_internal.euler_deg.heading;
    if (delta >= 180.0f) {
      delta -= 360.0f;
    } else if (delta < -180.0f) {
      delta += 360.0f;
    }
    p.reported_heading_delta_deg = delta;
  }

  if (p.north_ready) {
    p.heading_delta_deg = p.reported_heading_delta_deg;
    const quat_t a = quat_float(g_bno_internal.quaternion);
    const quat_t b = quat_float(g_bno_external.quaternion);
    if (usable_quat(a) && usable_quat(b)) {
      /* The relative rotation is conjugate(a) * b. Its scale cancels in atan2. */
      const vec3_t rot = {
        a.w*b.x - a.x*b.w - a.y*b.z + a.z*b.y,
        a.w*b.y - a.y*b.w - a.z*b.x + a.x*b.z,
        a.w*b.z - a.z*b.w - a.x*b.y + a.y*b.x
      };
      const float dot = a.w*b.w + a.x*b.x + a.y*b.y + a.z*b.z;
      p.orientation_delta_deg = 2.0f * RAD_TO_DEG * atan2f(norm(rot), fabsf(dot));
      if (axis_valid(BNO_INTERNAL_LEG_AXIS) && axis_valid(BNO_EXTERNAL_LEG_AXIS)) {
        const vec3_t va = leg_direction(a, BNO_INTERNAL_LEG_AXIS);
        const vec3_t vb = leg_direction(b, BNO_EXTERNAL_LEG_AXIS);
        p.segment_angle_deg = RAD_TO_DEG * atan2f(norm(cross(va, vb)),
                                                  va.x*vb.x + va.y*vb.y + va.z*vb.z);
      }
    }
  }
  g_bno_pair = p;
}

void bno_debug_sample(void)
{
  sample_one(&internal, &g_bno_internal);
  sample_one(&external, &g_bno_external);
  update_pair();
}
