# Read-only BNO settings/data check. UNIT_SEL is copied into its existing
# diagnostic field. No sensor reset or operation-mode change is performed.
# Suspend at main/FW_angle_poll, outside interrupts and I2C transfers.
if ((hi2c1.State != HAL_I2C_STATE_READY) || (hi2c2.State != HAL_I2C_STATE_READY))
  echo I2C transfer is active. Stop at main/FW_angle_poll before running this file.\n
else
  echo Library acceleration divisor (expected 100):\n
  p accelScale
  echo \nInternal: current UNIT_SEL, then acceleration and calibration\n
  call bno055_select(1, 'FW_angle.c'::s_fact_int.addr7)
  call bno055_setPage(0)
  p fw_hw_i2c_read_reg(1, 'FW_angle.c'::s_fact_int.addr7, 0x3b, &'FW_angle.c'::s_fact_int.unit)
  p /x 'FW_angle.c'::s_fact_int.unit
  p bno055_getVector(0x08)
  p bno055_getCalibrationState()
  p bno055_io_ok()
  echo \nExternal: current UNIT_SEL, then acceleration and calibration\n
  call bno055_select(2, 0x29)
  call bno055_setPage(0)
  p fw_hw_i2c_read_reg(2, 0x29, 0x3b, &'FW_angle.c'::s_fact_ext.unit)
  p /x 'FW_angle.c'::s_fact_ext.unit
  p bno055_getVector(0x08)
  p bno055_getCalibrationState()
  p bno055_io_ok()
end
