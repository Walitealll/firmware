#include "FW_ble.h"

#include "FW_battery.h"
#include "FW_diagnostics.h"
#include "FW_protocol.h"
#include "FW_states.h"
#include "FW_sync.h"
#include "fw_config.h"
#include "fw_hw.h"

#include <string.h>

/* Вызывается перед каждой AT-командой.
   Вычитывает байты, которые уже лежат в приёмнике, чтобы старый ответ не склеился с новым.
   Нового байта не ждёт. */
static void drain_rx(void)
{
  /* Очередной байт из приёмника. Содержимое выбрасываем. */
  uint8_t byte = 0;
  /* n ограничивает цикл: если модуль сыплет байтами без остановки, мы не застрянем здесь навсегда. */
  unsigned n = 0;

  while (n < FW_BLE_AT_LINE_BYTES && fw_hw_uart_rx(&byte, 0u) == FW_HW_OK) {
    n++;
  }
}

/* Вызывается сразу после отправки команды.
   Собирает ответ до перевода строки. Возвращает 0, когда строка готова, и -1, если модуль молчит или строка битая. */
static int read_line(char *buf, unsigned cap)
{
  /* n — сколько полезных символов уже сложили, без \r и \n. */
  unsigned n = 0;
  /* Момент старта ожидания строки. От него считается таймаут. */
  uint32_t start = fw_hw_tick_ms();

  /* В буфере должно хватить места хотя бы на один символ и закрывающий ноль. */
  if (cap < 2u) {
    return -1;
  }

  for (;;) {
    /* Сколько миллисекунд уже ждём и сколько ещё можно ждать этот байт. */
    uint32_t elapsed = fw_hw_tick_ms() - start;
    uint32_t left;
    /* Принятый байт и чем кончилось чтение: байт есть, тишина или ошибка UART. */
    uint8_t byte = 0;
    fw_hw_status_t st;

    /* Ждали дольше предела. Модуль на эту команду не ответил. */
    if (elapsed >= FW_IO_TIMEOUT_MS) {
      break;
    }
    left = FW_IO_TIMEOUT_MS - elapsed;
    st = fw_hw_uart_rx(&byte, left);

    /* За оставшееся время байт так и не пришёл. */
    if (st == FW_HW_TIMEOUT) {
      break;
    }
    /* UART сломался, это не просто тишина. */
    if (st != FW_HW_OK) {
      return -1;
    }
    if (byte == (uint8_t)'\n') {
      /* Пустая строка до текста — модуль иногда начинает ответ с пустого \r\n. Её пропускаем. */
      if (n > 0u) {
        buf[n] = '\0';
        return 0;
      }
      continue;
    }
    /* \r к тексту ответа не относится, конец строки узнаём по \n. */
    if (byte == (uint8_t)'\r') {
      continue;
    }
    /* Ответ длиннее буфера. Это уже не короткая строка +OK, дальше копить нельзя. */
    if ((n + 1u) >= cap) {
      return -1;
    }
    buf[n] = (char)byte;
    n++;
  }
  return -1;
}

/* Вызывается на каждую команду настройки и проверки.
   Шлёт cmd без добавления \r\n и сверяет ответ с expect.
   fail_code попадает в лог, если ответ есть, но он не тот. */
static int send_expect(const char *cmd, const char *expect, const char *fail_code, const char **code)
{
  /* Ответ модуля одной строкой, без перевода строки. */
  char line[FW_BLE_AT_LINE_BYTES];
  /* Длина команды в байтах. В UART уходит ровно столько. */
  unsigned len = 0;

  while (cmd[len] != '\0') {
    len++;
  }
  /* Пустую команду слать нечего. Слишком длинная не влезет в один вызов UART. */
  if (len == 0u || len > 0xFFFFu) {
    *code = fail_code;
    return -1;
  }

  drain_rx();
  /* Байт ушёл не весь. Ответ ждать бессмысленно. */
  if (fw_hw_uart_tx((const uint8_t *)cmd, (uint16_t)len) != FW_HW_OK) {
    *code = FW_DIAG_CODE_AT;
    return -1;
  }
  /* Модуль промолчал. */
  if (read_line(line, (unsigned)sizeof line) != 0) {
    *code = FW_DIAG_CODE_AT;
    return -1;
  }
  /* Команды пошли слишком часто. Отдельной паузы в мануале нет, поэтому это ошибка шага, а не повод ждать наугад. */
  if (strcmp(line, FW_BLE_RSP_ERR_GAP) == 0) {
    *code = FW_DIAG_CODE_AT_GAP;
    return -1;
  }
  /* Любая другая ошибка модуля. */
  if (strncmp(line, FW_BLE_RSP_ERR_PREFIX, sizeof FW_BLE_RSP_ERR_PREFIX - 1u) == 0) {
    *code = fail_code;
    return -1;
  }
  /* Ответ пришёл, но это не то, что ждём. Настройка не принялась. */
  if (strcmp(line, expect) != 0) {
    *code = fail_code;
    return -1;
  }
  return 0;
}

/* Вызывается в начале настройки и ещё раз после AT+RESET.
   Если модуль не сказал +OK, один раз будит его по WKP и спрашивает снова. Второго пробуждения нет. */
static int at_ping(const char **code)
{
  if (send_expect(FW_BLE_CMD_AT, FW_BLE_RSP_OK, FW_DIAG_CODE_AT, code) == 0) {
    return 0;
  }
  fw_hw_ble_wake();
  return send_expect(FW_BLE_CMD_AT, FW_BLE_RSP_OK, FW_DIAG_CODE_AT, code);
}

/* Смотри FW_ble_init_stage. До первого вызова инициализации шага ещё не было. */
static int s_ble_stage = -2;

int FW_ble_init_stage(void)
{
  return s_ble_stage;
}

/* Вызывается после опроса, когда модуль уже ответил следом за AT+RESET.
   Защёлкивается смена уровня, а не «пин и раньше был высоким»: короче 200 мс она недействительна.
   До соединения команды AT от этого уровня не зависят. */
static int latch_mod(const char **code)
{
  fw_hw_ble_mod_low();
  fw_hw_delay_ms(FW_BLE_MOD_HOLD_MS);
  fw_hw_ble_mod_high();
  fw_hw_delay_ms(FW_BLE_MOD_HOLD_MS);
  if (!fw_hw_ble_mod_is_high()) {
    *code = FW_DIAG_CODE_MOD;
    return -1;
  }
  return 0;
}

int FW_ble_init(const char **code)
{
  /* Команды, которые надо записать до сброса. Ответ на каждую — просто +OK. */
  static const char *const setup[] = {
    FW_BLE_CMD_BAUD,
    FW_BLE_CMD_PARI,
    FW_BLE_CMD_HWFC,
    FW_BLE_CMD_ROLE,
    FW_BLE_CMD_ONSLEEP,
    FW_BLE_CMD_LOGMSG,
    FW_BLE_CMD_ADV,
    FW_BLE_CMD_CONPARAMS
  };
  /* После сброса эти же параметры читаем обратно и сверяем с тем, что записывали. */
  static const struct {
    const char *cmd;
    const char *rsp;
  } query[] = {
    { FW_BLE_CMD_BAUD_Q, FW_BLE_RSP_BAUD },
    { FW_BLE_CMD_PARI_Q, FW_BLE_RSP_ZERO },
    { FW_BLE_CMD_HWFC_Q, FW_BLE_RSP_ZERO },
    { FW_BLE_CMD_ROLE_Q, FW_BLE_RSP_ZERO },
    { FW_BLE_CMD_ONSLEEP_Q, FW_BLE_RSP_ZERO },
    { FW_BLE_CMD_LOGMSG_Q, FW_BLE_RSP_ONE },
    { FW_BLE_CMD_ADV_Q, FW_BLE_RSP_ONE }
  };
  /* Номер команды в списке записи или проверки. */
  unsigned i;

  *code = FW_DIAG_CODE_AT;
  s_ble_stage = -1;

  /* RST держим высоким. Сколько миллисекунд должен длиться импульс сброса, в мануале нет, поэтому импульс не делаем.
     MOD здесь не трогаем: защёлку делаем в конце, когда модуль уже жив после своего сброса. */
  fw_hw_ble_rst_idle();

  /* UART ещё не в том кадре, который ждёт модуль. Команды слать рано. */
  if (!fw_hw_uart_ready()) {
    *code = FW_DIAG_CODE_PERIPH;
    return -1;
  }
  /* Модуль не ответил даже после одного пробуждения. Настройки не пишем. */
  if (at_ping(code) != 0) {
    s_ble_stage = -1;
    return -1;
  }
  s_ble_stage = 0;

  /* Одна из записей не принялась. Сброс и повторную проверку уже не делаем. */
  for (i = 0; i < (unsigned)(sizeof setup / sizeof setup[0]); i++) {
    if (send_expect(setup[i], FW_BLE_RSP_OK, FW_DIAG_CODE_PARAM, code) != 0) {
      s_ble_stage = 0;
      return -1;
    }
  }
  /* Сброс нужен, чтобы скорость, чётность, поток и роль начали действовать. */
  if (send_expect(FW_BLE_CMD_RESET, FW_BLE_RSP_OK, FW_DIAG_CODE_PARAM, code) != 0) {
    s_ble_stage = 0;
    return -1;
  }
  /* После сброса модуль мог не успеть или уснуть. Снова спрашиваем AT, при тишине — один WKP. */
  if (at_ping(code) != 0) {
    s_ble_stage = -1;
    return -1;
  }

  /* Записанное не совпало с прочитанным. В ADVERTISING с такими настройками не идём. */
  for (i = 0; i < (unsigned)(sizeof query / sizeof query[0]); i++) {
    if (send_expect(query[i].cmd, query[i].rsp, FW_DIAG_CODE_PARAM, code) != 0) {
      /* 0 — скорость. 1…4 — чётность, поток, роль, автосон: в текст они не входят. 5 — статус, 6 — реклама. */
      if (i == 0u) {
        s_ble_stage = 1;
      } else if (i < 5u) {
        s_ble_stage = 2;
      } else if (i == 5u) {
        s_ble_stage = 3;
      } else {
        s_ble_stage = 4;
      }
      return -1;
    }
  }
  /* Интервал пишется до сброса и сохраняется. Сверяем уже после сброса. */
  if (send_expect(FW_BLE_CMD_CONPARAMS_Q, FW_BLE_RSP_CONPARAMS, FW_DIAG_CODE_PARAM, code) != 0) {
    s_ble_stage = 8;
    return -1;
  }
  /* AT+ADV? уже ответил 1. Датчики занимают секунды, приём STA ещё выключен.
     Гасим эфир, чтобы телефон не соединился, пока режим ещё INITIALIZATION.
     Включим снова непосредственно перед входом в ADVERTISING. */
  if (send_expect(FW_BLE_CMD_ADV_OFF, FW_BLE_RSP_OK, FW_DIAG_CODE_PARAM, code) != 0) {
    s_ble_stage = 7;
    return -1;
  }
  if (latch_mod(code) != 0) {
    s_ble_stage = 5;
    return -1;
  }
  s_ble_stage = 6;
  return 0;
}

/* Последняя узнанная строка соединения. Пустая, пока телефон не подключился.
   В отладчике её смотрят так же, как g_line. */
static char g_sta[FW_BLE_AT_LINE_BYTES];
/* Строка, которую сейчас собираем из кольца. Пустая, пока это не начало STA. */
static char s_acc[FW_BLE_AT_LINE_BYTES];
/* Сколько байт уже лежит в s_acc. */
static unsigned s_acc_n;
/* 1, если соединение пришло на последнее AT+ADV=1 раньше, чем режим стал ADVERTISING.
   Эти байты из кольца уже забраны, повторно в FW_ble_poll они не придут. */
static uint8_t s_early_link;

/* Кладёт текст строки STA в g_sta, чтобы её было видно в отладчике. */
static void remember_sta(const char *line)
{
  unsigned i = 0u;

  while (line[i] != '\0' && (i + 1u) < FW_BLE_AT_LINE_BYTES) {
    g_sta[i] = line[i];
    i++;
  }
  g_sta[i] = '\0';
}

/* Одна строка из кольца приёма. start — начало ожидания +OK, общее на все строки этого ответа.
   Пустой перевод строки пропускается. Возврат 0 — в buf текст строки, без перевода строки. */
static int ring_text(char *buf, unsigned cap, uint32_t start)
{
  unsigned n = 0u;

  if (cap < 2u) {
    return -1;
  }
  for (;;) {
    uint8_t byte = 0u;

    if ((fw_hw_tick_ms() - start) >= FW_IO_TIMEOUT_MS) {
      return -1;
    }
    if (fw_hw_uart_take(&byte) != FW_HW_OK) {
      continue;
    }
    if (byte == (uint8_t)'\n') {
      if (n > 0u) {
        buf[n] = '\0';
        return 0;
      }
      continue;
    }
    if (byte == (uint8_t)'\r') {
      continue;
    }
    if ((n + 1u) >= cap) {
      return -1;
    }
    buf[n] = (char)byte;
    n++;
  }
}

/* Ждёт +OK на AT+ADV=1. Строки STA за это же время не выбрасывает:
   connect запоминает, прочие статусные пропускает. Чужой текст — отказ. */
static int wait_adv_ok(void)
{
  char line[FW_BLE_AT_LINE_BYTES];
  uint32_t start = fw_hw_tick_ms();

  for (;;) {
    if (ring_text(line, (unsigned)sizeof line, start) != 0) {
      return -1;
    }
    if (strcmp(line, FW_BLE_RSP_OK) == 0) {
      return 0;
    }
    if (strcmp(line, FW_BLE_STA_CONNECT) == 0 || strcmp(line, FW_BLE_STA_CONNECTED) == 0) {
      remember_sta(line);
      s_early_link = 1u;
      continue;
    }
    if (strcmp(line, FW_BLE_STA_DISCONNECT) == 0 ||
        strcmp(line, FW_BLE_STA_WAKEUP) == 0 ||
        strcmp(line, FW_BLE_STA_SLEEP) == 0) {
      continue;
    }
    return -1;
  }
}

/* Команда без конца строки. Ответ здесь не читается: его забирает кольцо. */
static int tx_cmd(const char *cmd)
{
  unsigned len = 0u;

  while (cmd[len] != '\0') {
    len++;
  }
  if (len == 0u || len > 0xFFFFu) {
    return -1;
  }
  if (fw_hw_uart_tx((const uint8_t *)cmd, (uint16_t)len) != FW_HW_OK) {
    return -1;
  }
  return 0;
}

int FW_ble_advertising_on(const char **code)
{
  s_early_link = 0u;
  *code = FW_DIAG_CODE_PERIPH;
  /* Старые байты опроса AT не должны склеиться с +OK. Приём в прерывание ещё выключен. */
  drain_rx();
  /* Кольцо раньше команды: строка соединения, пришедшая вместе с ответом, не теряется. */
  if (fw_hw_uart_rx_listen() == 0) {
    return -1;
  }
  *code = FW_DIAG_CODE_PARAM;
  if (tx_cmd(FW_BLE_CMD_ADV) != 0) {
    return -1;
  }
  if (wait_adv_ok() != 0) {
    s_early_link = 0u;
    /* Команда могла успеть включить эфир, а +OK мы не дождались. Гасим, режим не меняем. */
    (void)tx_cmd(FW_BLE_CMD_ADV_OFF);
    return -1;
  }
  return 0;
}

void FW_ble_advertising_off(void)
{
  if (tx_cmd(FW_BLE_CMD_ADV_OFF) != 0) {
    return;
  }
  (void)wait_adv_ok();
}

void FW_ble_accept_early_link(void)
{
  if (s_early_link == 0u) {
    return;
  }
  /* Пускает только из ADVERTISING. Раньше смены режима вызов ничего не снимет. */
  if (FW_states_to_connected() == 0) {
    return;
  }
  s_early_link = 0u;
  FW_sync_power_arm();
}

/* Байт i внутри кадра \r\n + текст STA + \r\n. known_len — длина текста без перевода строки. */
static char frame_at(const char *known, unsigned known_len, unsigned i)
{
  if (i == 0u) {
    return '\r';
  }
  if (i == 1u) {
    return '\n';
  }
  if (i < (known_len + 2u)) {
    return known[i - 2u];
  }
  if (i == (known_len + 2u)) {
    return '\r';
  }
  return '\n';
}

/* 1 — text ещё может быть строкой статуса модуля в оболочке \r\n ... \r\n.
   exact становится 1, когда оболочка и текст совпали целиком. */
static int frame_holds(const char *text, unsigned n, int *exact)
{
  static const char *const known[] = {
    FW_BLE_STA_CONNECT,
    FW_BLE_STA_CONNECTED,
    FW_BLE_STA_DISCONNECT,
    FW_BLE_STA_WAKEUP,
    FW_BLE_STA_SLEEP
  };
  /* Номер известной строки STA, с которой сейчас сравниваем. */
  unsigned i;

  *exact = 0;
  for (i = 0u; i < (unsigned)(sizeof known / sizeof known[0]); i++) {
    /* Длина текста STA без оболочки \r\n, номер байта в наборе и совпал ли он. */
    unsigned k = 0u;
    unsigned j;
    int same = 1;

    while (known[i][k] != '\0') {
      k++;
    }
    if (n > (k + 4u)) {
      continue;
    }
    for (j = 0u; j < n; j++) {
      if (text[j] != frame_at(known[i], k, j)) {
        same = 0;
        break;
      }
    }
    if (!same) {
      continue;
    }
    if (n == (k + 4u)) {
      *exact = 1;
    }
    return 1;
  }
  return 0;
}

/* Вызывается для байтов, которые уже не могут быть строкой STA. */
static void feed_payload(const char *text, unsigned n)
{
  /* Номер байта, который отдаём в разбор команды. */
  unsigned i;

  for (i = 0u; i < n; i++) {
    FW_protocol_rx_byte((uint8_t)text[i]);
  }
}

static void take_line(void);

/* Один байт из кольца. Строка статуса — это \r\nSTA:...\r\n целиком.
   Пока набор байт на неё похож, держим их. Иначе отдаём в команду, включая \r и \n. */
static void take_rx(uint8_t byte)
{
  /* 1 — оболочка и текст STA совпали целиком. */
  int exact = 0;
  /* Последний байт набора. Если это \r, с него может начаться следующая строка. */
  char last;

  if (s_acc_n == 0u && byte != (uint8_t)'\r') {
    FW_protocol_rx_byte(byte);
    return;
  }
  if ((s_acc_n + 1u) >= FW_BLE_AT_LINE_BYTES) {
    feed_payload(s_acc, s_acc_n);
    s_acc_n = 0u;
    if (byte != (uint8_t)'\r') {
      FW_protocol_rx_byte(byte);
      return;
    }
  }
  s_acc[s_acc_n] = (char)byte;
  s_acc_n++;
  if (frame_holds(s_acc, s_acc_n, &exact) == 0) {
    last = s_acc[s_acc_n - 1u];
    if (s_acc_n > 1u) {
      feed_payload(s_acc, s_acc_n - 1u);
    }
    s_acc_n = 0u;
    if (last == '\r') {
      s_acc[0] = '\r';
      s_acc_n = 1u;
      return;
    }
    FW_protocol_rx_byte((uint8_t)last);
    return;
  }
  if (exact == 0) {
    return;
  }
  memmove(s_acc, s_acc + 2, s_acc_n - 4u);
  s_acc_n -= 4u;
  s_acc[s_acc_n] = '\0';
  take_line();
  s_acc_n = 0u;
}

/* Слоты исходящей очереди. DMA читает тот, что сейчас в хвосте, пока цикл кладёт новые в голову. */
static uint8_t s_tx_slot[FW_BLE_TX_SLOTS][FW_BLE_TX_SLOT_BYTES];
/* Длина кадра в каждом слоте. */
static uint8_t s_tx_len[FW_BLE_TX_SLOTS];
/* Куда кладём следующий кадр и откуда DMA его забирает. */
static unsigned s_tx_head;
static unsigned s_tx_tail;
/* Сколько слотов занято. */
static unsigned s_tx_count;
/* Сколько слотов ещё не ушло. После кадра на телефоне здесь 0. */
static unsigned g_tx_left;
/* 1, пока DMA ещё не дописал текущий слот. */
static uint8_t s_tx_busy;
/* Срок следующего старта, миллисекунды сетки 5 мс. До первого кадра расписания нет. */
static uint32_t s_tx_next_ms;
static uint8_t s_tx_paced;
/* Сколько раз срок уже прошёл, а кадр ещё был на линии. Пачкой такие пропуски не догоняем. */
static unsigned g_tx_miss;

/* Кладёт один кадр в очередь и сразу выходит. Места нет — кадр не берём, цикл не ждёт. */
static int tx_put(const uint8_t *data, unsigned len)
{
  /* Номер байта, который копируем в слот. */
  unsigned i;

  if (data == 0 || len == 0u || len > FW_BLE_TX_SLOT_BYTES) {
    return -1;
  }
  if (s_tx_count >= FW_BLE_TX_SLOTS) {
    FW_diagnostics_report(FW_DIAG_MODULE_BLE, FW_DIAG_LEVEL_ERR, FW_DIAG_CODE_TXQ);
    return -1;
  }
  for (i = 0u; i < len; i++) {
    s_tx_slot[s_tx_head][i] = data[i];
  }
  s_tx_len[s_tx_head] = (uint8_t)len;
  s_tx_head = (s_tx_head + 1u) % FW_BLE_TX_SLOTS;
  s_tx_count++;
  g_tx_left = s_tx_count;
  return 0;
}

int FW_ble_send(const uint8_t *data, unsigned len)
{
  if (FW_states_mode() != FW_MODE_MEASUREMENT) {
    return -1;
  }
  return tx_put(data, len);
}

/* Один кадр за вызов, не чаще чем раз в FW_SAMPLE_PERIOD_MS.
   Конец передачи не ждём: линия ещё занята — выходим, очередь остаётся на месте.
   Если срок наступил, пока прошлый кадр ещё шёл, следующий старт отодвигается на период.
   Накопившиеся слоты подряд не выталкиваются. */
static void tx_pump(void)
{
  /* Сейчас по сетке 5 мс. Ею же метим измерительный кадр. */
  uint32_t now;

  if (s_tx_busy != 0u) {
    if (fw_hw_uart_tx_done() == 0) {
      return;
    }
    s_tx_busy = 0u;
    s_tx_tail = (s_tx_tail + 1u) % FW_BLE_TX_SLOTS;
    s_tx_count--;
    g_tx_left = s_tx_count;
  }
  if (s_tx_count == 0u || s_tx_busy != 0u) {
    return;
  }
  /* Бит ещё на линии. Слот не трогаем и DMA снова не включаем. */
  if (fw_hw_uart_tx_line_idle() == 0) {
    return;
  }
  now = fw_hw_timer_5ms_ms();
  if (s_tx_paced == 0u) {
    s_tx_next_ms = now;
    s_tx_paced = 1u;
  }
  if ((int32_t)(now - s_tx_next_ms) < 0) {
    return;
  }
  if (fw_hw_uart_tx_dma(s_tx_slot[s_tx_tail], s_tx_len[s_tx_tail]) != FW_HW_OK) {
    return;
  }
  s_tx_busy = 1u;
  s_tx_next_ms += FW_SAMPLE_PERIOD_MS;
  /* Старт и так опоздал больше чем на период. Догонять пачкой нечем. */
  if ((int32_t)(now - s_tx_next_ms) >= 0) {
    g_tx_miss++;
    s_tx_next_ms = now + FW_SAMPLE_PERIOD_MS;
  }
}

/* 0 — кабеля нет, 1 — заряд идёт, 2 — кабель есть и заряд окончен.
   Пока кабель читается с USB_SENSE, вставленный кабель и есть зарядка. */
static uint8_t charge_code(void)
{
#if FW_CABLE_SRC == FW_CABLE_SRC_PGOOD
  if (fw_hw_pgood_is_low() == 0) {
    return FW_CHARGE_NONE;
  }
  if (fw_hw_chg_is_low() != 0) {
    return FW_CHARGE_ON;
  }
  return FW_CHARGE_DONE;
#else
  if (fw_hw_cable_present() == 0) {
    return FW_CHARGE_NONE;
  }
  return FW_CHARGE_ON;
#endif
}

/* Кадр питания. Милливольты — уже на аккумуляторе, не на ножке делителя. */
static void queue_power(void)
{
  /* Четыре байта кадра питания: тип, милливольты, код зарядки. */
  uint8_t frame[FW_FRAME_POWER_BYTES];
  uint16_t mv = FW_battery_millivolts();

  frame[0] = FW_FRAME_TYPE_POWER;
  frame[1] = (uint8_t)(mv & 0xFFu);
  frame[2] = (uint8_t)(mv >> 8);
  frame[3] = charge_code();
  (void)tx_put(frame, FW_FRAME_POWER_BYTES);
}

/* Событие 0x02: тип, номер, код ошибки. Сейчас ошибка всегда 0. */
static void queue_event(uint8_t event)
{
  /* Три байта события: тип, номер события, код ошибки. */
  uint8_t frame[FW_FRAME_EVENT_BYTES];

  frame[0] = FW_FRAME_TYPE_EVENT;
  frame[1] = event;
  frame[2] = 0u;
  (void)tx_put(frame, FW_FRAME_EVENT_BYTES);
}

/* START без кабеля и только из CONNECTED уходит в измерение, ответа нет.
   START при кабеле шлёт событие 1 и остаётся в CONNECTED.
   STOP гасит измерение и шлёт событие 3 с кодом 0. Другое число на телефон не уходит. */
static void apply_cmd(uint8_t cmd)
{
  /* Режим на момент команды. START пускает только из CONNECTED. */
  fw_mode_t mode = FW_states_mode();

  if (cmd == FW_CMD_STOP) {
    if (FW_states_stop_measurement() != 0) {
      FW_sync_stop();
    }
    /* Телефон ждёт 02 03, иначе кнопка START остаётся выключенной. */
    if (FW_states_mode() == FW_MODE_CONNECTED) {
      queue_event(FW_EVENT_STOPPED);
    }
    return;
  }
  if (mode != FW_MODE_CONNECTED && mode != FW_MODE_MEASUREMENT) {
    return;
  }
  if (cmd == FW_CMD_START && mode == FW_MODE_CONNECTED) {
    FW_states_set_charging(fw_hw_cable_present());
    if (FW_states_to_measurement() != 0) {
      return;
    }
    queue_event(FW_EVENT_CABLE);
  }
  FW_diagnostics_report(FW_DIAG_MODULE_BLE, FW_DIAG_LEVEL_ERR, FW_DIAG_CODE_CMD);
}

/* Вызывается, когда собрана целая строка. Соединение запоминает и уходит в CONNECTED.
   Разрыв возвращает в ADVERTISING. Прочие строки STA выбрасываются: это не команда телефона. */
static void take_line(void)
{
  /* Номер символа при копировании строки STA в g_sta. */
  unsigned i;

  if (strcmp(s_acc, FW_BLE_STA_DISCONNECT) == 0) {
    i = 0u;
    while (s_acc[i] != '\0' && (i + 1u) < FW_BLE_AT_LINE_BYTES) {
      g_sta[i] = s_acc[i];
      i++;
    }
    g_sta[i] = '\0';
    (void)FW_states_link_down();
    FW_sync_power_stop();
    FW_sync_stop();
    return;
  }
  if (strcmp(s_acc, FW_BLE_STA_CONNECT) != 0 && strcmp(s_acc, FW_BLE_STA_CONNECTED) != 0) {
    if (strcmp(s_acc, FW_BLE_STA_WAKEUP) != 0 && strcmp(s_acc, FW_BLE_STA_SLEEP) != 0) {
      feed_payload(s_acc, s_acc_n);
    }
    return;
  }
  i = 0u;
  while (s_acc[i] != '\0' && (i + 1u) < FW_BLE_AT_LINE_BYTES) {
    g_sta[i] = s_acc[i];
    i++;
  }
  g_sta[i] = '\0';
  /* MOD уже высокий с инициализации. Здесь его не опускаем: иначе байты на телефон не пойдут. */
  if (FW_states_to_connected() != 0) {
    FW_sync_power_arm();
  }
}

void FW_ble_poll(void)
{
  /* Режим, в котором вообще слушаем UART. */
  fw_mode_t mode = FW_states_mode();
  /* Один байт из кольца приёма или команда 0x04. */
  uint8_t byte;

  /* До рекламы модуль ещё отвечает на AT. Команды читаем и в измерении: STOP придёт туда же. */
  if (mode != FW_MODE_ADVERTISING && mode != FW_MODE_CONNECTED && mode != FW_MODE_MEASUREMENT) {
    return;
  }
  fw_hw_uart_rx_listen();
  while (fw_hw_uart_take(&byte) == FW_HW_OK) {
    take_rx(byte);
  }
  if (fw_hw_uart_rx_drop_take() != 0u) {
    FW_diagnostics_report(FW_DIAG_MODULE_BLE, FW_DIAG_LEVEL_ERR, FW_DIAG_CODE_RX);
  }
  while (FW_protocol_take_cmd(&byte) != 0) {
    apply_cmd(byte);
  }
  mode = FW_states_mode();
  if (FW_sync_power_due() != 0 &&
      (mode == FW_MODE_CONNECTED || mode == FW_MODE_MEASUREMENT)) {
    queue_power();
  }
  tx_pump();
}
