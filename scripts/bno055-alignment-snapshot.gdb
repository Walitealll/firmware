# Read the existing library directly. No sensor reset or mode switch.
# Suspend at main/FW_angle_poll, outside interrupts and active I2C transfers.
# Record with both modules in the same physical orientation, marking that pose.
if ((hi2c1.State != HAL_I2C_STATE_READY) || (hi2c2.State != HAL_I2C_STATE_READY))
  echo I2C transfer is active. Stop at main/FW_angle_poll before running this file.\n
else
  echo \nInternal: I2C1, detected address\n
  call bno055_select(1, 'FW_angle.c'::s_fact_int.addr7)
  echo Euler degrees (x=heading, y=roll, z=pitch):\n
  p bno055_getVector(0x1a)
  echo Quaternion (w,x,y,z):\n
  p bno055_getVector(0x20)
  echo Magnetic field uT (x,y,z):\n
  p bno055_getVector(0x0e)
  echo Acceleration m/s^2 (x,y,z):\n
  p bno055_getVector(0x08)
  echo Calibration (sys,gyro,mag,accel):\n
  p bno055_getCalibrationState()
  echo IO_OK (1=all library operations succeeded):\n
  p bno055_io_ok()

  echo \nExternal: I2C2, 0x29\n
  call bno055_select(2, 0x29)
  echo Euler degrees (x=heading, y=roll, z=pitch):\n
  p bno055_getVector(0x1a)
  echo Quaternion (w,x,y,z):\n
  p bno055_getVector(0x20)
  echo Magnetic field uT (x,y,z):\n
  p bno055_getVector(0x0e)
  echo Acceleration m/s^2 (x,y,z):\n
  p bno055_getVector(0x08)
  echo Calibration (sys,gyro,mag,accel):\n
  p bno055_getCalibrationState()
  echo IO_OK (1=all library operations succeeded):\n
  p bno055_io_ok()
end
