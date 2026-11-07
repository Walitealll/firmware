# Memory-only snapshot of the full firmware. No inferior function calls.
# Sensor facts are saved initialization results; calibration/Euler are the
# latest successful readings from the running main loop, not fresh I2C reads.
echo \nFull firmware: latest saved Euler and calibration\n
echo External:\n
p g_eul_external
p g_calib_external
echo Internal:\n
p g_eul_internal
p g_calib_internal
echo Initialization facts (not live registers):\n
p 'FW_angle.c'::s_fact_int
p 'FW_angle.c'::s_fact_ext
echo Last diagnostic line:\n
p 'FW_diagnostics.c'::g_line
