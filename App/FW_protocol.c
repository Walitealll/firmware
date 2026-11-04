#include "FW_protocol.h"

#include "fw_config.h"

/* Сколько байт команды уже лежит после типа 0x04. 0 — тип ещё не видели. */
static unsigned s_got;
/* Последняя разобранная команда текстом. Пустая, пока целого кадра не было. */
static char g_cmd[8];
/* Сколько целых кадров 0x04 уже собрано. Второй кадр в том же чтении увеличивает счётчик. */
static unsigned g_cmd_n;
/* Команды, которые цикл ещё не применил. Несколько кадров из одного чтения лежат по порядку. */
static uint8_t s_cmd_q[FW_CMD_QUEUE];
static unsigned s_cmd_n;

/* Вызывается, когда второй байт кадра 0x04 уже на месте. Режим сменит цикл, не этот разбор. */
static void remember_cmd(uint8_t cmd)
{
  /* Текст команды для строки: START, STOP или OTHER. */
  const char *text;
  /* Позиция в g_cmd, куда кладётся очередная буква. */
  unsigned i;

  g_cmd_n++;
  if (cmd == FW_CMD_START) {
    text = "START";
  } else if (cmd == FW_CMD_STOP) {
    text = "STOP";
  } else {
    text = "OTHER";
  }
  i = 0u;
  while (text[i] != '\0' && (i + 1u) < (unsigned)sizeof g_cmd) {
    g_cmd[i] = text[i];
    i++;
  }
  g_cmd[i] = '\0';
  if (s_cmd_n < FW_CMD_QUEUE) {
    s_cmd_q[s_cmd_n] = cmd;
    s_cmd_n++;
  }
}

int FW_protocol_take_cmd(uint8_t *cmd)
{
  /* Номер команды, которую сдвигаем к началу очереди. */
  unsigned i;

  if (cmd == 0 || s_cmd_n == 0u) {
    return 0;
  }
  *cmd = s_cmd_q[0];
  s_cmd_n--;
  for (i = 0u; i < s_cmd_n; i++) {
    s_cmd_q[i] = s_cmd_q[i + 1u];
  }
  return 1;
}

static void put_u16(uint8_t *dst, uint16_t value)
{
  dst[0] = (uint8_t)value;
  dst[1] = (uint8_t)(value >> 8);
}

void FW_protocol_encode_sample(uint8_t *frame, const uint16_t *emg, uint16_t angle_x10, uint32_t time_ms)
{
  unsigned i;
  unsigned pos;

  if (frame == 0 || emg == 0) {
    return;
  }
  frame[0] = FW_FRAME_TYPE_SAMPLE;
  pos = 1u;
  for (i = 0u; i < FW_EMG_FRAME_COUNT; i++) {
    put_u16(frame + pos, emg[i]);
    pos += 2u;
  }
  put_u16(frame + pos, angle_x10);
  pos += 2u;
  frame[pos] = (uint8_t)time_ms;
  frame[pos + 1u] = (uint8_t)(time_ms >> 8);
  frame[pos + 2u] = (uint8_t)(time_ms >> 16);
  frame[pos + 3u] = (uint8_t)(time_ms >> 24);
}

void FW_protocol_rx_byte(uint8_t byte)
{
  /* Тип ещё не копился. Чужой байт выбрасываем и смотрим следующий. */
  if (s_got == 0u) {
    if (byte != FW_FRAME_TYPE_CMD) {
      return;
    }
    s_got = 1u;
    return;
  }
  remember_cmd(byte);
  s_got = 0u;
}
