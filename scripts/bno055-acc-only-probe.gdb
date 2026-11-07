# Temporary diagnostic on internal BNO055. The MCU must be suspended in main,
# outside interrupts and active I2C transfers. BLE/other BNO are not changed.
# The saved NDOF mode is restored at the end. Do not interrupt the short script.
# Host waits are deliberate: the BNO continues running while the MCU is halted.

define fw-bno-internal-reg
  set $fw_bno_reg_saved_raw = g_calib_internal.raw
  set $fw_bno_reg_value = 0
  set $fw_bno_reg_io = fw_hw_i2c_read_reg(1, 'FW_angle.c'::s_fact_int.addr7, $arg0, &g_calib_internal.raw)
  if $fw_bno_reg_io == 0
    set $fw_bno_reg_value = (unsigned int)g_calib_internal.raw
    printf "reg 0x%02x = 0x%02x, IO=OK\n", $arg0, $fw_bno_reg_value
  else
    printf "reg 0x%02x: IO ERROR %d\n", $arg0, $fw_bno_reg_io
  end
  set g_calib_internal.raw = $fw_bno_reg_saved_raw
end

if ((hi2c1.State != HAL_I2C_STATE_READY) || (hi2c2.State != HAL_I2C_STATE_READY))
  echo I2C transfer is active. Stop at main before running this file.\n
else
  set $fw_bno_probe_entry_pc = ((unsigned long)FW_angle_poll) & 0xfffffffe
  if (((unsigned long)$pc < $fw_bno_probe_entry_pc) || ((unsigned long)$pc >= $fw_bno_probe_entry_pc + 16))
    echo Stop at entry to FW_angle_poll using tbreak FW_angle_poll then continue.\n
  else
    call bno055_select(1, 'FW_angle.c'::s_fact_int.addr7)
    call bno055_setPage(0)
    echo Internal mode before test:\n
    fw-bno-internal-reg 0x3d
    set $fw_bno_probe_saved_mode = $fw_bno_reg_value
    if ($fw_bno_reg_io == 0) && (($fw_bno_probe_saved_mode & 0x0f) == 0x0c)
      echo CHIP_ID, ACC_ID, MAG_ID, GYR_ID, SW_REV_LSB, SW_REV_MSB:\n
      fw-bno-internal-reg 0x00
      fw-bno-internal-reg 0x01
      fw-bno-internal-reg 0x02
      fw-bno-internal-reg 0x03
      fw-bno-internal-reg 0x04
      fw-bno-internal-reg 0x05

      set $fw_bno_probe_config = $fw_bno_probe_saved_mode & 0xf0
      echo Enter CONFIG (each write must report FW_HW_OK):\n
      set $fw_bno_probe_io = fw_hw_i2c_write_reg(1, 'FW_angle.c'::s_fact_int.addr7, 0x3d, $fw_bno_probe_config)
      p $fw_bno_probe_io
      shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 30"
      if $fw_bno_probe_io == 0
        echo Enter ACCONLY:\n
        set $fw_bno_probe_io = fw_hw_i2c_write_reg(1, 'FW_angle.c'::s_fact_int.addr7, 0x3d, $fw_bno_probe_config | 1)
        p $fw_bno_probe_io
        shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 650"
        if $fw_bno_probe_io == 0
          echo Actual mode, SYS_STATUS, SYS_ERR, UNIT_SEL in ACCONLY:\n
          fw-bno-internal-reg 0x3d
          fw-bno-internal-reg 0x39
          fw-bno-internal-reg 0x3a
          fw-bno-internal-reg 0x3b
          call bno055_select(1, 'FW_angle.c'::s_fact_int.addr7)
          echo Internal acceleration m/s^2, three readings:\n
          p bno055_getVector(0x08)
          shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 200"
          p bno055_getVector(0x08)
          shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 200"
          p bno055_getVector(0x08)
          echo Library IO_OK (expected 1):\n
          p bno055_io_ok()
        end
      end

      echo Restore original mode via CONFIG:\n
      p fw_hw_i2c_write_reg(1, 'FW_angle.c'::s_fact_int.addr7, 0x3d, $fw_bno_probe_config)
      shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 30"
      p fw_hw_i2c_write_reg(1, 'FW_angle.c'::s_fact_int.addr7, 0x3d, $fw_bno_probe_saved_mode)
      shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 650"
      echo Mode, SYS_STATUS and SYS_ERR after restoration:\n
      fw-bno-internal-reg 0x3d
      fw-bno-internal-reg 0x39
      fw-bno-internal-reg 0x3a
      echo Probe finished. Check restored mode low nibble is 0xC.\n
    else
      echo Internal was not in NDOF or mode read failed. No mode writes performed.\n
    end
  end
end
