/* The real FW_angle, library and port run against two independent register maps.
   No sensor fusion is simulated: these tests cannot prove physical heading stability. */
#include "FW_angle.h"
#include "FW_ble.h"
#include "FW_emg.h"
#include "FW_states.h"
#include "FW_sync.h"
#include "bno055_port.h"
#include "fw_config.h"
#include <string.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

/* MSVC ABI floating-point marker; this freestanding test needs no CRT. */
int _fltused;

typedef struct {
  uint8_t reg[2][128];
  uint8_t page;
  uint8_t addr;
  unsigned reset_holds;
  unsigned software_resets;
  unsigned internal_clock_writes;
  unsigned normal_power_writes;
  unsigned active_writes;
  unsigned late_defaults;
  uint32_t software_reset_at;
  uint32_t first_boot_read_at;
  uint32_t internal_clock_at;
  uint32_t fusion_at;
  uint32_t defaults_at;
} sensor_t;

static sensor_t sensor[2];
static uint32_t ticks;
static fw_mode_t mode = FW_MODE_ADVERTISING;
static int fail_bus, fail_reg = -1, fail_write;
static int bad_status_bus, error_bus, wrong_power_bus;
static uint32_t ready_at[2];
static uint32_t software_boot_delay_ms;
static int reset_nack_bus;
static uint32_t internal_probe_at;
static int internal_address_conflict;
static uint32_t fusion_delay_ms;
static int fusion_never_ready;
static uint32_t late_defaults_ms;
static int lose_every_mode_write;
static int fail_active_bus;
static int bad_crystal_bus;
static uint8_t calibration_at_start[2];
static uint8_t internal_reset_unit;
static unsigned sent_frames, sent_len;
static uint8_t sent_frame[FW_FRAME_SAMPLE_BYTES];
static int calls;

static void reset_registers(sensor_t *s);

static void finish_boot(sensor_t *s)
{
  if (s->defaults_at != 0u && ticks >= s->defaults_at) {
    s->defaults_at = 0u;
    reset_registers(s);
    s->late_defaults++;
  }
}

void *memcpy(void *dest, const void *src, size_t count)
{
  uint8_t *d = dest;
  const uint8_t *s = src;
  while (count-- != 0u) *d++ = *s++;
  return dest;
}

void *memset(void *dest, int value, size_t count)
{
  uint8_t *d = dest;
  while (count-- != 0u) *d++ = (uint8_t)value;
  return dest;
}

int strcmp(const char *left, const char *right)
{
  while (*left != 0 && *left == *right) { left++; right++; }
  return (unsigned char)*left - (unsigned char)*right;
}

int printf(const char *format, ...) { (void)format; return 0; }
float fabsf(float value) { return value < 0.0f ? -value : value; }

fw_mode_t FW_states_mode(void) { return mode; }
uint32_t fw_hw_tick_ms(void) { return ticks; }
void fw_hw_delay_ms(uint32_t ms)
{
  ticks += ms;
  finish_boot(&sensor[0]);
  finish_boot(&sensor[1]);
}
int fw_hw_i2c_ready(void) { return 1; }
void fw_hw_bno_reset_hold(fw_hw_bno_id_t id)
{
  sensor[id].reset_holds++;
}

void fw_hw_bno_reset_release(fw_hw_bno_id_t id)
{
  (void)id;
}

int FW_emg_start(void) { return 0; }
void FW_emg_stop(void) {}
int FW_emg_take(uint16_t *dst)
{
  for (unsigned i = 0; i < FW_EMG_FRAME_COUNT; ++i) dst[i] = (uint16_t)(i + 100u);
  return 1;
}
void fw_hw_timer_5ms_start(void) {}
uint32_t fw_hw_timer_5ms_take(void) { return 0; }
uint32_t fw_hw_timer_5ms_ms(void) { return 0x12345678u; }
int FW_ble_send(const uint8_t *data, unsigned len)
{
  sent_len = len;
  if (len > sizeof(sent_frame)) return -1;
  memcpy(sent_frame, data, len);
  sent_frames++;
  return 0;
}

static sensor_t *get_sensor(fw_hw_i2c_bus_t bus, uint8_t addr)
{
  if (bus < FW_HW_I2C1 || bus > FW_HW_I2C2) return 0;
  return sensor[bus - 1].addr == addr ? &sensor[bus - 1] : 0;
}

fw_hw_status_t fw_hw_i2c_probe(fw_hw_i2c_bus_t bus, uint8_t addr)
{
  if (bus < FW_HW_I2C1 || bus > FW_HW_I2C2) return FW_HW_ERR;
  if (ticks < ready_at[bus - 1]) return FW_HW_ABSENT;
  if (bus == FW_HW_I2C1) {
    if (ticks < internal_probe_at) return FW_HW_ABSENT;
    if (internal_address_conflict && (addr == FW_BNO_ADDR_COM3_LOW || addr == FW_BNO_ADDR_COM3_HIGH)) {
      return FW_HW_OK;
    }
  }
  return get_sensor(bus, addr) != 0 ? FW_HW_OK : FW_HW_ABSENT;
}

static void reset_registers(sensor_t *s)
{
  memset(s->reg, 0, sizeof(s->reg));
  s->page = 0;
  s->reg[0][BNO055_CHIP_ID] = BNO055_ID;
  s->reg[0][BNO055_OPR_MODE] = 0x10;
  s->reg[0][BNO055_QUA_DATA_W_LSB + 1u] = 0x40u;
  s->reg[0][BNO055_UNIT_SEL] = 0x80;
  s->reg[0][BNO055_AXIS_MAP_CONFIG] = 0x24;
}

fw_hw_status_t fw_hw_i2c_write_reg(fw_hw_i2c_bus_t bus, uint8_t addr, uint8_t reg, uint8_t value)
{
  sensor_t *s = get_sensor(bus, addr);
  calls++;
  if (s == 0 || ticks < ready_at[bus - 1] || (bus == fail_bus && reg == fail_reg && fail_write)) return FW_HW_ERR;
  finish_boot(s);
  if (s->page == 0 && reg == BNO055_OPR_MODE && bus == fail_active_bus &&
      (value & FW_BNO_OPR_MODE_MASK) == FW_BNO_OPR_ACTIVE) return FW_HW_ERR;
  if (reg == BNO055_PAGE_ID) { s->page = value & 1u; return FW_HW_OK; }
  if (s->page == 0 && reg == BNO055_SYS_TRIGGER && value == FW_BNO_RST_SYS) {
    if ((s->reg[0][BNO055_OPR_MODE] & FW_BNO_OPR_MODE_MASK) != FW_BNO_OPR_CONFIG) return FW_HW_ERR;
    reset_registers(s);
    s->software_resets++;
    s->software_reset_at = ticks;
    s->first_boot_read_at = 0u;
    ready_at[bus - 1] = ticks + software_boot_delay_ms;
    if (bus == FW_HW_I2C1) s->reg[0][BNO055_UNIT_SEL] = internal_reset_unit;
    return bus == reset_nack_bus ? FW_HW_ERR : FW_HW_OK;
  }
  s->reg[s->page][reg] = value;
  if (s->page == 0 && reg == BNO055_SYS_TRIGGER && value == 0u) {
    s->internal_clock_writes++;
    s->internal_clock_at = ticks;
  }
  if (s->page == 0 && reg == BNO055_SYS_TRIGGER && bus == bad_crystal_bus) {
    s->reg[0][reg] &= 0x7Fu; /* BNO falls back if the external crystal did not start. */
  }
  if (s->page == 0 && reg == BNO055_PWR_MODE) {
    if ((value & FW_BNO_PWR_MODE_MASK) == FW_BNO_PWR_NORMAL) s->normal_power_writes++;
    if (bus == wrong_power_bus) s->reg[0][reg] = 2;
  }
  if (s->page == 0 && reg == BNO055_OPR_MODE &&
      (value & FW_BNO_OPR_MODE_MASK) == FW_BNO_OPR_ACTIVE) {
    s->active_writes++;
    if (s->active_writes == 1 && late_defaults_ms != 0u) s->defaults_at = ticks + late_defaults_ms;
    if (lose_every_mode_write) s->defaults_at = ticks + 20u;
    s->reg[0][BNO055_SYS_STATUS] = bus == bad_status_bus ? 1 : 3;
    s->reg[0][BNO055_SYS_ERR] = bus == error_bus ? 3 : 0;
    s->fusion_at = ticks + fusion_delay_ms;
    /* Euler is the measured output. Leave quaternion registers zero so the
       tests reject any reintroduction of an unrelated nonzero-vector gate. */
    s->reg[0][BNO055_CALIB_STAT] = calibration_at_start[bus - 1];
  }
  return FW_HW_OK;
}

fw_hw_status_t fw_hw_i2c_read(fw_hw_i2c_bus_t bus, uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)
{
  sensor_t *s = get_sensor(bus, addr);
  calls++;
  if (s == 0 || reg + len > 128) return FW_HW_ERR;
  finish_boot(s);
  if (reg == BNO055_CHIP_ID && s->software_resets != 0u && s->first_boot_read_at == 0u) {
    s->first_boot_read_at = ticks;
  }
  if (ticks < ready_at[bus - 1]) return FW_HW_ERR;
  if (s->page == 0 && reg == BNO055_SYS_STATUS &&
      (s->reg[0][BNO055_OPR_MODE] & FW_BNO_OPR_MODE_MASK) == FW_BNO_OPR_ACTIVE &&
      bus != bad_status_bus && !fusion_never_ready && ticks >= s->fusion_at) {
    s->reg[0][BNO055_SYS_STATUS] = 5;
  }
  if (bus == fail_bus && reg == fail_reg && !fail_write) {
    if (len != 0) data[0] = 0xAA; /* A failed transaction may have received a partial value. */
    return FW_HW_ERR;
  }
  if (reg == BNO055_PAGE_ID && len == 1u) {
    *data = s->page;
  } else {
    memcpy(data, &s->reg[s->page][reg], len);
  }
  return FW_HW_OK;
}

fw_hw_status_t fw_hw_i2c_read_reg(fw_hw_i2c_bus_t bus, uint8_t addr, uint8_t reg, uint8_t *value)
{
  return fw_hw_i2c_read(bus, addr, reg, value, 1);
}

static void prepare(void)
{
  memset(sensor, 0, sizeof(sensor));
  sensor[0].addr = FW_BNO_ADDR_COM3_LOW;
  sensor[1].addr = FW_BNO_ADDR_EXT;
  reset_registers(&sensor[0]);
  reset_registers(&sensor[1]);
  fail_bus = 0; fail_reg = -1; fail_write = 0;
  bad_status_bus = error_bus = wrong_power_bus = 0;
  ready_at[0] = ready_at[1] = 0u;
  software_boot_delay_ms = 0u;
  reset_nack_bus = 0;
  internal_probe_at = 0u;
  internal_address_conflict = 0;
  fusion_delay_ms = 0u; fusion_never_ready = 0;
  late_defaults_ms = 0u; lose_every_mode_write = 0;
  fail_active_bus = 0;
  bad_crystal_bus = 0;
  calibration_at_start[0] = calibration_at_start[1] = 0xFF;
  internal_reset_unit = 0x80u;
  sent_frames = sent_len = 0;
  mode = FW_MODE_ADVERTISING;
  ticks = 0;
}

static void set_euler(unsigned index, int heading, int roll, int pitch)
{
  int values[3] = { heading, roll, pitch };
  for (unsigned i = 0; i < 3; i++) {
    uint16_t raw = (uint16_t)(int16_t)values[i];
    sensor[index].reg[0][BNO055_EUL_HEADING_LSB + 2*i] = (uint8_t)raw;
    sensor[index].reg[0][BNO055_EUL_HEADING_LSB + 2*i + 1] = (uint8_t)(raw >> 8);
  }
}

int main(void)
{
  const char *module, *code;
  fw_bno_fact_t internal, external;
  uint16_t angle;
  bno055_vector_t vector;
  uint8_t buffer[6];

  prepare();
  internal_probe_at = FW_BNO_TPOR_MS + 100u;
  CHECK(FW_angle_init(&module, &code) == 0);
  FW_angle_facts(&internal, &external);
  CHECK(internal.addr7 == FW_BNO_ADDR_COM3_LOW && internal.addr_probe_mask == 1u);

  prepare();
  internal_probe_at = FW_BNO_TPOR_MS + FW_BNO_BOOT_TIMEOUT_MS + FW_BNO_BOOT_POLL_MS;
  CHECK(FW_angle_init(&module, &code) != 0 && strcmp(code, FW_DIAG_CODE_MAP) == 0);
  FW_angle_facts(&internal, &external);
  CHECK(internal.addr_probe_mask == 0u && sensor[0].software_resets == 0u);
  CHECK(ticks == FW_BNO_TPOR_MS + FW_BNO_BOOT_TIMEOUT_MS);

  prepare();
  internal_address_conflict = 1;
  CHECK(FW_angle_init(&module, &code) != 0 && strcmp(code, FW_DIAG_CODE_MAP) == 0);
  FW_angle_facts(&internal, &external);
  CHECK(internal.addr_probe_mask == 3u && sensor[0].software_resets == 0u);
  CHECK(ticks == FW_BNO_TPOR_MS);

  /* Exercise the real sample encoder and sync path with uncalibrated BNOs. */
  prepare();
  calibration_at_start[0] = calibration_at_start[1] = 0u;
  CHECK(FW_angle_init(&module, &code) == 0);
  set_euler(0, 17*16, 0, 0); set_euler(1, 107*16, 0, 0);
  mode = FW_MODE_MEASUREMENT;
  FW_sync_poll();
  CHECK(sent_frames == 1u && sent_len == 47u && sent_frame[0] == 0x03u);
  for (unsigned i = 0; i < FW_EMG_FRAME_COUNT; i++) {
    CHECK(sent_frame[1u + 2u*i] == 100u + i && sent_frame[2u + 2u*i] == 0u);
  }
  CHECK(sent_frame[41] == 0x84u && sent_frame[42] == 0x03u); /* 90.0 degrees */
  CHECK(sent_frame[43] == 0x78u && sent_frame[44] == 0x56u);
  CHECK(sent_frame[45] == 0x34u && sent_frame[46] == 0x12u);

  /* Roll, pitch, missing quaternion and a failed calibration read cannot stop the stream. */
  set_euler(0, 17*16, -25*16, 55*16); set_euler(1, 107*16, 37*16, -60*16);
  sensor[0].reg[0][BNO055_QUA_DATA_W_LSB + 1u] = 0u;
  sensor[1].reg[0][BNO055_QUA_DATA_W_LSB + 1u] = 0u;
  fail_bus = FW_HW_I2C1; fail_reg = BNO055_CALIB_STAT;
  FW_sync_poll();
  CHECK(sent_frames == 2u && sent_frame[41] == 0x84u && sent_frame[42] == 0x03u);
  CHECK(g_eul_internal.pitch_x == 55.0f && g_eul_external.roll_y == 37.0f);

  /* A failed Euler read skips that frame. The next sample sends the new heading. */
  fail_reg = BNO055_EUL_HEADING_LSB;
  FW_sync_poll();
  CHECK(sent_frames == 2u);
  fail_bus = 0;
  set_euler(1, 108*16, 37*16, -60*16);
  FW_sync_poll();
  CHECK(sent_frames == 3u && sent_frame[41] == 0x8Eu && sent_frame[42] == 0x03u);

  mode = FW_MODE_ADVERTISING;
  FW_sync_poll();
  sensor[1].reg[0][BNO055_CALIB_STAT] = 0xAA;
  ticks += FW_BNO_LIVE_MS;
  FW_angle_poll();
  CHECK(g_eul_internal.heading_z == 17.0f && g_eul_external.heading_z == 108.0f);
  CHECK(g_calib_external.raw == 0xAA);

  prepare();
  /* Warm restart: each chip retained a different page, clock and calibration. */
  software_boot_delay_ms = FW_BNO_RESET_DELAY_MS + 40u;
  reset_nack_bus = FW_HW_I2C2;
  internal_reset_unit = 0u; /* Internal BNO needs the CONFIG write after reset. */
  for (unsigned i = 0; i < 2; i++) {
    sensor[i].page = 1;
    sensor[i].reg[0][BNO055_OPR_MODE] = BNO055_OPERATION_MODE_IMU;
    sensor[i].reg[0][BNO055_SYS_TRIGGER] = 0x80;
    sensor[i].reg[0][BNO055_ACC_OFFSET_X_LSB] = 0x75;
    sensor[i].reg[1][BNO055_OPR_MODE] = 0xEE;
  }
  CHECK(FW_angle_init(&module, &code) == 0);
  FW_angle_facts(&internal, &external);
  CHECK(sensor[0].reset_holds == 0 && sensor[1].reset_holds == 0);
  CHECK(internal.reset_write_status == FW_HW_OK && external.reset_write_status == FW_HW_ERR);
  for (unsigned i = 0; i < 2; i++) {
    CHECK(sensor[i].software_resets == 1);
    CHECK(sensor[i].first_boot_read_at - sensor[i].software_reset_at == FW_BNO_RESET_DELAY_MS);
    CHECK(sensor[i].internal_clock_at >= ready_at[i] + FW_BNO_BOOT_SETTLE_MS);
    CHECK(sensor[i].internal_clock_writes == 1 && sensor[i].normal_power_writes == 1);
  }
  CHECK(sensor[0].reg[0][BNO055_ACC_OFFSET_X_LSB] == 0);
  CHECK(sensor[1].reg[0][BNO055_ACC_OFFSET_X_LSB] == 0);
  CHECK(internal.chip_id == 0xA0 && external.chip_id == 0xA0);
  CHECK(internal.opr_mode == 0x1C && external.opr_mode == 0x1C);
  CHECK(internal.pwr_mode == 0 && external.pwr_mode == 0);
  CHECK(internal.unit == 0x80 && external.unit == 0x80);
  CHECK(internal.sys_trigger == (FW_BNO_USE_EXT_CRYSTAL_INT ? 0x80 : 0));
  CHECK(external.sys_trigger == (FW_BNO_USE_EXT_CRYSTAL_EXT ? 0x80 : 0));
  CHECK(FW_angle_window(&angle) == 0 && angle == 0);

  /* Real signed Euler conversion and independent bus selection, including the 360-degree wrap. */
  set_euler(0, 359*16, -12*16, 34*16);
  set_euler(1, 1*16, 25*16, -47*16);
  for (unsigned i = 0; i < 10000; i++) {
    CHECK(FW_angle_window(&angle) == 0 && angle == 20);
    CHECK(g_eul_internal.heading_z == 359.0f && g_eul_external.heading_z == 1.0f);
    CHECK(g_eul_internal.roll_y == -12.0f && g_eul_external.pitch_x == -47.0f);
  }
  CHECK(g_calib_internal.raw == 0xFF && g_calib_external.raw == 0xFF);
  set_euler(0, 180*16, 0, 0);
  set_euler(1, 0, 0, 0);
  CHECK(FW_angle_window(&angle) == 0 && angle == 1800);
  ticks += FW_BNO_LIVE_MS;
  FW_angle_poll();
  CHECK(g_eul_internal.heading_z == 180.0f && g_eul_external.heading_z == 0.0f);
  CHECK(FW_angle_init(&module, &code) == 0);
  CHECK(sensor[0].software_resets == 2 && sensor[1].software_resets == 2);

  /* A BNO that takes longer than the initial boot wait can still be found. */
  prepare(); ready_at[0] = ready_at[1] = FW_BNO_TPOR_MS + 40u;
  CHECK(FW_angle_init(&module, &code) == 0);
  FW_angle_facts(&internal, &external);
  CHECK(sensor[0].software_resets == 1 && sensor[1].software_resets == 1);
  set_euler(0, 42*16, 0, 0); set_euler(1, 43*16, 0, 0);
  CHECK(FW_angle_window(&angle) == 0 && angle == 10);

  /* Hardware observation: the first NDOF read succeeds, then OPR_MODE
     becomes factory CONFIG (0x10), SYS_STATUS=0. A late NDOF write starts
     fusion. Model late boot defaults after the interface is already readable. */
  prepare(); late_defaults_ms = 250u;
  CHECK(FW_angle_init(&module, &code) == 0);
  FW_angle_facts(&internal, &external);
  CHECK(sensor[0].late_defaults == 1 && sensor[1].late_defaults == 1);
  CHECK(sensor[0].active_writes == 2 && sensor[1].active_writes == 2);
  CHECK(sensor[0].software_resets == 1 && sensor[1].software_resets == 1);
  CHECK(sensor[0].internal_clock_writes == 2 && sensor[1].internal_clock_writes == 2);
  CHECK(sensor[0].normal_power_writes == 2 && sensor[1].normal_power_writes == 2);
  CHECK(internal.opr_mode == 0x1C && external.opr_mode == 0x1C);
  CHECK(internal.sys_status == 5 && external.sys_status == 5);
  CHECK(internal.mode_retries == 1 && external.mode_retries == 1);

  /* The mode can also fall back during status polling, after the first
     post-settle OPR_MODE check. That must use the same bounded recovery. */
  prepare();
  late_defaults_ms = FW_BNO_OPR_SWITCH_MS + 200u;
  fusion_delay_ms = FW_BNO_OPR_SWITCH_MS + 300u;
  CHECK(FW_angle_init(&module, &code) == 0);
  CHECK(sensor[0].active_writes == 2 && sensor[1].active_writes == 2);

  /* Repeated loss of the selected mode must fail within a bounded time. */
  prepare(); lose_every_mode_write = 1; fusion_delay_ms = 100u;
  CHECK(FW_angle_init(&module, &code) != 0 && strcmp(code, FW_DIAG_CODE_MODE) == 0);
  CHECK(sensor[0].active_writes == 2 && sensor[0].software_resets == 1);
  FW_angle_facts(&internal, &external);
  CHECK(internal.opr_mode == 0x10 && internal.fail == FW_BNO_FAIL_MODE);
  CHECK(internal.mode_retries == 1);

  /* An I2C failure while selecting NDOF must not be treated as CONFIG
     fallback or retried merely because the preceding saved mode was zero. */
  prepare(); fail_active_bus = FW_HW_I2C1;
  CHECK(FW_angle_init(&module, &code) != 0 && strcmp(code, FW_DIAG_CODE_I2C) == 0);
  FW_angle_facts(&internal, &external);
  CHECK(internal.fail == FW_BNO_FAIL_I2C && internal.mode_retries == 0);

  prepare(); fusion_delay_ms = FW_BNO_OPR_SWITCH_MS + 200u;
  CHECK(FW_angle_init(&module, &code) == 0);
  FW_angle_facts(&internal, &external);
  CHECK(internal.sys_status == 5 && external.sys_status == 5);
  CHECK(g_eul_internal.heading_z == 0.0f && g_eul_external.heading_z == 0.0f);
  prepare(); fusion_never_ready = 1;
  CHECK(FW_angle_init(&module, &code) != 0 && strcmp(code, FW_DIAG_CODE_STATUS) == 0);
  FW_angle_facts(&internal, &external);
  CHECK(internal.sys_status == 3 && internal.sys_err == 0);
  prepare(); fail_bus = FW_HW_I2C1; fail_reg = BNO055_SYS_STATUS;
  CHECK(FW_angle_init(&module, &code) != 0 && strcmp(code, FW_DIAG_CODE_I2C) == 0);
  FW_angle_facts(&internal, &external);
  CHECK(internal.fail == FW_BNO_FAIL_STATUS_IO);
  prepare(); fail_bus = FW_HW_I2C1; fail_reg = BNO055_EUL_HEADING_LSB;
  CHECK(FW_angle_init(&module, &code) != 0 && strcmp(code, FW_DIAG_CODE_I2C) == 0);
  prepare();
  CHECK(FW_angle_init(&module, &code) == 0);

  /* Reading a one-byte operation mode must not leave the rest of an enum uninitialized. */
  bno055_select(FW_HW_I2C1, sensor[0].addr);
  CHECK(bno055_getOperationMode() == BNO055_OPERATION_MODE_NDOF);
  bno055_setOperationModeConfig();
  bno055_enableExternalCrystal();
  CHECK(sensor[0].reg[0][BNO055_SYS_TRIGGER] == 0x80);
  bno055_disableExternalCrystal();
  CHECK(sensor[0].reg[0][BNO055_SYS_TRIGGER] == 0);

  /* A page-select failure cannot read a different page and masquerade as valid Euler. */
  fail_bus = FW_HW_I2C1; fail_reg = BNO055_PAGE_ID; fail_write = 1;
  bno055_select(FW_HW_I2C1, sensor[0].addr);
  calls = 0;
  vector = bno055_getVectorEuler();
  CHECK(!bno055_io_ok() && calls == 1);
  CHECK(vector.x == 0 && vector.y == 0 && vector.z == 0);
  angle = 777;
  CHECK(FW_angle_window(&angle) != 0 && angle == 777);
  fail_reg = BNO055_EUL_HEADING_LSB; fail_write = 0;
  bno055_select(FW_HW_I2C1, sensor[0].addr);
  memset(buffer, 0xCC, sizeof(buffer));
  bno055_readData(BNO055_EUL_HEADING_LSB, buffer, sizeof(buffer));
  CHECK(!bno055_io_ok());
  for (unsigned i = 0; i < sizeof(buffer); i++) CHECK(buffer[i] == 0);

  /* Initialization rejects wrong chips and reset/status/power failures. */
  prepare(); sensor[0].reg[0][BNO055_CHIP_ID] = 0;
  CHECK(FW_angle_init(&module, &code) != 0 && strcmp(code, FW_DIAG_CODE_ID) == 0);
  CHECK(sensor[0].active_writes == 0);
  /* Failure to answer after the software reset blocks initialization. */
  prepare(); software_boot_delay_ms = FW_BNO_RESET_DELAY_MS + FW_BNO_BOOT_TIMEOUT_MS + 100u;
  CHECK(FW_angle_init(&module, &code) != 0 && strcmp(code, FW_DIAG_CODE_RESET) == 0);
  FW_angle_facts(&internal, &external);
  CHECK(internal.fail == FW_BNO_FAIL_RESET && sensor[0].active_writes == 0);
  prepare(); error_bus = FW_HW_I2C1;
  CHECK(FW_angle_init(&module, &code) != 0 && strcmp(code, FW_DIAG_CODE_STATUS) == 0);
  prepare(); bad_status_bus = FW_HW_I2C2;
  CHECK(FW_angle_init(&module, &code) != 0 && strcmp(module, FW_DIAG_MODULE_ANGLE_EXT) == 0);
  prepare(); wrong_power_bus = FW_HW_I2C1;
  CHECK(FW_angle_init(&module, &code) != 0);
  FW_angle_facts(&internal, &external);
  CHECK(internal.fail == FW_BNO_FAIL_PWR);
  /* The internal oscillator must be selected explicitly even with factory defaults. */
  prepare(); fail_bus = FW_HW_I2C2; fail_reg = BNO055_SYS_TRIGGER; fail_write = 1;
  CHECK(FW_angle_init(&module, &code) != 0 && strcmp(code, FW_DIAG_CODE_I2C) == 0);
  FW_angle_facts(&internal, &external);
  CHECK(external.fail == FW_BNO_FAIL_CLOCK && sensor[1].active_writes == 0);
  if (FW_BNO_USE_EXT_CRYSTAL_EXT) {
    prepare(); bad_crystal_bus = FW_HW_I2C2;
    CHECK(FW_angle_init(&module, &code) != 0 && strcmp(code, FW_DIAG_CODE_CLOCK) == 0);
    FW_angle_facts(&internal, &external);
    CHECK(external.fail == FW_BNO_FAIL_CLOCK && external.sys_trigger == 0);
  }
  prepare(); fail_bus = FW_HW_I2C2; fail_reg = BNO055_SYS_TRIGGER;
  CHECK(FW_angle_init(&module, &code) != 0 && strcmp(code, FW_DIAG_CODE_I2C) == 0);
  FW_angle_facts(&internal, &external);
  CHECK(external.fail == FW_BNO_FAIL_CLOCK);
  return 0;
}
