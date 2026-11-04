#include "FW_states.h"

#include "fw_config.h"
#include "fw_hw.h"

/* Текущий режим. Сразу после включения режима ещё нет: батарею не смотрели. */
static fw_mode_t g_mode = FW_MODE_NONE;
/* 1, когда кабель вставлен. Запрет измерений смотрит отсюда. */
static int g_charging = 0;
/* 1 после ошибки обязательного шага. Пока питание не сняли, в ADVERTISING не пускаем. */
static int g_advertising_blocked = 0;

/* Вызывается в начале FW_init_run, чтобы понять, не закончилась ли инициализация раньше. */
fw_mode_t FW_states_mode(void)
{
  return g_mode;
}

/* Вызывается, когда собирается строка лога. Отдаёт текст вроде INITIALIZATION, не число. */
const char *FW_states_mode_name(void)
{
  switch (g_mode) {
  case FW_MODE_LOW_BATTERY:
    return FW_MODE_NAME_LOW_BATTERY;
  case FW_MODE_INITIALIZATION:
    return FW_MODE_NAME_INITIALIZATION;
  case FW_MODE_ADVERTISING:
    return FW_MODE_NAME_ADVERTISING;
  case FW_MODE_CONNECTED:
    return FW_MODE_NAME_CONNECTED;
  case FW_MODE_MEASUREMENT:
    return FW_MODE_NAME_MEASUREMENT;
  case FW_MODE_NONE:
  default:
    /* Режима ещё нет или значение побилось. В лог уходит NONE, а не случайное имя. */
    return FW_MODE_NAME_NONE;
  }
}

/* Имя режима для USB. Буфер живёт, пока передача его не дошлёт. */
void FW_states_usb_mode(void)
{
  static char line[20];
  static uint32_t last_ms;
  const char *name;
  unsigned i;
  uint32_t now;

  now = fw_hw_tick_ms();
  if ((now - last_ms) < FW_USB_MODE_PERIOD_MS) {
    return;
  }
  last_ms = now;
  name = FW_states_mode_name();
  i = 0u;
  while (name[i] != '\0' && (i + 3u) < sizeof line) {
    line[i] = name[i];
    i++;
  }
  line[i] = '\r';
  i++;
  line[i] = '\n';
  i++;
  line[i] = '\0';
  fw_hw_usb_send((const uint8_t *)line, (uint16_t)i);
}

/* 1 — кабель вставлен. Запрет входа в измерение смотрит на тот же флаг. */
int FW_states_charging(void)
{
  return g_charging;
}

/* Вызывается из проверки батареи. on приходит как «PGOOD в нуле», то есть кабель на месте. */
void FW_states_set_charging(int on)
{
  /* Любое ненулевое значение считаем «кабель вставлен», чтобы снаружи не гадать про 2 и -1. */
  g_charging = on ? 1 : 0;
}

/* Разрешён со старта, из низкого заряда и если мы уже в инициализации.
   Из ADVERTISING и дальше инициализацию заново не начинают: периферия уже поднята. */
int FW_states_to_initialization(void)
{
  if (g_mode != FW_MODE_NONE &&
      g_mode != FW_MODE_LOW_BATTERY &&
      g_mode != FW_MODE_INITIALIZATION) {
    return 0;
  }
  g_mode = FW_MODE_INITIALIZATION;
  return 1;
}

/* Вызывается, как только напряжение оказалось ниже 3,4 В. Другие режимы этому не мешают. */
int FW_states_to_low_battery(void)
{
  if (FW_LOW_BATTERY_OFF != 0) {
    return 0;
  }
  g_mode = FW_MODE_LOW_BATTERY;
  return 1;
}

/* Вызывается в конце успешного FW_init_run. */
int FW_states_to_advertising(void)
{
  /* Шаг инициализации уже падал. Ждём перезапуск, а не тихий переход дальше. */
  if (g_advertising_blocked) {
    return 0;
  }
  /* В рекламу пускает только успешная инициализация, не CONNECTED и не измерение. */
  if (g_mode != FW_MODE_INITIALIZATION) {
    return 0;
  }
  g_mode = FW_MODE_ADVERTISING;
  return 1;
}

/* Вызывается из FW_ble, когда пришла STA:connect или STA:connected.
   Из инициализации и из измерения само соединение режим не переключает. */
int FW_states_to_connected(void)
{
  if (g_mode != FW_MODE_ADVERTISING) {
    return 0;
  }
  g_mode = FW_MODE_CONNECTED;
  return 1;
}

/* Вызывается по START, когда кабеля нет. Из рекламы и из уже идущего измерения сюда не входят. */
int FW_states_to_measurement(void)
{
  if (g_mode != FW_MODE_CONNECTED || g_charging != 0) {
    return 0;
  }
  g_mode = FW_MODE_MEASUREMENT;
  return 1;
}

/* Вызывается по STOP. Измерение само после этого не продолжится: нужен новый START. */
int FW_states_stop_measurement(void)
{
  if (g_mode != FW_MODE_MEASUREMENT) {
    return 0;
  }
  g_mode = FW_MODE_CONNECTED;
  return 1;
}

/* Вызывается, когда модуль написал STA:disconnect.
   Потока измерений ещё нет, но режим измерения уже бывает: разрыв возвращает в рекламу. */
int FW_states_link_down(void)
{
  if (g_mode != FW_MODE_CONNECTED && g_mode != FW_MODE_MEASUREMENT) {
    return 0;
  }
  g_mode = FW_MODE_ADVERTISING;
  return 1;
}

/* Ставится в fail_step. Отдельного снятия нет: флаг живёт до сброса питания. */
void FW_states_block_advertising(void)
{
  g_advertising_blocked = 1;
}

int FW_states_advertising_blocked(void)
{
  return g_advertising_blocked;
}
