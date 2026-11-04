#ifndef FW_INIT_H
#define FW_INIT_H

/* Вызывается из main один раз, сразу после того как Cube поднял тактирование и периферию.
   Проводит стартовые проверки и либо переводит устройство в ADVERTISING, либо останавливает запуск. */
void FW_init_run(void);

#endif /* FW_INIT_H */
