#ifndef FW_EMG_H
#define FW_EMG_H

#include <stdint.h>

/* Вход в MEASUREMENT. Поднимает таймер ЭМГ и кольцо DMA.
   0 — съём идёт. -1 — АЦП или таймер не стартовали. */
int FW_emg_start(void);
/* Выход из MEASUREMENT. Таймер и DMA останавливаются, хвост окна наружу не отдаётся. */
void FW_emg_stop(void);
/* Одна готовая половина кольца, FW_EMG_FRAME_COUNT кодов от старого к новому.
   1 — dst заполнен. 0 — нового окна нет. -1 — окно пропущено. */
int FW_emg_take(uint16_t *dst);

#endif /* FW_EMG_H */
