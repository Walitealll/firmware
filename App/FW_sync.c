#include "FW_sync.h"

#include "FW_angle.h"
#include "FW_ble.h"
#include "FW_emg.h"
#include "FW_protocol.h"
#include "FW_states.h"
#include "fw_config.h"
#include "fw_hw.h"

/* Сумма тактов 5 мс с момента соединения. На 10 с обнуляется и кадр можно класть. */
static uint32_t s_acc;
/* 1, пока телефон подключён и кадр питания вообще имеет смысл. */
static uint8_t s_run;

void FW_sync_start(void)
{
  fw_hw_timer_5ms_start();
}

void FW_sync_power_arm(void)
{
  /* Такты, пока ждали телефон, в эти 10 с не входят. Иначе кадр уйдёт в момент соединения. */
  (void)fw_hw_timer_5ms_take();
  s_acc = 0u;
  s_run = 1u;
}

void FW_sync_power_stop(void)
{
  s_acc = 0u;
  s_run = 0u;
}

int FW_sync_power_due(void)
{
  /* Сколько тактов 5 мс пришло с прошлого вызова. */
  uint32_t n = fw_hw_timer_5ms_take();

  /* Такты забираем всегда, иначе счётчик в прерывании копится, пока ждём телефон. */
  if (s_run == 0u) {
    return 0;
  }
  s_acc += n;
  if (s_acc < FW_POWER_PERIOD_TICKS) {
    return 0;
  }
  /* Пропущенные десятки секунд, если цикл стоял на паузе, одним кадром не догоняем. */
  s_acc = 0u;
  return 1;
}

/* 1, пока в этом заходе в MEASUREMENT таймер ЭМГ уже запущен. */
static uint8_t s_meas;

void FW_sync_stop(void)
{
  if (s_meas == 0u) {
    return;
  }
  FW_emg_stop();
  FW_angle_meas_end();
  s_meas = 0u;
}

void FW_sync_poll(void)
{
  /* Двадцать кодов одного окна. На стеке: в очередь их скопирует кодирование кадра. */
  uint16_t emg[FW_EMG_FRAME_COUNT];
  uint16_t angle;
  uint8_t frame[FW_FRAME_SAMPLE_BYTES];
  int got;

  if (FW_states_mode() != FW_MODE_MEASUREMENT) {
    FW_sync_stop();
    return;
  }
  if (s_meas == 0u) {
    if (FW_emg_start() != 0) {
      return;
    }
    s_meas = 1u;
    FW_angle_meas_begin();
  }
  got = FW_emg_take(emg);
  /* Нового окна нет — угол и кадр в этом проходе не трогаем. Пропуск — окно уже не годится. */
  if (got != 1) {
    return;
  }
  if (FW_angle_window(&angle) != 0) {
    return;
  }
  /* Метка после угла и прямо перед байтами кадра. Сетка 5 мс, не тик HAL. */
  FW_protocol_encode_sample(frame, emg, angle, fw_hw_timer_5ms_ms());
  (void)FW_ble_send(frame, FW_FRAME_SAMPLE_BYTES);
}
