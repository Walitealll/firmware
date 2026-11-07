# Read-only probe for the image that failed with ERR CLOCK. No BNO writes.
# Stop at tbreak FW_angle_poll / continue before sourcing this file.

define fw-bno-clock-read
  set $fw_clock_saved_raw = g_calib_internal.raw
  set $fw_clock_io = fw_hw_i2c_read_reg(1, 'FW_angle.c'::s_fact_int.addr7, $arg0, &g_calib_internal.raw)
  if $fw_clock_io == 0
    set $fw_clock_value = (unsigned int)g_calib_internal.raw
    printf "reg 0x%02x = 0x%02x, IO=OK\n", $arg0, $fw_clock_value
  else
    printf "reg 0x%02x: IO ERROR %d\n", $arg0, $fw_clock_io
  end
  set g_calib_internal.raw = $fw_clock_saved_raw
end

if hi2c1.State != HAL_I2C_STATE_READY
  echo I2C1 transfer is active. Stop at entry to FW_angle_poll.\n
else
  set $fw_clock_entry_pc = ((unsigned long)FW_angle_poll) & 0xfffffffe
  if (((unsigned long)$pc < $fw_clock_entry_pc) || ((unsigned long)$pc >= $fw_clock_entry_pc + 16))
    echo Stop using tbreak FW_angle_poll then continue before sourcing this file.\n
  else
    echo Internal BNO: saved initialization facts:\n
    p 'FW_angle.c'::s_fact_int
    echo Current PAGE_ID (0x07), expected 0:\n
    fw-bno-clock-read 0x07
    if ($fw_clock_io == 0) && ($fw_clock_value == 0)
      echo CHIP_ID (0x00), SYS_CLK_STATUS (0x38), SYS_TRIGGER (0x3f):\n
      fw-bno-clock-read 0x00
      fw-bno-clock-read 0x38
      fw-bno-clock-read 0x3f
      echo OPR_MODE (0x3d), SYS_STATUS (0x39), SYS_ERR (0x3a):\n
      fw-bno-clock-read 0x3d
      fw-bno-clock-read 0x39
      fw-bno-clock-read 0x3a
    else
      echo Page 0 was not confirmed; skipped page-specific registers. No page changed.\n
    end
    echo Read-only clock probe finished. No reset or clock selection was performed.\n
  end
end
