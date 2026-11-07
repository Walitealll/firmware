#ifndef BNO_DEBUG_H
#define BNO_DEBUG_H

#include "bno055.h"
#include <stdint.h>

typedef enum {
  BNO_INIT_NOT_RUN, BNO_INIT_OK, BNO_INIT_ADDRESS, BNO_INIT_CHIP_ID,
  BNO_INIT_IO, BNO_INIT_CONFIG, BNO_INIT_CLOCK, BNO_INIT_POWER, BNO_INIT_UNITS,
  BNO_INIT_AXIS_MAP, BNO_INIT_MODE, BNO_INIT_FUSION
} bno_init_result_t;

typedef struct { float heading, roll, pitch; } bno_euler_deg_t;

typedef struct {
  bno_init_result_t init_result;
  uint32_t sampled_ms;
  uint32_t io_errors;
  bno_euler_deg_t euler_deg;
  bno055_vector_t quaternion;
  bno055_vector_t accel_ms2;
  bno055_vector_t gyro_dps;
  bno055_vector_t mag_ut;
  float accel_norm_ms2;
  bno055_calibration_state_t calib;
  uint8_t calib_raw;
  uint8_t addr7, chip_id, address_mask;
  uint8_t opr_mode, pwr_mode, unit_sel, sys_status, sys_err, self_test;
  uint8_t axis_map, axis_sign;
  uint8_t sys_trigger, sys_clk_status;
  uint8_t crystal_requested, crystal_active, clock_fallback;
  uint8_t sample_ok, fusion_running, north_reference, fully_calibrated;
} bno_debug_sensor_t;

typedef struct {
  uint32_t sampled_ms;
  float heading_delta_deg;
  float orientation_delta_deg;
  float segment_angle_deg;
  uint8_t north_ready;
  uint8_t both_fully_calibrated;
  float reported_heading_delta_deg;
} bno_debug_pair_t;

extern volatile bno_debug_sensor_t g_bno_internal, g_bno_external;
extern volatile bno_debug_pair_t g_bno_pair;

void bno_debug_init(void);
void bno_debug_sample(void);

#endif
