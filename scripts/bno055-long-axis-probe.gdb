# Stop at entry to FW_angle_poll. Hold both modules with their foot ends down.
# When prompted, turn the pair clockwise viewed from above without tilting it.
set $axis_entry = ((unsigned long)FW_angle_poll) & 0xfffffffe
if (((unsigned long)$pc < $axis_entry) || ((unsigned long)$pc >= $axis_entry + 16))
  echo Stop using tbreak FW_angle_poll then continue before sourcing this file.\n
else
  if (hi2c1.State != HAL_I2C_STATE_READY || hi2c2.State != HAL_I2C_STATE_READY)
    echo Wait for the end of the active I2C transfer.\n
  else
    if ('FW_angle.c'::s_fact_int.addr7 == 0 || 'FW_angle.c'::s_fact_ext.addr7 == 0)
      echo Both BNOs must have completed initialization.\n
    else
      set $axis_int_x = 0.0
      set $axis_int_y = 0.0
      set $axis_int_z = 0.0
      set $axis_ext_x = 0.0
      set $axis_ext_y = 0.0
      set $axis_ext_z = 0.0
      set $axis_sample = 0
      set $axis_good = 0
      echo Get ready. Keep the short ends that point toward the foot down.\n
      shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 2000"
      echo Turn both modules clockwise now, viewed from above. Do not tilt or reverse.\n
      while $axis_sample < 12
        call bno055_select(1, 'FW_angle.c'::s_fact_int.addr7)
        set $axis_int = bno055_getVector(0x14)
        set $axis_int_ok = bno055_io_ok()
        call bno055_select(2, 'FW_angle.c'::s_fact_ext.addr7)
        set $axis_ext = bno055_getVector(0x14)
        if $axis_int_ok && bno055_io_ok()
          set $axis_int_x = $axis_int_x + $axis_int.x
          set $axis_int_y = $axis_int_y + $axis_int.y
          set $axis_int_z = $axis_int_z + $axis_int.z
          set $axis_ext_x = $axis_ext_x + $axis_ext.x
          set $axis_ext_y = $axis_ext_y + $axis_ext.y
          set $axis_ext_z = $axis_ext_z + $axis_ext.z
          set $axis_good = $axis_good + 1
        end
        set $axis_sample = $axis_sample + 1
        shell powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 120"
      end
      printf "Paired gyro samples: %d\n", $axis_good
      printf "Internal gyro sum X Y Z: %.2f %.2f %.2f\n", $axis_int_x, $axis_int_y, $axis_int_z
      printf "External gyro sum X Y Z: %.2f %.2f %.2f\n", $axis_ext_x, $axis_ext_y, $axis_ext_z
    end
  end
end
