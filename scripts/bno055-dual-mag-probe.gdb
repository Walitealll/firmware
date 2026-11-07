# BNO055_Dual only. Stop at the entry to bno_debug_sample, then run
# bno-dual-mag-probe (external AMG) or bno-dual-internal-amg (internal AMG).

define bno-dual-reg
  set $bno_io = fw_hw_i2c_read($arg0, $arg1, $arg2, $bno_scratch, 1)
  if $bno_io == 0
    set $bno_reg = (unsigned int)*$bno_scratch
    printf "reg 0x%02x = 0x%02x\n", $arg2, $bno_reg
  else
    printf "I2C error %d reading reg 0x%02x\n", $bno_io, $arg2
  end
end

define bno-dual-mag-dump
  set $bno_io = fw_hw_i2c_read($arg0, $arg1, 0x0e, $bno_scratch, 6)
  if $bno_io == 0
    printf "MAG_DATA bytes (X low/high, Y low/high, Z low/high):\n"
    x/6bx $bno_scratch
    printf "Magnetic field (uT): %g %g %g\n", (double)*(short *)$bno_scratch / 16.0, (double)*(short *)($bno_scratch + 2) / 16.0, (double)*(short *)($bno_scratch + 4) / 16.0
  else
    printf "I2C error %d reading MAG_DATA\n", $bno_io
  end
end

define bno-dual-switch-mode
  set $bno_target_mode = (unsigned int)$arg2
  set $bno_config = $bno_target_mode & 0xf0
  set $bno_switch_ok = 0
  set $bno_write = fw_hw_i2c_write_reg($arg0, $arg1, 0x3d, $bno_config)
  shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 30"
  if $bno_write == 0
    bno-dual-reg $arg0 $arg1 0x3d
    if $bno_io == 0 && ($bno_reg & 0x0f) == 0
      set $bno_write = fw_hw_i2c_write_reg($arg0, $arg1, 0x3d, $bno_target_mode)
      shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 650"
      if $bno_write == 0
        bno-dual-reg $arg0 $arg1 0x3d
        set $bno_switch_ok = $bno_io == 0 && ($bno_reg & 0x0f) == ($bno_target_mode & 0x0f)
      else
        printf "Mode write failed: %d\n", $bno_write
      end
    end
  else
    printf "CONFIG write failed: %d\n", $bno_write
  end
end

define bno-dual-amg
  set $bno_mode = (unsigned int)$arg0
  echo External in AMG:\n
  bno-dual-switch-mode 2 $bno_ext (($bno_mode&0xf0)|7)
  if $bno_switch_ok
    bno-dual-mag-dump 2 $bno_ext
  end
  echo Restoring external NDOF:\n
  bno-dual-switch-mode 2 $bno_ext $bno_mode
  if $bno_switch_ok
    bno-dual-reg 2 $bno_ext 0x39
    bno-dual-reg 2 $bno_ext 0x3a
    bno-dual-mag-dump 2 $bno_ext
  else
    echo External NDOF could not be confirmed. Restart Debug to reinitialize the BNOs.\n
  end
end

define bno-dual-check-stop
  set $bno_entry = (unsigned long)bno_debug_sample & 0xfffffffe
  set $bno_can_probe = $pc >= $bno_entry && $pc < $bno_entry + 16 && hi2c1.State == HAL_I2C_STATE_READY && hi2c2.State == HAL_I2C_STATE_READY
  if !$bno_can_probe
    echo Stop at entry to bno_debug_sample with tbreak bno_debug_sample, then continue.\n
  end
end

define bno-dual-save-scratch
  # Borrow eight bytes of debugger output while the MCU is stopped.
  set $bno_scratch = (unsigned char *)&g_bno_pair
  set $bno_save0 = *(unsigned int *)$bno_scratch
  set $bno_save1 = *(unsigned int *)($bno_scratch + 4)
end

define bno-dual-restore-scratch
  set *(unsigned int *)$bno_scratch = $bno_save0
  set *(unsigned int *)($bno_scratch + 4) = $bno_save1
end

define bno-dual-mag-probe
  bno-dual-check-stop
  if $bno_can_probe
    set $bno_int = (unsigned int)g_bno_internal.addr7
    set $bno_ext = (unsigned int)g_bno_external.addr7
    bno-dual-save-scratch
    echo Internal in NDOF:\n
    bno-dual-reg 1 $bno_int 0x07
    if $bno_io == 0 && $bno_reg == 0
      bno-dual-reg 1 $bno_int 0x3d
      bno-dual-mag-dump 1 $bno_int
    end
    echo External in NDOF:\n
    bno-dual-reg 2 $bno_ext 0x07
    if $bno_io == 0 && $bno_reg == 0
      bno-dual-reg 2 $bno_ext 0x3d
      if $bno_io == 0 && ($bno_reg & 0x0f) == 0x0c
        set $bno_initial_mode = $bno_reg
        bno-dual-mag-dump 2 $bno_ext
        if $bno_io == 0
          bno-dual-amg $bno_initial_mode
        end
      end
    end
    bno-dual-restore-scratch
    echo Probe finished.\n
  end
end

define bno-dual-internal-amg
  bno-dual-check-stop
  if $bno_can_probe
    if g_bno_internal.init_result != BNO_INIT_OK
      echo Internal BNO failed initialization. Print g_bno_internal first.\n
    else
      set $bno_int = (unsigned int)g_bno_internal.addr7
      bno-dual-save-scratch
      bno-dual-reg 1 $bno_int 0x07
      if $bno_io == 0 && $bno_reg == 0
        bno-dual-reg 1 $bno_int 0x3d
        if $bno_io == 0 && ($bno_reg & 0x0f) == 0x0c
          set $bno_internal_mode = $bno_reg
          echo Internal in NDOF:\n
          bno-dual-reg 1 $bno_int 0x35
          bno-dual-mag-dump 1 $bno_int
          echo Internal in AMG:\n
          bno-dual-switch-mode 1 $bno_int (($bno_internal_mode&0xf0)|7)
          if $bno_switch_ok
            bno-dual-reg 1 $bno_int 0x02
            bno-dual-reg 1 $bno_int 0x39
            bno-dual-reg 1 $bno_int 0x3a
            bno-dual-mag-dump 1 $bno_int
            bno-dual-reg 1 $bno_int 0x3b
            if $bno_io == 0
              set $bno_acc_scale = ($bno_reg & 1) ? (1000.0/9.80665) : 100.0
              set $bno_io = fw_hw_i2c_read(1, $bno_int, 0x08, $bno_scratch, 6)
              if $bno_io == 0
                printf "ACC_DATA bytes (X low/high, Y low/high, Z low/high):\n"
                x/6bx $bno_scratch
                printf "Acceleration (m/s^2): %g %g %g\n", (double)*(short *)$bno_scratch / $bno_acc_scale, (double)*(short *)($bno_scratch + 2) / $bno_acc_scale, (double)*(short *)($bno_scratch + 4) / $bno_acc_scale
              else
                printf "I2C error %d reading ACC_DATA\n", $bno_io
              end
            end
          end
          echo Restoring internal NDOF:\n
          bno-dual-switch-mode 1 $bno_int $bno_internal_mode
          if $bno_switch_ok
            bno-dual-reg 1 $bno_int 0x39
            bno-dual-reg 1 $bno_int 0x3a
          else
            echo Internal NDOF could not be confirmed. Restart Debug to reinitialize.\n
          end
        else
          echo Internal was not in NDOF or mode read failed. No mode writes performed.\n
        end
      end
      bno-dual-restore-scratch
      echo Internal probe finished.\n
    end
  end
end

define bno-dual-external-mode
  if $argc != 1
    echo Usage: bno-dual-external-mode 0x0b (NDOF_FMC_OFF) or 0x0c (NDOF).\n
  else
    set $bno_request = (unsigned int)$arg0
    if $bno_request != 0x0b && $bno_request != 0x0c
      echo Only 0x0b and 0x0c are supported by this command.\n
    else
      bno-dual-check-stop
      if $bno_can_probe
        set $bno_ext = (unsigned int)g_bno_external.addr7
        bno-dual-save-scratch
        bno-dual-reg 2 $bno_ext 0x07
        if $bno_io == 0 && $bno_reg == 0
          bno-dual-reg 2 $bno_ext 0x3d
          if $bno_io == 0 && (($bno_reg & 0x0f) == 0x0b || ($bno_reg & 0x0f) == 0x0c)
            set $bno_original_mode = $bno_reg
            bno-dual-switch-mode 2 $bno_ext (($bno_original_mode&0xf0)|$bno_request)
            if $bno_switch_ok
              bno-dual-reg 2 $bno_ext 0x39
              bno-dual-reg 2 $bno_ext 0x3a
              bno-dual-reg 2 $bno_ext 0x35
              bno-dual-mag-dump 2 $bno_ext
              echo Resume to observe live samples in this mode.\n
            else
              echo Mode change failed. Attempting to restore original mode.\n
              bno-dual-switch-mode 2 $bno_ext $bno_original_mode
              if !$bno_switch_ok
                echo Original mode could not be confirmed. Restart Debug to reinitialize.\n
              end
            end
          else
            echo External is not in NDOF or NDOF_FMC_OFF. No mode writes performed.\n
          end
        end
        bno-dual-restore-scratch
      end
    end
  end
end

define bno-dual-external-fresh-ndof
  bno-dual-check-stop
  if $bno_can_probe
    if g_bno_external.init_result != BNO_INIT_OK
      echo External BNO did not initialize in this session. No reset performed.\n
    else
      set $bno_ext = (unsigned int)g_bno_external.addr7
      bno-dual-save-scratch
      # STM32F103 GPIOB, PB2 = GPIO_OUT_EXT_BNO_RST_Pin on this board.
      echo Pulsing external BNO nRESET on PB2.\n
      call HAL_GPIO_WritePin((GPIO_TypeDef *)0x40010c00, 0x0004, GPIO_PIN_RESET)
      shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 20"
      call HAL_GPIO_WritePin((GPIO_TypeDef *)0x40010c00, 0x0004, GPIO_PIN_SET)
      shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 1000"
      bno-dual-reg 2 $bno_ext 0x00
      if $bno_io == 0 && $bno_reg == 0xa0
        bno-dual-reg 2 $bno_ext 0x3d
        if $bno_io == 0 && ($bno_reg & 0x0f) == 0
          set $bno_after_reset = $bno_reg
          bno-dual-reg 2 $bno_ext 0x3f
          if $bno_io == 0 && ($bno_reg & 0x80) == 0
            echo Enter NDOF with reset defaults.\n
            p fw_hw_i2c_write_reg(2, $bno_ext, 0x3d, ($bno_after_reset & 0xf0) | 0x0c)
            shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 1000"
            bno-dual-reg 2 $bno_ext 0x3d
            bno-dual-reg 2 $bno_ext 0x39
            bno-dual-reg 2 $bno_ext 0x3a
            bno-dual-reg 2 $bno_ext 0x3b
            bno-dual-reg 2 $bno_ext 0x35
            bno-dual-mag-dump 2 $bno_ext
            echo Resume and observe the external magnetic vector and calibration.\n
          else
            echo Default oscillator was not confirmed. No mode write performed.\n
          end
        else
          echo Reset was not confirmed in CONFIG mode. No mode write performed.\n
        end
      else
        echo External BNO did not respond with CHIP_ID 0xA0 after reset.\n
      end
      bno-dual-restore-scratch
    end
  end
end
