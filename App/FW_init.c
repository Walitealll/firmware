#include "FW_init.h"

#include "FW_angle.h"
#include "FW_battery.h"
#include "FW_ble.h"
#include "FW_diagnostics.h"
#include "FW_states.h"
#include "fw_config.h"
#include "fw_hw.h"

/* Периферия, которая уже проверена, идёт в текст раньше упавшей строки. */
static void report_head(int clock_ok, int uart_ok, int i2c_ok)
{
  if (clock_ok == 0) {
    FW_diagnostics_puts("Core is not 72 MHz\r\n");
    return;
  }
  FW_diagnostics_puts("Core ");
  FW_diagnostics_putu(FW_SYSCLK_HZ / 1000000u);
  FW_diagnostics_puts(" MHz\r\n");
  if (uart_ok == 0) {
    FW_diagnostics_puts("UART1 not ready\r\n");
    return;
  }
  FW_diagnostics_puts("UART1 ");
  FW_diagnostics_putu(FW_BLE_UART_BAUD_BPS);
  FW_diagnostics_puts("\r\n");
  if (i2c_ok == 0) {
    FW_diagnostics_puts("I2C not ready\r\n");
    return;
  }
  FW_diagnostics_puts("I2C1 ");
  FW_diagnostics_putu(FW_BNO_I2C_HZ / 1000u);
  FW_diagnostics_puts(" kHz\r\nI2C2 ");
  FW_diagnostics_putu(FW_BNO_I2C_HZ / 1000u);
  FW_diagnostics_puts(" kHz\r\nTIM2 ");
  FW_diagnostics_putu(1000u / FW_SYNC_TICK_MS);
  FW_diagnostics_puts(" Hz\r\n");
}

/* stage — то, что вернул FW_ble_init_stage. Успех печатает ровно согласованную строку. */
static void report_ble(int stage)
{
  if (stage < 0) {
    FW_diagnostics_puts("BLE: no AT\r\n");
    return;
  }
  FW_diagnostics_puts("BLE: AT ok");
  if (stage == 0) {
    FW_diagnostics_puts(", setup rejected\r\n");
    return;
  }
  if (stage == 1) {
    FW_diagnostics_puts(", baud is not 10\r\n");
    return;
  }
  FW_diagnostics_puts(", baud 10 (115200)");
  if (stage == 2) {
    FW_diagnostics_puts(", setup rejected\r\n");
    return;
  }
  if (stage == 3) {
    FW_diagnostics_puts(", status is not 1\r\n");
    return;
  }
  FW_diagnostics_puts(", status 1");
  if (stage == 4) {
    FW_diagnostics_puts(", adv is not 1\r\n");
    return;
  }
  FW_diagnostics_puts(", adv 1");
  if (stage == 8) {
    FW_diagnostics_puts(", connection is not 6,0,400\r\n");
    return;
  }
  if (stage == 7) {
    FW_diagnostics_puts(", radio stayed on\r\n");
    return;
  }
  if (stage == 5) {
    FW_diagnostics_puts(", MOD is not high\r\n");
    return;
  }
  FW_diagnostics_puts("\r\n");
}

/* Одна строка датчика. При успехе порядок полей тот, что согласован для терминала. */
static void report_bno(const char *name, const fw_bno_fact_t *fact)
{
  FW_diagnostics_puts(name);
  FW_diagnostics_puts(": ");
  if (fact->fail == FW_BNO_FAIL_COUNT) {
    FW_diagnostics_puts(fact->addr_probe_mask != 0u ?
      "both I2C addresses answered\r\n" : "sensor did not answer\r\n");
    return;
  }
  if (fact->fail == FW_BNO_FAIL_I2C && fact->bus == 0u) {
    FW_diagnostics_puts("no I2C\r\n");
    return;
  }
  if (fact->fail == FW_BNO_FAIL_MAP || fact->bus == 0u) {
    FW_diagnostics_puts("reset did not tell them apart\r\n");
    return;
  }
  FW_diagnostics_puts("bus I2C");
  FW_diagnostics_putu(fact->bus);
  FW_diagnostics_puts(", addr 0x");
  FW_diagnostics_puthex2(fact->addr7);
  if (fact->fail == FW_BNO_FAIL_I2C) {
    FW_diagnostics_puts(", no I2C\r\n");
    return;
  }
  FW_diagnostics_puts(", I2C ok");
  if (fact->fail == FW_BNO_FAIL_ID) {
    FW_diagnostics_puts(", CHIP_ID is not 0xA0\r\n");
    return;
  }
  if (fact->fail == FW_BNO_FAIL_RESET) {
    FW_diagnostics_puts(", software reset failed\r\n");
    return;
  }
  if (fact->fail == FW_BNO_FAIL_CLOCK) {
    FW_diagnostics_puts(", clock setup failed, SYS_TRIGGER 0x");
    FW_diagnostics_puthex2(fact->sys_trigger);
    FW_diagnostics_puts(", waited ms ");
    FW_diagnostics_putu(fact->clock_wait_ms);
    FW_diagnostics_puts("\r\n");
    return;
  }
  if (fact->fail == FW_BNO_FAIL_PWR) {
    FW_diagnostics_puts(", power is not NORMAL\r\n");
    return;
  }
  if (fact->fail == FW_BNO_FAIL_UNIT) {
    FW_diagnostics_puts(", power NORMAL, units ");
    if (fact->unit_known != 0u) {
      FW_diagnostics_putu(fact->unit);
    } else {
      FW_diagnostics_puts("not read");
    }
    FW_diagnostics_puts("\r\n");
    return;
  }
  if (fact->fail == FW_BNO_FAIL_MODE) {
    FW_diagnostics_puts(", mode is not active\r\n");
    return;
  }
  FW_diagnostics_puts(", mode NDOF, power NORMAL, units ");
  FW_diagnostics_putu(fact->unit);
  FW_diagnostics_puts((fact->sys_trigger & FW_BNO_CLK_EXT_MASK) != 0u ? ", clock external" : ", clock internal");
  if (fact->fail == FW_BNO_FAIL_QUAT) {
    FW_diagnostics_puts(", no orientation\r\n");
    return;
  }
  if (fact->fail == FW_BNO_FAIL_STATUS_IO) {
    FW_diagnostics_puts(", status not read\r\n");
    return;
  }
  FW_diagnostics_puts(", SYS_STATUS ");
  FW_diagnostics_putu(fact->sys_status);
  FW_diagnostics_puts(", SYS_ERR ");
  FW_diagnostics_putu(fact->sys_err);
  if (fact->fail == FW_BNO_FAIL_NONE) {
    FW_diagnostics_puts(", Euler read ok");
  }
  FW_diagnostics_puts("\r\n");
}

static void report_bno_pair(void)
{
  fw_bno_fact_t internal;
  fw_bno_fact_t external;

  FW_angle_facts(&internal, &external);
  report_bno("Internal", &internal);
  if (internal.fail == FW_BNO_FAIL_NONE) {
    report_bno("External", &external);
  }
}

/* Вызывается между шагами и при ошибке шага.
   Нужна, чтобы низкий заряд перебил любой другой итог: при нём строку диагностики не пишем. */
static int voltage_is_low(void)
{
  if (FW_LOW_BATTERY_OFF != 0) {
    return 0;
  }
  return FW_battery_check() == FW_BATTERY_LOW;
}

/* Вызывается, когда обязательный шаг не прошёл: АЦП, такт, UART, BLE или датчик угла.
   Закрывает переход в ADVERTISING и оставляет устройство в INITIALIZATION до перезапуска.
   Если к этому моменту батарея уже ниже 3,4 В, отчёт об ошибке не нужен — уходим в LOW_BATTERY. */
static void fail_step(const char *module, const char *code)
{
  /* Заряд сел, пока шёл упавший шаг. Это важнее, чем записать, какой шаг сломался. */
  if (voltage_is_low()) {
    FW_states_to_low_battery();
    return;
  }
  FW_states_to_initialization();
  FW_states_block_advertising();
  FW_diagnostics_report(module, FW_DIAG_LEVEL_ERR, code);
}

/* module — имя шага для строки лога, если упадёт датчик угла. До угла стоит заглушка.
   code — короткая метка ошибки. До BLE стоит метка «нет ответа AT», её перезапишет сам шаг. */
void FW_init_run(void)
{
  /* Режим до этого прохода. Из рекламы и дальше инициализацию не повторяем. */
  fw_mode_t mode = FW_states_mode();
  /* Итог батареи: низкий заряд, ошибка АЦП или порог ещё не из чего считать. */
  fw_battery_status_t battery;
  /* Метка ошибки. До BLE стоит «нет ответа AT», шаг её перезапишет. */
  const char *code = FW_DIAG_CODE_AT;
  /* Имя шага в строке лога. До угла считаем, что не прошёл внутренний датчик. */
  const char *module = FW_DIAG_MODULE_ANGLE_INT;

  /* Сюда попадаем, только если инициализация уже когда-то успешно кончилась
     или устройство ушло в работу. Повторно периферию не поднимаем. */
  if (mode == FW_MODE_ADVERTISING || mode == FW_MODE_CONNECTED || mode == FW_MODE_MEASUREMENT) {
    return;
  }
  /* Прошлый запуск сломался на обязательном шаге. Лечится выключением питания, не новым проходом. */
  if (FW_states_advertising_blocked()) {
    return;
  }

  /* COM поднимаем сразу: пока идут BLE и датчики, хост успевает перечислить устройство. */
  fw_hw_usb_start();

  battery = FW_battery_check();
  /* Батарея ниже 3,4 В. Пока FW_LOW_BATTERY_OFF, этот порог запуск не обрывает. */
  if (battery == FW_BATTERY_LOW && FW_LOW_BATTERY_OFF == 0) {
    FW_states_to_low_battery();
    return;
  }
  /* АЦП батареи не ответил. Это ошибка базовой инициализации, дальше идти нельзя. */
  if (battery == FW_BATTERY_ADC_ERROR) {
    FW_diagnostics_begin();
    FW_diagnostics_puts("no ADC\r\n");
    FW_diagnostics_publish();
    fail_step(FW_DIAG_MODULE_INIT, FW_DIAG_CODE_ADC);
    return;
  }
  /* Напряжение не низкое (или делитель ещё не задан). Входим в INITIALIZATION.
     Если режим уже ADVERTISING и выше, переход запрещён — это сбой порядка вызовов. */
  if (!FW_states_to_initialization()) {
    fail_step(FW_DIAG_MODULE_INIT, FW_DIAG_CODE_PERIPH);
    return;
  }
  /* Такт, UART модуля или шины датчиков не такие, как в схеме. BLE и угол на этом не запускаем. */
  FW_diagnostics_begin();
  if (!fw_hw_clock_ready()) {
    report_head(0, 0, 0);
    FW_diagnostics_publish();
    fail_step(FW_DIAG_MODULE_INIT, FW_DIAG_CODE_PERIPH);
    return;
  }
  if (!fw_hw_uart_ready()) {
    report_head(1, 0, 0);
    FW_diagnostics_publish();
    fail_step(FW_DIAG_MODULE_INIT, FW_DIAG_CODE_PERIPH);
    return;
  }
  if (!fw_hw_i2c_ready()) {
    report_head(1, 1, 0);
    FW_diagnostics_publish();
    fail_step(FW_DIAG_MODULE_INIT, FW_DIAG_CODE_PERIPH);
    return;
  }
  report_head(1, 1, 1);

  /* Движок к B. Если байт не ушёл, модуль и датчики не настраиваем. */
  if (fw_hw_dp_wiper_min() != FW_HW_OK) {
    FW_diagnostics_puts("Potentiometer: byte was not sent\r\n");
    FW_diagnostics_publish();
    fail_step(FW_DIAG_MODULE_INIT, FW_DIAG_CODE_POT);
    return;
  }

  /* Модуль E104 не ответил или настройки не сошлись. Датчики угла уже не инициализируем. */
  if (FW_ble_init(&code) != 0) {
    report_ble(FW_ble_init_stage());
    FW_diagnostics_publish();
    fail_step(FW_DIAG_MODULE_BLE, code);
    return;
  }
  report_ble(FW_ble_init_stage());
  /* Пока говорили с модулем, батарея просела. Угол не трогаем и строку успеха не пишем. */
  if (voltage_is_low()) {
    FW_states_to_low_battery();
    return;
  }

  /* Сначала внутренний датчик, потом внешний. module скажет, какой из двух не прошёл. */
  if (FW_angle_init(&module, &code) != 0) {
    report_bno_pair();
    FW_diagnostics_publish();
    fail_step(module, code);
    return;
  }
  report_bno_pair();
  /* Внутренний датчик поднялся, но заряд уже ниже порога. В ADVERTISING не входим. */
  if (voltage_is_low()) {
    FW_states_to_low_battery();
    return;
  }

  /* Приём уже стоит, и только потом модуль выходит в эфир. Иначе STA:connect за время команды теряется. */
  if (FW_ble_advertising_on(&code) != 0) {
    FW_diagnostics_puts("Advertising did not start\r\n");
    FW_diagnostics_publish();
    fail_step(FW_DIAG_MODULE_BLE, code);
    return;
  }
  /* Строку пишем уже после перехода: иначе в ней навсегда остаётся INITIALIZATION. */
  if (!FW_states_to_advertising()) {
    FW_ble_advertising_off();
    FW_diagnostics_puts("Mode did not change\r\n");
    FW_diagnostics_publish();
    fail_step(FW_DIAG_MODULE_INIT, FW_DIAG_CODE_PERIPH);
    return;
  }
  FW_ble_accept_early_link();
  FW_diagnostics_puts("Advertising on\r\n");
  FW_diagnostics_publish();
  FW_diagnostics_report(FW_DIAG_MODULE_DIAG, FW_DIAG_LEVEL_OK, FW_DIAG_CODE_OK);
}
