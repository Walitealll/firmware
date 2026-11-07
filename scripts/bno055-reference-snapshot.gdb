# Memory-only snapshot of the standalone image after at least 30 seconds of run.
echo \nStandalone BNO055 reference image\n
echo Init result (0=success, -2=not run):\n
p g_bno_reference_init_result
if g_bno_reference_init_result != 0
  p g_bno_reference_module
  p g_bno_reference_code
  p 'FW_angle.c'::s_fact_int
  p 'FW_angle.c'::s_fact_ext
else
  echo BLE nRESET level (0=held in reset, 1=released):\n
  p (((GPIO_TypeDef *)0x40010c00)->ODR >> 14) & 1
  echo \nExternal:\n
  p g_eul_external
  p g_calib_external
  p g_bno_reference_external
  echo \nInternal:\n
  p g_eul_internal
  p g_calib_internal
  p g_bno_reference_internal
end
