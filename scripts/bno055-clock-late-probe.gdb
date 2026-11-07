# Internal BNO only. Run at entry to FW_angle_poll on the existing ERR CLOCK image.
# Try external clock after boot has finished, then restore the initial clock.
# The chip must already be in CONFIG; no reset or change to NDOF is performed.

define fw-bno-late-clock-read
  set $fw_late_saved_raw = g_calib_internal.raw
  set $fw_late_io = fw_hw_i2c_read_reg(1, 'FW_angle.c'::s_fact_int.addr7, $arg0, &g_calib_internal.raw)
  if $fw_late_io == 0
    set $fw_late_value = (unsigned int)g_calib_internal.raw
    printf "reg 0x%02x = 0x%02x, IO=OK\n", $arg0, $fw_late_value
  else
    printf "reg 0x%02x: IO ERROR %d\n", $arg0, $fw_late_io
  end
  set g_calib_internal.raw = $fw_late_saved_raw
end

set $fw_late_entry_pc = ((unsigned long)FW_angle_poll) & 0xfffffffe
if (((unsigned long)$pc < $fw_late_entry_pc) || ((unsigned long)$pc >= $fw_late_entry_pc + 16))
  echo Stop using tbreak FW_angle_poll then continue before sourcing this file.\n
else
  if hi2c1.State != HAL_I2C_STATE_READY
    echo I2C1 transfer is active; no changes performed.\n
  else
    fw-bno-late-clock-read 0x07
    if ($fw_late_io == 0) && ($fw_late_value == 0)
      fw-bno-late-clock-read 0x3d
      if ($fw_late_io == 0) && (($fw_late_value & 0x0f) == 0)
        fw-bno-late-clock-read 0x3f
        if ($fw_late_io == 0) && (($fw_late_value & 0x7f) == 0)
          set $fw_late_saved_trigger = $fw_late_value
          echo Late external clock request, SYS_TRIGGER=0x80:\n
          p fw_hw_i2c_write_reg(1, 'FW_angle.c'::s_fact_int.addr7, 0x3f, 0x80)
          shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 1000"
          echo At +1 second: SYS_CLK_STATUS then SYS_TRIGGER:\n
          fw-bno-late-clock-read 0x38
          fw-bno-late-clock-read 0x3f
          shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 1000"
          echo At +2 seconds: SYS_CLK_STATUS then SYS_TRIGGER:\n
          fw-bno-late-clock-read 0x38
          fw-bno-late-clock-read 0x3f
          echo Restore original clock selection:\n
          p fw_hw_i2c_write_reg(1, 'FW_angle.c'::s_fact_int.addr7, 0x3f, $fw_late_saved_trigger)
          shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 700"
          fw-bno-late-clock-read 0x38
          fw-bno-late-clock-read 0x3f
          echo Late clock probe finished. No reset, power, units or NDOF changes were made.\n
        else
          echo Unexpected SYS_TRIGGER or read failure; no changes performed.\n
        end
      else
        echo CONFIG was not confirmed; no changes performed.\n
      end
    else
      echo Page 0 was not confirmed; no changes performed.\n
    end
  end
end
