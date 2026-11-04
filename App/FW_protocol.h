#ifndef FW_PROTOCOL_H
#define FW_PROTOCOL_H

#include <stdint.h>

/* Вызывается из FW_ble для каждого байта, который не является строкой STA.
   Собирает кадр 0x04: тип и один байт команды. Неполный кадр ждёт следующий байт.
   Режим сам не меняет: готовую команду забирает FW_protocol_take_cmd. */
void FW_protocol_rx_byte(uint8_t byte);
/* Забирает одну разобранную команду. 1 — байт лежит в cmd, 0 — новых команд нет. */
int FW_protocol_take_cmd(uint8_t *cmd);

/* Кладёт кадр 0x03 в frame. Размер буфера — FW_FRAME_SAMPLE_BYTES.
   emg — FW_EMG_FRAME_COUNT кодов от старого к новому. Угол уже в десятых долях градуса.
   Метку снимает вызывающий непосредственно перед этим вызовом. */
void FW_protocol_encode_sample(uint8_t *frame, const uint16_t *emg, uint16_t angle_x10, uint32_t time_ms);

#endif /* FW_PROTOCOL_H */
