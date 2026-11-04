#include "FW_battery.h"

#include "FW_states.h"
#include "fw_config.h"
#include "fw_hw.h"

/* Вызывается из FW_battery_check сразу после удачного чтения АЦП.
   Переводит код ножки в милливольты на аккумуляторе: ножка видит 5/8, наружу уходит целое. */
static int32_t fw_battery_mV(uint16_t raw)
{
#if FW_BATTERY_SCALE_READY == 0 || FW_BATTERY_DIVIDER_DEN == 0 || FW_ADC_FULL_SCALE == 0
  /* Делителя нет. Делить на ноль нельзя, а выдуманный коэффициент дал бы ложные 3,4 В. */
  (void)raw;
  return 0;
#else
  /* Милливольты после опоры АЦП, делителя и сдвига. */
  int32_t scaled;

  /* Сначала все множители, потом одно деление: иначе 8/5 съедает доли милливольта. */
  scaled = (int32_t)(((uint32_t)raw * (uint32_t)FW_ADC_VREF_mV * (uint32_t)FW_BATTERY_DIVIDER_NUM) /
                     ((uint32_t)FW_ADC_FULL_SCALE * (uint32_t)FW_BATTERY_DIVIDER_DEN));
  return scaled + (int32_t)FW_BATTERY_OFFSET_mV;
#endif
}

/* Снимок для отладчика. Ноль, пока ни разу не измеряли. */
volatile fw_battery_dbg_t fw_battery_dbg;

/* Кладёт в снимок одно измерение. Вызывается на старте и потом раз в секунду. */
static void dbg_take(void)
{
  uint16_t raw = 0;
  int32_t mV;

  fw_battery_dbg.cable = (uint8_t)(fw_hw_cable_present() != 0);
  fw_battery_dbg.pgood_low = (uint8_t)(fw_hw_pgood_is_low() != 0);
  fw_battery_dbg.chg_low = (uint8_t)(fw_hw_chg_is_low() != 0);
  if (fw_hw_adc_battery_raw(&raw) != FW_HW_OK) {
    fw_battery_dbg.adc_ok = 0u;
    fw_battery_dbg.raw = 0u;
    fw_battery_dbg.mv = 0u;
    fw_battery_dbg.seq++;
    return;
  }
  fw_battery_dbg.raw = raw;
  fw_battery_dbg.adc_ok = 1u;
  mV = fw_battery_mV(raw);
  if (mV <= 0) {
    fw_battery_dbg.mv = 0u;
  } else if (mV > 65535) {
    fw_battery_dbg.mv = 65535u;
  } else {
    fw_battery_dbg.mv = (uint16_t)mV;
  }
  fw_battery_dbg.seq++;
}

fw_battery_status_t FW_battery_check(void)
{
  /* mV — милливольты на аккумуляторе из только что снятого снимка. */
  int32_t mV;

  dbg_take();
  /* Флаг кабеля нужен до решения по напряжению: при кабеле START в измерение не пускает. Сейчас это USB_SENSE. */
  FW_states_set_charging(fw_battery_dbg.cable);

  /* Преобразования не было. Код 0 при удачном чтении — уровень на входе, запуск из-за него не рвём. */
  if (fw_battery_dbg.adc_ok == 0u) {
    return FW_BATTERY_ADC_ERROR;
  }

  mV = (int32_t)fw_battery_dbg.mv;
  /* Код есть, формулы нет. Это не «батарея села»: шаги BLE и угла при таком ответе ещё идут. */
  if (FW_BATTERY_SCALE_READY == 0 || FW_BATTERY_DIVIDER_DEN == 0 || FW_ADC_FULL_SCALE == 0) {
    (void)mV;
    return FW_BATTERY_SCALE_UNKNOWN;
  }
  /* Строго ниже 3400. Ровно 3,4 В по ТЗ ещё позволяет запускаться. */
  if (mV < (int32_t)FW_BATTERY_LOW_mV) {
    return FW_BATTERY_LOW;
  }
  return FW_BATTERY_NOT_LOW;
}

/* Милливольты на аккумуляторе, уже с делителем 8/5. 0 — АЦП не ответил или формула ещё выключена. */
uint16_t FW_battery_millivolts(void)
{
  uint16_t raw = 0;
  int32_t mV;

  if (fw_hw_adc_battery_raw(&raw) != FW_HW_OK) {
    return 0u;
  }
  mV = fw_battery_mV(raw);
  if (mV <= 0) {
    return 0u;
  }
  if (mV > 65535) {
    return 65535u;
  }
  return (uint16_t)mV;
}

/* Вызывается из главного цикла. Держит флаг кабеля свежим между проверками на старте. */
void FW_battery_poll(void)
{
  FW_states_set_charging(fw_hw_cable_present());
}

/* Вызывается из главного цикла каждый проход.
   Секунду отмеряем тиком HAL. Снимок нужен в отладчике и до рекламы: на макете USB нет. */
void FW_battery_debug_poll(void)
{
  /* 0, пока ещё ни разу не снимали по таймеру. Иначе первый снимок ждал бы целую секунду. */
  static uint8_t started;
  /* Тик прошлого снимка. Следующий — не раньше чем через FW_BATTERY_DEBUG_PERIOD_MS. */
  static uint32_t last_ms;
  uint32_t now;

  now = fw_hw_tick_ms();
  if (started != 0u && (now - last_ms) < FW_BATTERY_DEBUG_PERIOD_MS) {
    return;
  }
  started = 1u;
  last_ms = now;
  dbg_take();
}
