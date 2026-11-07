#ifndef FW_CONFIG_H
#define FW_CONFIG_H

#include "main.h"

/* Угол в кадре: |heading_ext − heading_int|, 0…180°, десятые в протоколе.
   Читается через bno055_getVectorEuler (NDOF). */

/* Имена пинов схемы. Номера ног здесь не выдумываются: это те же метки, что завёл Cube. */

/* Вход «питание с кабеля в порядке». Ноль — зарядник видит источник. Сейчас кабель читаем не отсюда. */
#define FW_PGOOD_PORT GPIO_IN_PGOOD_GPIO_Port
#define FW_PGOOD_PIN GPIO_IN_PGOOD_Pin
/* Вход «заряд идёт». Ноль — ток в батарею есть. Пока кабель решаем по USB_SENSE, этот пин в байт зарядки не входит. */
#define FW_CHG_PORT GPIO_IN_CHG_GPIO_Port
#define FW_CHG_PIN GPIO_IN_CHG_Pin
/* Ножка 38, PA15. Пока по ней решаем, вставлен ли кабель. */
#define FW_USB_SENSE_PORT GPIO_IN_USB_SENSE_GPIO_Port
#define FW_USB_SENSE_PIN GPIO_IN_USB_SENSE_Pin
/* Какой вход считать кабелем. Чтобы перейти на PGOOD, меняют это число, не вызовы. */
#define FW_CABLE_SRC_USB_SENSE 1u
#define FW_CABLE_SRC_PGOOD 2u
#define FW_CABLE_SRC FW_CABLE_SRC_USB_SENSE
/* 1 — на USB_SENSE единица значит, что кабель вставлен. 0 — кабель это ноль. */
#define FW_USB_SENSE_PRESENT_HIGH 1

/* Сброс модуля BLE. Импульс им сейчас не делаем, только держим отпущенным. */
#define FW_BLE_RST_PORT GPIO_OUT_BLE_RST_GPIO_Port
#define FW_BLE_RST_PIN GPIO_OUT_BLE_RST_Pin
/* Режим модуля: высокий уровень нужен, чтобы байты потом шли на телефон. */
#define FW_BLE_MOD_PORT GPIO_OUT_BLE_MOD_GPIO_Port
#define FW_BLE_MOD_PIN GPIO_OUT_BLE_MOD_Pin
/* Пробуждение модуля, если он не ответил на AT. */
#define FW_BLE_WKP_PORT GPIO_OUT_WKP_GPIO_Port
#define FW_BLE_WKP_PIN GPIO_OUT_WKP_Pin

/* Сброс датчика угла на самой плате. По нему узнаём, кто из двух внутренний. */
#define FW_BNO_INT_RST_PORT GPIO_OUT_INT_BNO_RST_GPIO_Port
#define FW_BNO_INT_RST_PIN GPIO_OUT_INT_BNO_RST_Pin
/* Сброс выносного датчика угла. На плате это PB2. */
#define FW_BNO_EXT_RST_PORT GPIO_OUT_EXT_BNO_RST_GPIO_Port
#define FW_BNO_EXT_RST_PIN GPIO_OUT_EXT_BNO_RST_Pin
/* Макет Blue Pill: ножки PB2 нет, тот же сброс временно на PB1. Сейчас не пишем. */
#define EXT_BNO_RST_DEMO_PORT GPIOB
#define EXT_BNO_RST_DEMO_PIN GPIO_PIN_1

/* Частота ядра после настройки тактирования в Cube. С ней сверяемся, что старт прошёл штатно. */
#define FW_SYSCLK_HZ 72000000u

/* Скорость UART к модулю E104. Такую же уже поставил Cube, второй раз порт не настраиваем. */
#define FW_BLE_UART_BAUD_BPS 115200u

/* Скорость обеих шин к BNO055. Датчик на ней рассчитан, быстрее в этой прошивке не включаем. */
#define FW_BNO_I2C_HZ 100000u
/* Сколько раз стучаться в один адрес за один опрос. Одного раза хватает, чтобы отличить «нет» от «есть». */
#define FW_I2C_PROBE_TRIALS 1u

/* Сколько миллисекунд ждать один ответ UART, I2C или АЦП на инициализации.
   В мануалах модуля и датчика этой цифры нет. Она нужна, чтобы вызов не завис навсегда. */
#define FW_IO_TIMEOUT_MS 100u

/* Сколько держать WKP после спада, чтобы модуль проснулся. Столько же он спит от фронта. */
#define FW_BLE_WKP_LEVEL_MS 200u
/* Сколько MOD должен оставаться высоким, чтобы модуль запомнил смену режима. */
#define FW_BLE_MOD_HOLD_MS 200u

/* TODO_HW: сколько миллисекунд держать импульс сброса модуля, в мануале не написано. 0 — импульс не делаем. */
#define FW_BLE_RST_PULSE_MS 0u
/* TODO_HW: пауза между командами AT не задана. 0 — свою задержку не вставляем. */
#define FW_BLE_AT_GAP_MS 0u
/* TODO_HW: пауза после AT+RESET не задана. 0 — сразу снова спрашиваем AT, а если тишина, будим по WKP. */
#define FW_BLE_RESET_PAUSE_MS 0u
/* TODO_HW: сколько миллисекунд повторять AT, пока модуль после своего включения не ответит. В мануале нет. 0 — как сейчас, один ответ и сразу WKP. */
#define FW_BLE_BOOT_WAIT_MS 0u

/* Сколько байт ответа AT готовы принять. Ответы инициализации короткие, длиннее считаем ошибкой. */
#define FW_BLE_AT_LINE_BYTES 32u
/* Кольцо приёма UART. Хватает на несколько коротких строк STA, пока нет потока с телефона. */
#define FW_BLE_RX_RING_BYTES 64u
/* Ниже приоритета TIM2: сетка 5 мс на приоритете 0. Иначе байт UART мог бы задержать сетку. */
#define FW_BLE_UART_IRQ_PRIORITY 1u

/* Команды, которые уходят в модуль при инициализации. Конца строки в них нет: так устроен модуль. */
#define FW_BLE_CMD_AT "AT"
#define FW_BLE_CMD_BAUD "AT+BAUD=10"
#define FW_BLE_CMD_PARI "AT+PARI=0"
#define FW_BLE_CMD_HWFC "AT+HWFC=0"
#define FW_BLE_CMD_ROLE "AT+ROLE=0"
#define FW_BLE_CMD_ONSLEEP "AT+ONSLEEP=0"
/* Печать статуса в UART. Без неё модуль молчит о соединении: пина LINK на плате нет. */
#define FW_BLE_CMD_LOGMSG "AT+LOGMSG=1"
/* Обычная реклама, не iBeacon. Para 1 — заводское значение, после сна его надо вернуть. */
#define FW_BLE_CMD_ADV "AT+ADV=1"
/* Следующая связь: интервал 6×1,25 мс = 7,5 мс, без пропуска встреч, таймаут 500 мс.
   Минимум intv из мануала. Текущее соединение эта запись не переключает. */
#define FW_BLE_CMD_CONPARAMS "AT+CONPARAMS=6,0,400"
/* Эфир выключен. Так модуль ждёт, пока датчики инициализируются и приём строк STA уже стоит. */
#define FW_BLE_CMD_ADV_OFF "AT+ADV=0"
#define FW_BLE_CMD_RESET "AT+RESET"
/* Те же параметры, но уже вопрос модулю: что у него записано после сброса. */
#define FW_BLE_CMD_BAUD_Q "AT+BAUD?"
#define FW_BLE_CMD_PARI_Q "AT+PARI?"
#define FW_BLE_CMD_HWFC_Q "AT+HWFC?"
#define FW_BLE_CMD_ROLE_Q "AT+ROLE?"
#define FW_BLE_CMD_ONSLEEP_Q "AT+ONSLEEP?"
#define FW_BLE_CMD_LOGMSG_Q "AT+LOGMSG?"
#define FW_BLE_CMD_ADV_Q "AT+ADV?"
#define FW_BLE_CMD_CONPARAMS_Q "AT+CONPARAMS?"

/* Ответ «команда принята». */
#define FW_BLE_RSP_OK "+OK"
/* Ответ на вопрос о скорости: пункт 10 таблицы, это 115200. */
#define FW_BLE_RSP_BAUD "+OK=10"
/* Ответ на чётность, поток, роль и автосон: везде заводской ноль. */
#define FW_BLE_RSP_ZERO "+OK=0"
/* Ответ на печать статуса и рекламу: обе включены, para 1. */
#define FW_BLE_RSP_ONE "+OK=1"
/* Ответ на AT+CONPARAMS?: те же три числа, что записали. */
#define FW_BLE_RSP_CONPARAMS "+OK=6,0,400"
/* Две записи одного события в мануале. Какая придёт с модуля, ещё не видно, поэтому узнаём обе. */
#define FW_BLE_STA_CONNECT "STA:connect"
#define FW_BLE_STA_CONNECTED "STA:connected"
/* Остальные строки статуса. В кадр телефона их не кладём. */
#define FW_BLE_STA_DISCONNECT "STA:disconnect"
#define FW_BLE_STA_WAKEUP "STA:wakeup"
#define FW_BLE_STA_SLEEP "STA:sleep"

/* Кадр с телефона: тип и один байт команды. Длины и контрольной суммы в кадре нет. */
#define FW_FRAME_TYPE_CMD 0x04u
#define FW_CMD_START 1u
#define FW_CMD_STOP 2u
/* Кадры платы к телефону. Размеры вместе с байтом типа. */
#define FW_FRAME_TYPE_POWER 0x01u
#define FW_FRAME_POWER_BYTES 4u
/* Байт зарядки кадра питания. */
#define FW_CHARGE_NONE 0u
#define FW_CHARGE_ON 1u
#define FW_CHARGE_DONE 2u
#define FW_FRAME_TYPE_EVENT 0x02u
#define FW_FRAME_EVENT_BYTES 3u
/* Событие 1: кабель вставлен, измерения запрещены. Код ошибки в этом кадре — 0. */
#define FW_EVENT_CABLE 1u
/* Событие 3: измерение остановлено, режим CONNECTED. На STOP код ошибки — 0. */
#define FW_EVENT_STOPPED 3u
/* Кадр измерений 0x03. Окно 10 мс: 20 отсчётов ЭМГ, один угол, метка. */
#define FW_FRAME_TYPE_SAMPLE 0x03u
#define FW_SAMPLE_PERIOD_MS 10u
/* 2000 Гц * 10 мс / 1000 = 20. Число целое, пока FW_EMG_HZ делится на 100. */
#define FW_EMG_FRAME_COUNT ((FW_EMG_HZ * FW_SAMPLE_PERIOD_MS) / 1000u)
/* Две половины кольца DMA: пока одна пишется, вторая — готовое окно. */
#define FW_EMG_DMA_SAMPLES (FW_EMG_FRAME_COUNT * 2u)
/* Тип, 20×uint16, угол uint16, метка uint32. */
#define FW_FRAME_SAMPLE_BYTES (1u + (FW_EMG_FRAME_COUNT * 2u) + 2u + 4u)
/* Приоритет DMA ЭМГ. Ниже TIM2, чтобы сетка 5 мс не ждала половину кольца. */
#define FW_EMG_DMA_IRQ_PRIORITY 1u
/* Сколько целых команд 0x04 держим, если они пришли пачкой, пока цикл их не разобрал. */
#define FW_CMD_QUEUE 8u
/* Сетка таймера. 10 с кадра питания — это она же, не отдельный счётчик SysTick. */
#define FW_SYNC_TICK_MS 5u
/* Частота съёма ЭМГ, Гц. Сейчас 2 кГц. Период TIM3 и число отсчётов в кадре считаются из этого числа. */
#define FW_EMG_HZ 2000u
/* Единственный вход ЭМГ: PB0, ADC1 канал 8. */
#define FW_EMG_ADC_CH ADC_CHANNEL_8
/* Время выборки этого входа. То же, что Cube уже записал каналу АЦП: 1,5 такта. */
#define FW_EMG_ADC_SAMPLETIME ADC_SAMPLETIME_239CYCLES_5
#define FW_POWER_PERIOD_MS 10000u
#define FW_POWER_PERIOD_TICKS (FW_POWER_PERIOD_MS / FW_SYNC_TICK_MS)
/* Два измерительных кадра, один кадр питания и один кадр события. */
#define FW_BLE_TX_SLOTS 4u
/* В слот входит самый длинный из этих кадров. */
#define FW_BLE_TX_SLOT_BYTES FW_FRAME_SAMPLE_BYTES
/* Модуль ругается, что команды пришли слишком часто. */
#define FW_BLE_RSP_ERR_GAP "+ERR=0"
/* Начало любой ошибки модуля. По нему отличаем сбой от нормального +OK. */
#define FW_BLE_RSP_ERR_PREFIX "+ERR"

/* Как часто в ADVERTISING обновлять снимок батареи для отладчика.
   Реже одного раза в секунду контроль питания в этом режиме не оставляем. */
#define FW_BATTERY_DEBUG_PERIOD_MS 1000u
/* Ниже этого напряжения, в милливольтах, запуск прекращается и ставится LOW_BATTERY. */
#define FW_BATTERY_LOW_mV 3400
/* Пока 1: в LOW_BATTERY не переходим, низкое напряжение запуск не обрывает. */
#define FW_LOW_BATTERY_OFF 1
/* С этого напряжения можно снова выйти из LOW_BATTERY без выключения. В этом проходе порог не проверяется. */
#define FW_BATTERY_RECOVER_mV 3600

/* Делитель известен: на PA0 приходит 5/8 напряжения аккумулятора 3,5–4,2 В. */
#define FW_BATTERY_SCALE_READY 1
/* Чтобы из напряжения ножки получить напряжение АКБ, умножаем на 8/5. */
#define FW_BATTERY_DIVIDER_NUM 8
#define FW_BATTERY_DIVIDER_DEN 5
/* Сдвига диапазона нет: 3,5–4,2 В на батарее, без добавки к милливольтам. */
#define FW_BATTERY_OFFSET_mV 0
/* Опора АЦП: питание микроконтроллера 3,3 В. Имеет смысл только вместе с известным делителем. */
#define FW_ADC_VREF_mV 3300
/* Верх кода 12-битного АЦП. На него делят сырой отсчёт, когда делитель уже впишут. */
#define FW_ADC_FULL_SCALE 4095

/* Адрес BNO055, если ножка COM3 притянута к земле. */
#define FW_BNO_ADDR_COM3_LOW 0x28u
/* Адрес BNO055, если COM3 притянута к питанию. */
#define FW_BNO_ADDR_COM3_HIGH 0x29u
/* Внешний BNO055 на I2C2. Второй адрес на этой шине не ищем. */
#define FW_BNO_ADDR_EXT FW_BNO_ADDR_COM3_HIGH
/* Сколько шин I2C отведено под датчики угла. */
#define FW_BNO_BUS_COUNT 2
/* Внутренний BNO055 на I2C1, внешний на I2C2. Номера совпадают с FW_HW_I2C1 и FW_HW_I2C2.
   Какая шина чья, сбросом не проверяем. */
#define FW_BNO_BUS_INT 1u
#define FW_BNO_BUS_EXT 2u
/* Сколько адресов из даташита проверяем на внутреннем I2C1. На I2C2 адрес один: FW_BNO_ADDR_EXT. */
#define FW_BNO_ADDR_COUNT 2
/* Сколько датчиков должно ответить, когда оба сброса отпущены. */
#define FW_BNO_EXPECT_COUNT 2

/* Регистр режима работы датчика. Пишется, чтобы войти в настройку и потом в рабочий режим. */
#define FW_BNO_REG_OPR_MODE 0x3Du
/* Страница и идентификатор для запуска датчика и проверки после RST_SYS. */
#define FW_BNO_REG_PAGE_ID 0x07u
#define FW_BNO_REG_CHIP_ID 0x00u
#define FW_BNO_CHIP_ID 0xA0u
/* CLK_SEL, бит 7 SYS_TRIGGER: 0 — внутренний генератор, 1 — внешний кварц 32,768 кГц.
   Включать внешний можно только при наличии кварца на соответствующем BNO055. */
#define FW_BNO_REG_SYS_TRIGGER 0x3Fu
#define FW_BNO_CLK_EXT_MASK 0x80u
#define FW_BNO_RST_SYS 0x20u
/* Штатные генераторы обоих BNO, как в проверенном диагностическом запуске. */
#define FW_BNO_USE_EXT_CRYSTAL_INT 0
#define FW_BNO_USE_EXT_CRYSTAL_EXT 0
/* Статус калибровки: SYS, GYR, ACC, MAG по два бита. 0 — нет, 3 — готово. */
#define FW_BNO_REG_CALIB_STAT 0x35u
/* Регистр питания. Отдельной записью ставится нормальный режим. */
#define FW_BNO_REG_PWR_MODE 0x3Eu
/* Регистр единиц углов. Оба датчика должны держать одно и то же значение. */
#define FW_BNO_REG_UNIT_SEL 0x3Bu
/* Ускорение X, дальше Y и Z. Шесть байт подряд, по два на ось. */
#define FW_BNO_REG_ACC_X_LSB 0x08u
/* Угловая скорость X, дальше Y и Z. Шесть байт подряд. */
#define FW_BNO_REG_GYR_X_LSB 0x14u
/* Длина пачки ускорения или угловой скорости. */
#define FW_BNO_VEC_BYTES 6u
/* Сколько отсчётов ускорения в 1 м/с². */
#define FW_BNO_ACC_LSB_PER_MS2 100
/* Сколько отсчётов угловой скорости в 1 °/с. */
#define FW_BNO_GYR_LSB_PER_DPS 16
/* Пачка Эйлера с EUL_DATA_X: heading, затем roll, затем pitch.
   Знаковое 16 бит, младший байт первый. По байту не читать. */
#define FW_BNO_REG_EUL_DATA_X_LSB 0x1Au
/* EUL_DATA_Y, крен, поворот вокруг Y платы. В пачке это байты 2 и 3. */
#define FW_BNO_REG_EUL_DATA_Y_LSB 0x1Cu
/* EUL_DATA_Z, тангаж, поворот вокруг X платы. В пачке это байты 4 и 5. */
#define FW_BNO_REG_EUL_DATA_Z_LSB 0x1Eu
/* Длина пачки Эйлера. */
#define FW_BNO_EUL_BYTES 6u
/* Сколько отсчётов Эйлера в 1°. */
#define FW_BNO_EUL_LSB_PER_DEG 16
/* С какого байта пачки брать курс (heading, ось Z). */
#define FW_BNO_EUL_OFF_HEADING 0u
/* С какого байта пачки брать крен (roll, ось Y). */
#define FW_BNO_EUL_OFF_ROLL 2u
/* С какого байта пачки брать тангаж (pitch, ось X). */
#define FW_BNO_EUL_OFF_PITCH 4u
/* Первый байт кватерниона. Дальше читаются ещё семь байт подряд. */
#define FW_BNO_REG_QUA_W_LSB 0x20u
/* Длина кватерниона в байтах: w, x, y, z, у каждой части младший и старший байт. */
#define FW_BNO_QUA_BYTES 8u

/* Какие биты регистра режима вообще относятся к режиму. Остальные при записи сохраняем. */
#define FW_BNO_OPR_MODE_MASK 0x0Fu
/* Код режима настройки. В нём пишутся питание и единицы. */
#define FW_BNO_OPR_CONFIG 0x00u
/* Код режима IMU: акселерометр и гироскоп, без магнитометра. */
#define FW_BNO_OPR_IMU 0x08u
/* Код режима NDOF: акселерометр, гироскоп и магнитометр, абсолютный курс.
   Не путать с AMG 0x07: там fusion и ориентации нет. */
#define FW_BNO_OPR_NDOF 0x0Cu
/* Какой режим писать при инициализации. Сейчас NDOF — с магнитометром. */
#define FW_BNO_OPR_ACTIVE FW_BNO_OPR_NDOF
/* Какие биты регистра питания задают режим питания. */
#define FW_BNO_PWR_MODE_MASK 0x03u
/* Нормальное питание: датчик не спит. */
#define FW_BNO_PWR_NORMAL 0x00u
/* Заводское значение единиц. Его же записываем и потом читаем обратно. */
#define FW_BNO_UNIT_SEL 0x80u
#define FW_BNO_UNIT_MASK 0x87u  /* Формат ориентации, Euler, GYR и ACC. */

/* Выдержка перед первым обращением к датчикам. */
#define FW_BNO_TPOR_MS 650u
/* Выдержки после RST_SYS и после подтверждения загрузки. */
#define FW_BNO_RESET_DELAY_MS 700u
#define FW_BNO_BOOT_SETTLE_MS 50u
#define FW_BNO_BOOT_POLL_MS 10u
#define FW_BNO_BOOT_TIMEOUT_MS 1000u
/* Пауза после изменения питания или источника тактирования. */
#define FW_BNO_CONFIG_WRITE_MS 10u
/* Сколько ждать после команды «войти в настройку», прежде чем писать другие регистры. */
#define FW_BNO_CONFIG_SWITCH_MS 19u
/* Выдержка CONFIG -> рабочий режим, как в Bosch BNO055 SensorAPI.
   Раннее чтение через 7 мс на этой плате не подтверждало сохранение NDOF. */
#define FW_BNO_OPR_SWITCH_MS 600u
/* Одна повторная настройка, если после выдержки режим вернулся в CONFIG. */
#define FW_BNO_MODE_RETRIES 1u
/* Период, с которым датчик обновляет ориентацию. На инициализации с этим шагом опрашиваем статус. */
#define FW_BNO_FUSION_PERIOD_MS 10u
/* Дополнительный срок ожидания SYS_STATUS=5 после выдержки NDOF.
   При опросе сохраняем фактические OPR_MODE/SYS_STATUS/SYS_ERR. */
#define FW_BNO_FUSION_WAIT_MS 1000u
/* Пауза живого чтения Euler из главного цикла. Это не окно измерения 10 мс. */
#define FW_BNO_LIVE_MS 500u
/* Сколько отсчётов регистра в единице кватерниона: 2^14. */
#define FW_BNO_QUA_ONE 16384
/* Байт состояния датчика. 5 — алгоритм ориентации уже считает. */
#define FW_BNO_REG_SYS_STATUS 0x39u
#define FW_BNO_SYS_ERROR 1u
#define FW_BNO_SYS_FUSION 5u
/* Байт ошибки. Имеет смысл, когда статус равен 1. В текст пишем его всё равно. */
#define FW_BNO_REG_SYS_ERR 0x3Au

/* AXIS_MAP_CONFIG внутреннего, регистр 0x41. TODO_HW: 0 — в датчик не писать.
   Заводские 0x24 — сброс в P1, это не крепление на конечности. */
#define FW_BNO_AXIS_MAP_CONFIG_INT 0
/* AXIS_MAP_SIGN внутреннего, регистр 0x42. TODO_HW: 0 — в датчик не писать.
   Заводской 0x00 — тоже сброс в P1, не выбранный знак осей. */
#define FW_BNO_AXIS_MAP_SIGN_INT 0
/* AXIS_MAP_CONFIG внешнего, регистр 0x41. TODO_HW: 0 — в датчик не писать. */
#define FW_BNO_AXIS_MAP_CONFIG_EXT 0
/* AXIS_MAP_SIGN внешнего, регистр 0x42. TODO_HW: 0 — в датчик не писать. */
#define FW_BNO_AXIS_MAP_SIGN_EXT 0

/* Сетка 5 мс — TIM2. В Cube этого таймера нет, его поднимает FW_TIM2_Init. */

/* Длина одной строки лога вместе с концом строки и нулём. */
#define FW_DIAG_LINE_BYTES 96u
/* Текст итога инициализации для виртуального COM. Успешный блок короче, запас на обрыв посередине. */
#define FW_DIAG_TEXT_BYTES 512u
/* Код движка AD5160: 0 прижимает W к B, между ними остаётся только сопротивление ключа. */
#define FW_DP_WIPER_MIN 0u

/* Тексты режима для строки лога. Совпадают с именами режимов в ТЗ. */
#define FW_MODE_NAME_NONE "NONE"
#define FW_MODE_NAME_LOW_BATTERY "LOW_BATTERY"
#define FW_MODE_NAME_INITIALIZATION "INITIALIZATION"
#define FW_MODE_NAME_ADVERTISING "ADVERTISING"
#define FW_MODE_NAME_CONNECTED "CONNECTED"
#define FW_MODE_NAME_MEASUREMENT "MEASUREMENT"
/* Как часто писать имя режима в виртуальный COM. */
#define FW_USB_MODE_PERIOD_MS 5000u

/* Кто попал в строку лога. Имена на английском, чтобы GDB печатал строку без ломаных букв. */
#define FW_DIAG_MODULE_INIT "FW_init"
#define FW_DIAG_MODULE_BLE "FW_ble"
#define FW_DIAG_MODULE_ANGLE_INT "FW_angle.internal"
#define FW_DIAG_MODULE_ANGLE_EXT "FW_angle.external"
#define FW_DIAG_MODULE_DIAG "FW_diagnostics"

/* Уровень строки. OK — шаг прошёл, ERR — обязательный шаг не прошёл. */
#define FW_DIAG_LEVEL_OK "OK"
#define FW_DIAG_LEVEL_ERR "ERR"

/* Короткое слово в конце строки: что именно случилось. Числового справочника ошибок в ТЗ нет. */
#define FW_DIAG_CODE_OK "OK"             /* все обязательные шаги прошли */
#define FW_DIAG_CODE_PERIPH "PERIPH"     /* такт, UART или I2C не готовы, либо переход режима запрещён */
#define FW_DIAG_CODE_ADC "ADC"           /* АЦП батареи не ответил */
#define FW_DIAG_CODE_AT "AT"             /* модуль молчит или команда AT не ушла */
#define FW_DIAG_CODE_AT_GAP "AT_GAP"     /* команды AT слишком часто, модуль ответил +ERR=0 */
#define FW_DIAG_CODE_PARAM "PARAM"       /* настройка BLE не принялась или чтение не совпало */
#define FW_DIAG_CODE_MOD "MOD"           /* пин MOD не остался высоким, прозрачный режим выключен */
#define FW_DIAG_CODE_I2C "I2C"           /* шина BNO: таймаут или ошибка чтения/записи */
#define FW_DIAG_CODE_MODE "MODE"         /* BNO не остался в выбранном режиме fusion */
#define FW_DIAG_CODE_ID "ID"             /* по адресу отвечает не BNO055 */
#define FW_DIAG_CODE_RESET "RESET"       /* BNO055 не вышел из программного сброса */
#define FW_DIAG_CODE_CLOCK "CLOCK"       /* CLK_SEL не совпал с конфигурацией платы */
#define FW_DIAG_CODE_UNIT "UNIT"         /* UNIT_SEL прочитался не тем, что записали */
#define FW_DIAG_CODE_QUAT "QUAT"         /* прежний код проверки ненулевого кватерниона */
#define FW_DIAG_CODE_MAP "MAP"           /* не два BNO или сброс не отличил внутренний от внешнего */
/* SYS_STATUS после входа в fusion не равен 5, либо SYS_ERR ненулевой. */
#define FW_DIAG_CODE_STATUS "STATUS"
/* Байт в цифровой потенциометр не ушёл. */
#define FW_DIAG_CODE_POT "POT"
/* Команда телефона не принята. На воздух такой отказ не уходит. */
#define FW_DIAG_CODE_CMD "CMD"
/* Исходящая очередь UART заполнена, кадр не взяли. */
#define FW_DIAG_CODE_TXQ "TXQ"
/* Кольцо приёма UART переполнено, байт потерян. */
#define FW_DIAG_CODE_RX "RX"

/* TODO_HW: отдельного UART для лога в схеме нет. USART1 занят BLE, USB под лог не занимаем. */

#endif /* FW_CONFIG_H */
