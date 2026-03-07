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
 *      Purpose: UART4 Loopback Test using CMSIS USART Driver
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include "cmsis_os2.h"
#include "Driver_USART.h"
#include "loopback_test.h"

/* CMSIS USART Driver for UART4 */
#define UART4_DRIVER_NUM  4
extern ARM_DRIVER_USART   ARM_Driver_USART_(UART4_DRIVER_NUM);
#define ptrUART4          (&ARM_Driver_USART_(UART4_DRIVER_NUM))

/* Thread attributes for the UART test thread */
static const osThreadAttr_t thread_attr_UART = { .name = "UART_Test" };

/* Thread ID for the UART test thread */
static osThreadId_t tid_UART;

/* UART test buffers and data */
#define UART_BUFFER_SIZE 64
static uint8_t tx_buffer[UART_BUFFER_SIZE];
static uint8_t rx_buffer[UART_BUFFER_SIZE];

/*
  Initialize UART4 using CMSIS USART Driver
*/
static int UART4_Init(void) {
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
                        420000) != ARM_DRIVER_OK) {
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
static __NO_RETURN void thread_UART (void *argument) {
  uint32_t test_count = 0;
  uint32_t pass_count = 0;
  uint32_t fail_count = 0;

  (void)argument;

  printf("UART4 Loopback Test Starting...\n");

  // Initialize UART4
  if (UART4_Init() != 0) {
    printf("UART4 Initialization Failed!\n");
    for (;;) osDelay(osWaitForever);
  }

  printf("UART4 Initialized (420000 baud)\n");
  printf("Connect UART4 TX (PA0) to UART4 RX (PA1)\n\n");

  // Initialize test data pattern
  for (int i = 0; i < UART_BUFFER_SIZE; i++) {
    tx_buffer[i] = i;
  }

  osDelay(1000U);  // Initial delay

  for (;;) {
    test_count++;
    
    // Clear receive buffer
    memset(rx_buffer, 0, UART_BUFFER_SIZE);

    // Start non-blocking receive
    if (ptrUART4->Receive(rx_buffer, UART_BUFFER_SIZE) != ARM_DRIVER_OK) {
      printf("Test #%u: RX Start Failed\n", test_count);
      fail_count++;
      osDelay(1000U);
      continue;
    }

    // Small delay to ensure receiver is ready
    osDelay(10U);

    // Transmit test data
    if (ptrUART4->Send(tx_buffer, UART_BUFFER_SIZE) != ARM_DRIVER_OK) {
      printf("Test #%u: TX Start Failed\n", test_count);
      fail_count++;
      osDelay(1000U);
      continue;
    }

    // Wait for transmit to complete
    while (ptrUART4->GetStatus().tx_busy != 0U) {
      osDelay(1U);
    }

    // Wait for reception to complete (with timeout)
    uint32_t timeout = 100;  // 100ms timeout
    while (ptrUART4->GetStatus().rx_busy != 0U && timeout > 0) {
      osDelay(1U);
      timeout--;
    }

    if (timeout == 0) {
      printf("Test #%u: RX Timeout\n", test_count);
      ptrUART4->Control(ARM_USART_ABORT_RECEIVE, 0);
      fail_count++;
      osDelay(1000U);
      continue;
    }

    // Compare transmitted and received data
    if (memcmp(tx_buffer, rx_buffer, UART_BUFFER_SIZE) == 0) {
      pass_count++;
      printf("Test #%u: PASS (Total: %u pass, %u fail)\n", 
             test_count, pass_count, fail_count);
    } else {
      fail_count++;
      printf("Test #%u: FAIL - Data mismatch\n", test_count);
      
      // Print first few mismatches for debugging
      printf("  First bytes - TX: ");
      for (int i = 0; i < 8; i++) printf("%02X ", tx_buffer[i]);
      printf("\n  First bytes - RX: ");
      for (int i = 0; i < 8; i++) printf("%02X ", rx_buffer[i]);
      printf("\n");
    }

    // Update test pattern for next iteration
    for (int i = 0; i < UART_BUFFER_SIZE; i++) {
      tx_buffer[i]++;
    }

    // Wait before next test
    osDelay(2000U);
  }
}

/**
  Initialize and start UART4 loopback test

  \return          0 on success, or -1 on error.
*/
int loopback_test_start(void) {
  /* Create UART test thread */
  tid_UART = osThreadNew(thread_UART, NULL, &thread_attr_UART);
  
  if (tid_UART == NULL) {
    return -1;
  }
  
  return 0;
}
