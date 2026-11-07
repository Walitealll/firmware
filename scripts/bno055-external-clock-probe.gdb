# External BNO: I2C2 / 0x29. No reset. Restores original mode and clock.
# Stop at entry to FW_angle_poll on the existing failed-init firmware.

define fw-bno-ext-clock-read
  set $fw_ext_clock_raw = g_calib_internal.raw
  set $fw_ext_clock_io = fw_hw_i2c_read_reg(2, 0x29, $arg0, &g_calib_internal.raw)
  if $fw_ext_clock_io == 0
    set $fw_ext_clock_value = (unsigned int)g_calib_internal.raw
    printf "reg 0x%02x = 0x%02x, IO=OK\n", $arg0, $fw_ext_clock_value
  else
    printf "reg 0x%02x: IO ERROR %d\n", $arg0, $fw_ext_clock_io
  end
  set g_calib_internal.raw = $fw_ext_clock_raw
end

set $fw_ext_clock_entry = ((unsigned long)FW_angle_poll) & 0xfffffffe
if (((unsigned long)$pc < $fw_ext_clock_entry) || ((unsigned long)$pc >= $fw_ext_clock_entry + 16))
  echo Stop using tbreak FW_angle_poll then continue before sourcing this file.\n
else
  if hi2c2.State != HAL_I2C_STATE_READY
    echo I2C2 transfer is active; no changes performed.\n
  else
    echo External BNO: page, original mode, original clock:\n
    fw-bno-ext-clock-read 0x07
    if ($fw_ext_clock_io == 0) && ($fw_ext_clock_value == 0)
      fw-bno-ext-clock-read 0x3d
      set $fw_ext_clock_mode_io = $fw_ext_clock_io
      set $fw_ext_clock_saved_mode = $fw_ext_clock_value
      fw-bno-ext-clock-read 0x3f
      if ($fw_ext_clock_io == 0) && ($fw_ext_clock_mode_io == 0) && (($fw_ext_clock_value & 0x7f) == 0) && ((($fw_ext_clock_saved_mode & 0x0f) == 0) || (($fw_ext_clock_saved_mode & 0x0f) == 0x0c))
        set $fw_ext_clock_saved_trigger = $fw_ext_clock_value
        set $fw_ext_clock_config_mode = $fw_ext_clock_saved_mode & 0xf0
        echo Enter CONFIG without reset:\n
        set $fw_ext_clock_config_io = fw_hw_i2c_write_reg(2, 0x29, 0x3d, $fw_ext_clock_config_mode)
        p $fw_ext_clock_config_io
        shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 30"
        if $fw_ext_clock_config_io == 0
          echo Request external clock:\n
          p fw_hw_i2c_write_reg(2, 0x29, 0x3f, 0x80)
          shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 1000"
          echo At +1 second: SYS_CLK_STATUS then SYS_TRIGGER:\n
          fw-bno-ext-clock-read 0x38
          fw-bno-ext-clock-read 0x3f
          shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 1000"
          echo At +2 seconds: SYS_CLK_STATUS then SYS_TRIGGER:\n
          fw-bno-ext-clock-read 0x38
          fw-bno-ext-clock-read 0x3f
          echo Restore original clock:\n
          p fw_hw_i2c_write_reg(2, 0x29, 0x3f, $fw_ext_clock_saved_trigger)
          shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 700"
        end
        echo Restore original operation mode:\n
        p fw_hw_i2c_write_reg(2, 0x29, 0x3d, $fw_ext_clock_saved_mode)
        shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 650"
        fw-bno-ext-clock-read 0x3d
        fw-bno-ext-clock-read 0x3f
        echo External clock probe finished. Original mode and clock requested; no reset performed.\n
      else
        echo Unexpected mode/trigger or read failure; no changes performed.\n
      end
    else
      echo Page 0 was not confirmed; no changes performed.\n
    end
  end
end
