#ifndef FW_DIAGNOSTICS_H
#define FW_DIAGNOSTICS_H

#include <stdint.h>

/* Вызывается один раз в конце инициализации: либо всё прошло, либо обязательный шаг упал.
   Собирает одну строку «время режим модуль уровень код». Эта строка остаётся в памяти для отладчика. */
void FW_diagnostics_report(const char *module, const char *level, const char *code);
/* Отдаёт последнюю собранную строку из пяти полей. */
const char *FW_diagnostics_line(void);

/* Начинает текст для виртуального COM. Предыдущий текст стирается. */
void FW_diagnostics_begin(void);
/* Дописывает кусок текста как есть, без перевода строки. */
void FW_diagnostics_puts(const char *text);
/* Дописывает десятичное число. */
void FW_diagnostics_putu(uint32_t value);
/* Дописывает байт двумя шестнадцатеричными цифрами. */
void FW_diagnostics_puthex2(uint8_t value);
/* Отдаёт собранный текст в USB. Буфер остаётся: повторное открытие порта шлёт его снова. */
void FW_diagnostics_publish(void);

#endif /* FW_DIAGNOSTICS_H */
