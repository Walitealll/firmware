# Commands for the already loaded BNO055 reference image.
# PB14 is GPIO_OUT_BLE_RST in Core/Inc/main.h. GPIOB base is 0x40010c00.
# These commands only change BLE nRESET, leaving both BNO reset pins alone.
# Run commands while suspended. Resume main manually to collect new samples.

define bno-ble-release
  if g_bno_reference_init_result == 0
    call HAL_GPIO_WritePin((GPIO_TypeDef *)0x40010c00, 0x4000, GPIO_PIN_SET)
    echo BLE nRESET released (PB14=1). Resume, wait 60 seconds, then suspend and take a reference snapshot.\n
  else
    echo Reference image has not initialized successfully; no GPIO change made.\n
  end
end
document bno-ble-release
Release only BLE nRESET on the BNO055 reference image. Does not configure UART,
send AT commands, start advertising explicitly or reset either BNO055.
end

define bno-ble-reset
  if g_bno_reference_init_result == 0
    call HAL_GPIO_WritePin((GPIO_TypeDef *)0x40010c00, 0x4000, GPIO_PIN_RESET)
    echo BLE held in reset (PB14=0). Resume, wait 60 seconds, then suspend and take a reference snapshot.\n
  else
    echo Reference image has not initialized successfully; no GPIO change made.\n
  end
end
document bno-ble-reset
Hold only BLE nRESET low on the BNO055 reference image; neither BNO055 is reset.
end

echo Loaded bno-ble-release and bno-ble-reset. No GPIO changed yet.\n
