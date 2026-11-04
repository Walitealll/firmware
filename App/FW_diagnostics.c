#include "FW_diagnostics.h"

#include "FW_states.h"
#include "fw_config.h"
#include "fw_hw.h"

/* Готовая строка лога. Пустая, пока инициализация не дошла до отчёта. */
static char g_line[FW_DIAG_LINE_BYTES];

/* Вызывается в конце сборки строки. Сейчас только запоминает текст.
   Писать его в USART1 нельзя: этот порт занят модулем BLE. USB логом тоже не назначаем. */
static void fw_diag_sink(const char *line)
{
  (void)line;
  /* TODO_HW: отдельного UART лога в .ioc нет. USART1 — BLE, USB логом не назначать. */
}

/* Кладёт один символ в строку. Если места уже нет, символ выбрасывается, чтобы не залезть за буфер. */
static unsigned append_char(unsigned pos, char ch)
{
  /* pos + 1 оставляем под завершающий ноль. Иначе строка перестанет быть строкой. */
  if ((pos + 1u) < FW_DIAG_LINE_BYTES) {
    g_line[pos] = ch;
    return pos + 1u;
  }
  return pos;
}

/* Дописывает текст поля: имя режима, модуля, уровня или кода. Пустой указатель просто пропускаем. */
static unsigned append_str(unsigned pos, const char *text)
{
  if (text == 0) {
    return pos;
  }
  while (*text != '\0') {
    pos = append_char(pos, *text);
    text++;
  }
  return pos;
}

/* Дописывает миллисекунды с включения десятичными цифрами. sprintf не используем: ему нужен вывод, которого нет. */
static unsigned append_u32(unsigned pos, uint32_t value)
{
  /* Цифры собираются с конца, потом выкладываются в обычном порядке. */
  /* Цифры числа с конца. Десяти разрядов хватает на uint32. */
  char tmp[10];
  /* Сколько цифр уже лежит в tmp. */
  unsigned n = 0;

  /* Ноль отдельным случаем: цикл ниже на нуле не сделал бы ни одной цифры. */
  if (value == 0u) {
    return append_char(pos, '0');
  }
  while (value > 0u && n < (unsigned)sizeof tmp) {
    tmp[n] = (char)('0' + (value % 10u));
    value /= 10u;
    n++;
  }
  while (n > 0u) {
    n--;
    pos = append_char(pos, tmp[n]);
  }
  return pos;
}

void FW_diagnostics_report(const char *module, const char *level, const char *code)
{
  /* pos — сколько символов уже лежит в g_line. */
  unsigned pos = 0;

  pos = append_u32(pos, fw_hw_tick_ms());
  pos = append_char(pos, ' ');
  pos = append_str(pos, FW_states_mode_name());
  pos = append_char(pos, ' ');
  pos = append_str(pos, module);
  pos = append_char(pos, ' ');
  pos = append_str(pos, level);
  pos = append_char(pos, ' ');
  pos = append_str(pos, code);
  pos = append_str(pos, "\r\n");
  /* Место осталось — закрываем строку нулём там, где кончился текст. */
  if (pos < FW_DIAG_LINE_BYTES) {
    g_line[pos] = '\0';
  } else {
    /* Буфер забит до конца. Ноль ставим в последний байт, чтобы чтение строки не убежало дальше. */
    g_line[FW_DIAG_LINE_BYTES - 1u] = '\0';
  }
  fw_diag_sink(g_line);
}

const char *FW_diagnostics_line(void)
{
  return g_line;
}

/* Текст для виртуального COM. Живёт, пока порт его не дошлёт и при повторном открытии. */
static char g_text[FW_DIAG_TEXT_BYTES];
static unsigned g_text_n;

void FW_diagnostics_begin(void)
{
  g_text_n = 0u;
  g_text[0] = '\0';
}

void FW_diagnostics_puts(const char *text)
{
  if (text == 0) {
    return;
  }
  while (*text != '\0' && (g_text_n + 1u) < FW_DIAG_TEXT_BYTES) {
    g_text[g_text_n] = *text;
    g_text_n++;
    text++;
  }
  g_text[g_text_n] = '\0';
}

void FW_diagnostics_putu(uint32_t value)
{
  char tmp[10];
  unsigned n = 0u;

  if (value == 0u) {
    FW_diagnostics_puts("0");
    return;
  }
  while (value > 0u && n < (unsigned)sizeof tmp) {
    tmp[n] = (char)('0' + (value % 10u));
    value /= 10u;
    n++;
  }
  while (n > 0u) {
    char one[2];

    n--;
    one[0] = tmp[n];
    one[1] = '\0';
    FW_diagnostics_puts(one);
  }
}

void FW_diagnostics_puthex2(uint8_t value)
{
  static const char hex[] = "0123456789ABCDEF";
  char one[3];

  one[0] = hex[(value >> 4) & 0x0Fu];
  one[1] = hex[value & 0x0Fu];
  one[2] = '\0';
  FW_diagnostics_puts(one);
}

void FW_diagnostics_publish(void)
{
  fw_hw_usb_send((const uint8_t *)g_text, (uint16_t)g_text_n);
}
