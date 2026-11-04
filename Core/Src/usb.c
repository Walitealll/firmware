/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usb.c
  * @brief   This file provides code for the configuration
  *          of the USB instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "usb.h"

/* USER CODE BEGIN 0 */
#include "fw_hw.h"

#include <string.h>
/* USER CODE END 0 */

PCD_HandleTypeDef hpcd_USB_FS;

/* USB init function */

void MX_USB_PCD_Init(void)
{

  /* USER CODE BEGIN USB_Init 0 */

  /* USER CODE END USB_Init 0 */

  /* USER CODE BEGIN USB_Init 1 */

  /* USER CODE END USB_Init 1 */
  hpcd_USB_FS.Instance = USB;
  hpcd_USB_FS.Init.dev_endpoints = 8;
  hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_Init 2 */

  /* USER CODE END USB_Init 2 */

}

void HAL_PCD_MspInit(PCD_HandleTypeDef* pcdHandle)
{

  if(pcdHandle->Instance==USB)
  {
  /* USER CODE BEGIN USB_MspInit 0 */

  /* USER CODE END USB_MspInit 0 */
    /* USB clock enable */
    __HAL_RCC_USB_CLK_ENABLE();
  /* USER CODE BEGIN USB_MspInit 1 */
    /* Ниже сетки 5 мс и UART BLE: перечисление не должно задерживать эти прерывания. */
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
  /* USER CODE END USB_MspInit 1 */
  }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef* pcdHandle)
{

  if(pcdHandle->Instance==USB)
  {
  /* USER CODE BEGIN USB_MspDeInit 0 */

  /* USER CODE END USB_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USB_CLK_DISABLE();
  /* USER CODE BEGIN USB_MspDeInit 1 */

  /* USER CODE END USB_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* Пакет EP0. Самый длинный дескриптор — конфигурация, она короче этого буфера. */
#define USB_EP0_SIZE 64u
#define USB_BULK_MPS 64u
#define USB_NOTE_MPS 8u

/* Куда в памяти USB лежат буферы. Таблица концов занимает первые 0x18 байт. */
#define USB_PMA_EP0_TX 0x18u
#define USB_PMA_EP0_RX 0x58u
#define USB_PMA_EP1_TX 0x98u
#define USB_PMA_EP1_RX 0xD8u
#define USB_PMA_EP2_TX 0x118u

static uint8_t s_ep0[128];
/* Буфер приёма EP0. Размер как у пакета: меньший затирает соседние флаги, если хост пришлёт полный пакет. */
static uint8_t s_ep0_out[USB_EP0_SIZE];
static uint16_t s_ep0_total;
static uint16_t s_ep0_sent;
/* 1 — шлём данные хосту. 2 — после них нулевой пакет, длина была кратна 64. */
static uint8_t s_ep0_zlp;
/* Сколько байт ещё ждём от хоста на EP0. 0 — это не приём настройки линии. */
static uint8_t s_ep0_out_left;
static uint8_t s_configured;
static uint8_t s_line[7] = { 0x00u, 0xC2u, 0x01u, 0x00u, 0x00u, 0x00u, 0x08u };

static const uint8_t *s_tx;
static uint16_t s_tx_len;
static uint16_t s_tx_off;
static uint8_t s_tx_busy;
static uint8_t s_port_open;
static uint8_t s_tx_restart;
static uint8_t s_bulk_out[USB_BULK_MPS];

/* Виртуальный COM, который Windows узнаёт без своего inf: класс CDC ACM и пара ST. */
static const uint8_t s_dev_desc[] = {
  18, 1, 0x00, 0x02, 0x02, 0x00, 0x00, 64,
  0x83, 0x04, 0x40, 0x57, 0x00, 0x02, 1, 2, 0, 1
};

static const uint8_t s_cfg_desc[] = {
  9, 2, 0x4B, 0x00, 2, 1, 0, 0x80, 50,
  8, 0x0B, 0, 2, 0x02, 0x02, 0x01, 0,
  9, 4, 0, 0, 1, 0x02, 0x02, 0x01, 0,
  5, 0x24, 0x00, 0x10, 0x01,
  5, 0x24, 0x01, 0x00, 1,
  4, 0x24, 0x02, 0x02,
  5, 0x24, 0x06, 0, 1,
  7, 5, 0x82, 3, USB_NOTE_MPS, 0, 16,
  9, 4, 1, 0, 2, 0x0A, 0, 0, 0,
  7, 5, 0x01, 2, 64, 0, 0,
  7, 5, 0x81, 2, 64, 0, 0
};

static void ep0_stall(PCD_HandleTypeDef *hpcd)
{
  (void)HAL_PCD_EP_SetStall(hpcd, 0x80u);
  (void)HAL_PCD_EP_SetStall(hpcd, 0x00u);
}

static void ep0_send_chunk(PCD_HandleTypeDef *hpcd)
{
  uint16_t left = (uint16_t)(s_ep0_total - s_ep0_sent);
  uint16_t n = left > USB_EP0_SIZE ? USB_EP0_SIZE : left;

  (void)HAL_PCD_EP_Transmit(hpcd, 0x80u, &s_ep0[s_ep0_sent], n);
  s_ep0_sent = (uint16_t)(s_ep0_sent + n);
}

static void ep0_send(PCD_HandleTypeDef *hpcd, const uint8_t *data, uint16_t len, uint16_t ask)
{
  if (len > ask) {
    len = ask;
  }
  if (len > (uint16_t)sizeof s_ep0) {
    len = (uint16_t)sizeof s_ep0;
  }
  /* Строковый дескриптор уже лежит в s_ep0. Копировать его в себя нельзя. */
  if (len > 0u && data != 0 && data != s_ep0) {
    memcpy(s_ep0, data, len);
  }
  s_ep0_total = len;
  s_ep0_sent = 0u;
  s_ep0_zlp = (len != 0u && (len % USB_EP0_SIZE) == 0u) ? 1u : 0u;
  ep0_send_chunk(hpcd);
}

static void ep0_status(PCD_HandleTypeDef *hpcd)
{
  s_ep0_total = 0u;
  s_ep0_sent = 0u;
  s_ep0_zlp = 0u;
  (void)HAL_PCD_EP_Transmit(hpcd, 0x80u, 0, 0u);
}

static void usb_tx_next(void)
{
  uint16_t left;
  uint16_t n;

  if (!s_port_open || s_tx == 0 || s_tx_off >= s_tx_len) {
    s_tx_busy = 0u;
    return;
  }
  left = (uint16_t)(s_tx_len - s_tx_off);
  n = left > USB_BULK_MPS ? USB_BULK_MPS : left;
  (void)HAL_PCD_EP_Transmit(&hpcd_USB_FS, 0x81u, (uint8_t *)(s_tx + s_tx_off), n);
  s_tx_off = (uint16_t)(s_tx_off + n);
}

static void usb_tx_kick(void)
{
  uint32_t prim = __get_PRIMASK();

  __disable_irq();
  if (!s_port_open || s_tx_busy || s_tx == 0 || s_tx_len == 0u) {
    if (prim == 0u) {
      __enable_irq();
    }
    return;
  }
  s_tx_busy = 1u;
  s_tx_off = 0u;
  if (prim == 0u) {
    __enable_irq();
  }
  usb_tx_next();
}

static void usb_open_data(PCD_HandleTypeDef *hpcd)
{
  (void)HAL_PCD_EP_Open(hpcd, 0x01u, USB_BULK_MPS, EP_TYPE_BULK);
  (void)HAL_PCD_EP_Open(hpcd, 0x81u, USB_BULK_MPS, EP_TYPE_BULK);
  (void)HAL_PCD_EP_Open(hpcd, 0x82u, USB_NOTE_MPS, EP_TYPE_INTR);
  (void)HAL_PCD_EP_Receive(hpcd, 0x01u, s_bulk_out, USB_BULK_MPS);
  s_configured = 1u;
}

static void put_string_desc(const char *text)
{
  unsigned n = 0u;
  unsigned i;

  s_ep0[0] = 0u;
  s_ep0[1] = 3u;
  while (text[n] != '\0' && (2u + (n + 1u) * 2u) < sizeof s_ep0) {
    n++;
  }
  for (i = 0u; i < n; i++) {
    s_ep0[2u + i * 2u] = (uint8_t)text[i];
    s_ep0[3u + i * 2u] = 0u;
  }
  s_ep0[0] = (uint8_t)(2u + n * 2u);
}

void fw_hw_usb_start(void)
{
  (void)HAL_PCDEx_PMAConfig(&hpcd_USB_FS, 0x00u, PCD_SNG_BUF, USB_PMA_EP0_TX);
  (void)HAL_PCDEx_PMAConfig(&hpcd_USB_FS, 0x80u, PCD_SNG_BUF, USB_PMA_EP0_RX);
  (void)HAL_PCDEx_PMAConfig(&hpcd_USB_FS, 0x81u, PCD_SNG_BUF, USB_PMA_EP1_TX);
  (void)HAL_PCDEx_PMAConfig(&hpcd_USB_FS, 0x01u, PCD_SNG_BUF, USB_PMA_EP1_RX);
  (void)HAL_PCDEx_PMAConfig(&hpcd_USB_FS, 0x82u, PCD_SNG_BUF, USB_PMA_EP2_TX);
  (void)HAL_PCD_Start(&hpcd_USB_FS);
}

void fw_hw_usb_send(const uint8_t *data, uint16_t len)
{
  uint32_t prim = __get_PRIMASK();

  __disable_irq();
  s_tx = data;
  s_tx_len = len;
  s_tx_off = 0u;
  if (s_tx_busy) {
    s_tx_restart = 1u;
    if (prim == 0u) {
      __enable_irq();
    }
    return;
  }
  if (prim == 0u) {
    __enable_irq();
  }
  usb_tx_kick();
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
  s_configured = 0u;
  s_port_open = 0u;
  s_tx_busy = 0u;
  (void)HAL_PCD_EP_Open(hpcd, 0x00u, USB_EP0_SIZE, EP_TYPE_CTRL);
  (void)HAL_PCD_EP_Open(hpcd, 0x80u, USB_EP0_SIZE, EP_TYPE_CTRL);
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
  /* Восемь байт setup лежат в начале буфера HAL. */
  const uint8_t *pkt = (const uint8_t *)hpcd->Setup;
  uint8_t type = pkt[0];
  uint8_t req = pkt[1];
  uint16_t value = (uint16_t)pkt[2] | ((uint16_t)pkt[3] << 8);
  uint16_t length = (uint16_t)pkt[6] | ((uint16_t)pkt[7] << 8);

  s_ep0_out_left = 0u;
  if ((type & 0x60u) == 0x00u) {
    if (req == 6u) {
      uint8_t kind = (uint8_t)(value >> 8);
      uint8_t index = (uint8_t)(value & 0xFFu);

      if (kind == 1u) {
        ep0_send(hpcd, s_dev_desc, (uint16_t)sizeof s_dev_desc, length);
        return;
      }
      if (kind == 2u) {
        ep0_send(hpcd, s_cfg_desc, (uint16_t)sizeof s_cfg_desc, length);
        return;
      }
      if (kind == 3u && index == 0u) {
        s_ep0[0] = 4u;
        s_ep0[1] = 3u;
        s_ep0[2] = 0x09u;
        s_ep0[3] = 0x04u;
        ep0_send(hpcd, s_ep0, 4u, length);
        return;
      }
      if (kind == 3u && index == 1u) {
        put_string_desc("Reflex");
        ep0_send(hpcd, s_ep0, s_ep0[0], length);
        return;
      }
      if (kind == 3u && index == 2u) {
        put_string_desc("Reflex COM");
        ep0_send(hpcd, s_ep0, s_ep0[0], length);
        return;
      }
    } else if (req == 5u) {
      /* Адрес применит само железо после пустого подтверждения, не раньше. */
      hpcd->USB_Address = (uint8_t)(value & 0x7Fu);
      ep0_status(hpcd);
      return;
    } else if (req == 9u) {
      if ((value & 0xFFu) != 0u) {
        usb_open_data(hpcd);
      } else {
        s_configured = 0u;
        s_port_open = 0u;
      }
      ep0_status(hpcd);
      return;
    } else if (req == 8u) {
      s_ep0[0] = s_configured;
      ep0_send(hpcd, s_ep0, 1u, length);
      return;
    } else if (req == 0u) {
      s_ep0[0] = 0u;
      s_ep0[1] = 0u;
      ep0_send(hpcd, s_ep0, 2u, length);
      return;
    } else if (req == 11u) {
      /* SET_INTERFACE. Windows шлёт его при открытии COM. Отказ здесь и есть «порт не открывается». */
      ep0_status(hpcd);
      return;
    } else if (req == 10u) {
      s_ep0[0] = 0u;
      ep0_send(hpcd, s_ep0, 1u, length);
      return;
    }
  } else if ((type & 0x7Fu) == 0x21u) {
    /* Запрос класса с данными от хоста. Скорость линии — 7 байт. Принимаем целый пакет EP0:
       счётчик на 7 байт этот контроллер не подтверждает, и открытие порта зависает. */
    if ((type & 0x80u) == 0u && length != 0u) {
      if (length > USB_EP0_SIZE) {
        length = USB_EP0_SIZE;
      }
      s_ep0_out_left = (uint8_t)length;
      (void)HAL_PCD_EP_Receive(hpcd, 0x00u, s_ep0_out, USB_EP0_SIZE);
      return;
    }
    if (req == 0x21u) {
      ep0_send(hpcd, s_line, 7u, length);
      return;
    }
    if (req == 0x22u) {
      /* Бит DTR: терминал открыл порт. По спаду порт закрыт, по новому фронту текст уходит сначала. */
      uint8_t open = ((value & 1u) != 0u) ? 1u : 0u;

      if (open != 0u && s_port_open == 0u) {
        s_port_open = 1u;
        s_tx_off = 0u;
        if (s_tx_busy) {
          s_tx_restart = 1u;
        } else {
          usb_tx_kick();
        }
      } else if (open == 0u) {
        s_port_open = 0u;
      }
      ep0_status(hpcd);
      return;
    }
    /* Остальные команды класса без данных. Отказ на них тоже не даёт открыть COM. */
    ep0_status(hpcd);
    return;
  }
  ep0_stall(hpcd);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  if (epnum == 0u) {
    if (s_ep0_sent < s_ep0_total) {
      ep0_send_chunk(hpcd);
      return;
    }
    if (s_ep0_zlp != 0u) {
      s_ep0_zlp = 0u;
      (void)HAL_PCD_EP_Transmit(hpcd, 0x80u, 0, 0u);
      return;
    }
    (void)HAL_PCD_EP_Receive(hpcd, 0x00u, s_ep0_out, USB_EP0_SIZE);
    return;
  }
  if (epnum == 1u) {
    if (s_tx_restart != 0u) {
      s_tx_restart = 0u;
      s_tx_off = 0u;
      if (s_port_open != 0u) {
        usb_tx_next();
      } else {
        s_tx_busy = 0u;
      }
      return;
    }
    if (s_port_open != 0u && s_tx_off < s_tx_len) {
      usb_tx_next();
      return;
    }
    if (s_port_open != 0u && s_tx_len != 0u && (s_tx_len % USB_BULK_MPS) == 0u && s_tx_off == s_tx_len) {
      /* Хост отличает конец кратной пачки по пустому пакету. Один раз. */
      s_tx_off = (uint16_t)(s_tx_off + 1u);
      (void)HAL_PCD_EP_Transmit(hpcd, 0x81u, 0, 0u);
      return;
    }
    s_tx_busy = 0u;
  }
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  if (epnum == 0u && s_ep0_out_left != 0u) {
    if (s_ep0_out_left == 7u) {
      memcpy(s_line, s_ep0_out, 7u);
    }
    s_ep0_out_left = 0u;
    ep0_status(hpcd);
    return;
  }
  if (epnum == 1u && s_configured != 0u) {
    (void)HAL_PCD_EP_Receive(hpcd, 0x01u, s_bulk_out, USB_BULK_MPS);
  }
}

/* USER CODE END 1 */
