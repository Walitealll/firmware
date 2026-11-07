# Read-only sensor snapshot for the current firmware. Run while suspended at
# main or at the entry of FW_angle_poll, outside interrupts and I2C transfers.
# This file does not reset a BNO055, change its operating mode or resume main.
# Selecting page 0 is part of the existing library's normal read path.

if ((hi2c1.State != HAL_I2C_STATE_READY) || (hi2c2.State != HAL_I2C_STATE_READY))
  echo I2C transfer is active. Stop at main/FW_angle_poll before running this file.\n
else
  echo \nExternal: I2C2, 0x29\n
  call bno055_select(2, 0x29)
  echo Euler degrees (x=heading, y=roll, z=pitch):\n
  p bno055_getVector(0x1a)
  echo Gyroscope deg/s (x, y, z):\n
  p bno055_getVector(0x14)
  echo Accelerometer m/s^2 (x, y, z):\n
  p bno055_getVector(0x08)
  echo Calibration (sys, gyro, mag, accel; each 0..3):\n
  p bno055_getCalibrationState()
  echo IO_OK (1 means all library reads above succeeded):\n
  p bno055_io_ok()

  echo \nInternal: I2C1, detected address\n
  call bno055_select(1, 'FW_angle.c'::s_fact_int.addr7)
  echo Euler degrees (x=heading, y=roll, z=pitch):\n
  p bno055_getVector(0x1a)
  echo Gyroscope deg/s (x, y, z):\n
  p bno055_getVector(0x14)
  echo Accelerometer m/s^2 (x, y, z):\n
  p bno055_getVector(0x08)
  echo Calibration (sys, gyro, mag, accel; each 0..3):\n
  p bno055_getCalibrationState()
  echo IO_OK (1 means all library reads above succeeded):\n
  p bno055_io_ok()
end
