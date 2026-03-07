/*---------------------------------------------------------------------------
 * Copyright (c) 2024-2025 Arm Limited (or its affiliates).
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *      Name:    loopback_test.c
 *      Purpose: UART4 loopback test using CMSIS USART driver
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>

#include <string.h>

#include "cmsis_os2.h"

#include "Driver_USART.h"

#include "loopback_test.h"

/* CMSIS USART Driver for UART4 */
#define UART4_DRIVER_NUM  4
extern ARM_DRIVER_USART ARM_Driver_USART_(UART4_DRIVER_NUM);
#define ptrUART4          (&ARM_Driver_USART_(UART4_DRIVER_NUM))

/* Thread attributes for the UART4 loopback test thread */
static const osThreadAttr_t thread_attr_uart4_loopback = { .name = "UART4_Loop" };

/* Thread ID for the UART4 loopback test thread */
static osThreadId_t tid_uart4_loopback;

/* UART test buffers */
#define UART_BUFFER_SIZE 64U
static uint8_t tx_buffer[UART_BUFFER_SIZE];
static uint8_t rx_buffer[UART_BUFFER_SIZE];

/*
  Initialize UART4 using CMSIS USART Driver
*/
static int uart4_init(void) {
  if (ptrUART4->Initialize(NULL) != ARM_DRIVER_OK) {
    return -1;
  }

  if (ptrUART4->PowerControl(ARM_POWER_FULL) != ARM_DRIVER_OK) {
    return -1;
  }

  if (ptrUART4->Control(ARM_USART_MODE_ASYNCHRONOUS |
                        ARM_USART_DATA_BITS_8       |
                        ARM_USART_PARITY_NONE       |
                        ARM_USART_STOP_BITS_1       |
                        ARM_USART_FLOW_CONTROL_NONE,
                        420000U) != ARM_DRIVER_OK) {
    return -1;
  }

  if (ptrUART4->Control(ARM_USART_CONTROL_RX, 1U) != ARM_DRIVER_OK) {
    return -1;
  }

  if (ptrUART4->Control(ARM_USART_CONTROL_TX, 1U) != ARM_DRIVER_OK) {
    return -1;
  }

  return 0;
}

/*
  Thread that tests UART4 loopback (TX connected to RX).
*/
static __NO_RETURN void thread_uart4_loopback(void *argument) {
  uint32_t test_count = 0U;
  uint32_t pass_count = 0U;
  uint32_t fail_count = 0U;

  (void)argument;

  printf("UART4 loopback test started\n");
  printf("Connect UART4 TX (PA0) to UART4 RX (PA1)\n");

  if (uart4_init() != 0) {
    printf("UART4 initialization failed\n");
    for (;;) {
      osDelay(osWaitForever);
    }
  }

  for (uint32_t i = 0U; i < UART_BUFFER_SIZE; i++) {
    tx_buffer[i] = (uint8_t)i;
  }

  for (;;) {
    uint32_t timeout = 100U;

    test_count++;
    memset(rx_buffer, 0, sizeof(rx_buffer));

    if (ptrUART4->Receive(rx_buffer, UART_BUFFER_SIZE) != ARM_DRIVER_OK) {
      printf("Test #%u: RX start failed\n", (unsigned int)test_count);
      fail_count++;
      osDelay(1000U);
      continue;
    }

    osDelay(10U);

    if (ptrUART4->Send(tx_buffer, UART_BUFFER_SIZE) != ARM_DRIVER_OK) {
      printf("Test #%u: TX start failed\n", (unsigned int)test_count);
      fail_count++;
      osDelay(1000U);
      continue;
    }

    while (ptrUART4->GetStatus().tx_busy != 0U) {
      osDelay(1U);
    }

    while ((ptrUART4->GetStatus().rx_busy != 0U) && (timeout > 0U)) {
      osDelay(1U);
      timeout--;
    }

    if (timeout == 0U) {
      printf("Test #%u: RX timeout\n", (unsigned int)test_count);
      ptrUART4->Control(ARM_USART_ABORT_RECEIVE, 0U);
      fail_count++;
      osDelay(1000U);
      continue;
    }

    if (memcmp(tx_buffer, rx_buffer, UART_BUFFER_SIZE) == 0) {
      pass_count++;
      printf("Test #%u: PASS (pass=%u fail=%u)\n",
             (unsigned int)test_count,
             (unsigned int)pass_count,
             (unsigned int)fail_count);
    } else {
      fail_count++;
      printf("Test #%u: FAIL - data mismatch\n", (unsigned int)test_count);
      printf("  TX: ");
      for (uint32_t i = 0U; i < 8U; i++) {
        printf("%02X ", (unsigned int)tx_buffer[i]);
      }
      printf("\n  RX: ");
      for (uint32_t i = 0U; i < 8U; i++) {
        printf("%02X ", (unsigned int)rx_buffer[i]);
      }
      printf("\n");
    }

    for (uint32_t i = 0U; i < UART_BUFFER_SIZE; i++) {
      tx_buffer[i]++;
    }

    osDelay(2000U);
  }
}

/**
  Initialize and start UART4 loopback test

  \return          0 on success, or -1 on error.
*/
int loopback_test_start(void) {
  tid_uart4_loopback = osThreadNew(thread_uart4_loopback, NULL, &thread_attr_uart4_loopback);

  if (tid_uart4_loopback == NULL) {
    return -1;
  }

  return 0;
}
