#include "FW_emg.h"

#include "fw_hw.h"

/* Частота и число отсчётов окна заданы FW_EMG_HZ. Здесь только граница между циклом и DMA. */

int FW_emg_start(void)
{
  return fw_hw_emg_start() == FW_HW_OK ? 0 : -1;
}

void FW_emg_stop(void)
{
  fw_hw_emg_stop();
}

int FW_emg_take(uint16_t *dst)
{
  return fw_hw_emg_take(dst);
}
