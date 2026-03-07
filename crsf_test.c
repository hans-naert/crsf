/*---------------------------------------------------------------------------
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 *---------------------------------------------------------------------------*/

#include <stdio.h>

#include "cmsis_os2.h"
#include "Driver_USART.h"

#include "crsf_parser.h"
#include "crsf_test.h"

/* CMSIS USART Driver for UART4 */
#define UART4_DRIVER_NUM 4
extern ARM_DRIVER_USART ARM_Driver_USART_(UART4_DRIVER_NUM);
#define ptrUART4 (&ARM_Driver_USART_(UART4_DRIVER_NUM))

/* Thread attributes */
static const osThreadAttr_t thread_attr_crsf_rx = { .name = "CRSF_RX" };
static const osThreadAttr_t thread_attr_crsf_tx = { .name = "CRSF_TX" };

/* Thread IDs */
static osThreadId_t tid_crsf_rx;
static osThreadId_t tid_crsf_tx;

/* UART RX double-buffering for interrupt/DMA mode */
#define CRSF_RX_CHUNK_SIZE 64U
static uint8_t crsf_rx_buffer_a[CRSF_RX_CHUNK_SIZE];
static uint8_t crsf_rx_buffer_b[CRSF_RX_CHUNK_SIZE];
static volatile uint8_t *rx_buffer_active = crsf_rx_buffer_a;
static volatile uint8_t *rx_buffer_process = crsf_rx_buffer_b;
static volatile uint32_t rx_overflow_count = 0U;
static volatile uint32_t rx_buffer_busy = 0U;  /* Set by thread, checked by callback */
static volatile uint32_t rx_overrun_count = 0U; /* Buffer overwrite (thread too slow) */

/* Thread flag for signaling buffer ready */
#define RX_BUFFER_READY_FLAG 0x00000001U

/* Fixed valid CRSF dummy stream transmitted on UART4 TX */
static const uint8_t crsf_dummy_stream[] = {
  0x00U, 0x18U, 0x16U, 0xBDU, 0x08U, 0x9FU, 0xF4U, 0xAEU, 0xF7U,
  0xBDU, 0xEFU, 0x7DU, 0xEFU, 0xFBU, 0xADU, 0xFDU, 0x45U, 0x2BU,
  0x5AU, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x6CU,
  0x00U, 0x18U, 0x16U, 0xBDU, 0x08U, 0x9FU, 0xF4U, 0xAAU, 0xF7U,
  0xBDU, 0xEFU, 0x7DU, 0xEFU, 0xFBU, 0xADU, 0xFDU, 0x45U, 0x2BU,
  0x5AU, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x94U
};

/*
  UART4 event callback (called from interrupt context).
  Handles receive complete and overflow events.
*/
static void uart4_event_callback(uint32_t event) {
  if (event & ARM_USART_EVENT_RECEIVE_COMPLETE) {
    /* Check if thread is still processing previous buffer (overrun condition) */
    if (rx_buffer_busy != 0U) {
      rx_overrun_count++;  /* Thread too slow - about to corrupt buffer! */
    }
    
    /* Swap active/process buffer pointers */
    volatile uint8_t *temp = rx_buffer_active;
    rx_buffer_active = rx_buffer_process;
    rx_buffer_process = temp;
    
    /* Start next receive immediately to avoid gaps */
    ptrUART4->Receive((void *)rx_buffer_active, CRSF_RX_CHUNK_SIZE);
    
    /* Signal thread atomically using thread flags (only if thread exists) */
    if (tid_crsf_rx != NULL) {
      osThreadFlagsSet(tid_crsf_rx, RX_BUFFER_READY_FLAG);
    }
  }
  
  if (event & ARM_USART_EVENT_RX_OVERFLOW) {
    /* Hardware FIFO overflow - data lost */
    rx_overflow_count++;
  }
}

static int uart4_init(void) {
  if (ptrUART4->Initialize(uart4_event_callback) != ARM_DRIVER_OK) {
    return -1;
  }

  if (ptrUART4->PowerControl(ARM_POWER_FULL) != ARM_DRIVER_OK) {
    return -1;
  }

  if (ptrUART4->Control(ARM_USART_MODE_ASYNCHRONOUS |
                        ARM_USART_DATA_BITS_8 |
                        ARM_USART_PARITY_NONE |
                        ARM_USART_STOP_BITS_1 |
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

static int uart4_start_receive(void) {
  /* Start first continuous receive (subsequent receives started in callback) */
  if (ptrUART4->Receive((void *)rx_buffer_active, CRSF_RX_CHUNK_SIZE) != ARM_DRIVER_OK) {
    return -1;
  }
  return 0;
}

static void print_decoded_channels(const crsf_channel_data_t *channels) {
  printf("  Decoded %u channel(s), start=%u, bits=%u\n",
         (unsigned int)channels->channel_count,
         (unsigned int)channels->start_channel,
         (unsigned int)channels->bits_per_channel);

  for (uint8_t i = 0U; i < channels->channel_count; i++) {
    const uint8_t ch = (uint8_t)(channels->start_channel + i);
    printf("    CH%u=%u",
           (unsigned int)(ch + 1U),
           (unsigned int)channels->channel[ch]);

    if (((i + 1U) % 4U) == 0U || (i + 1U) == channels->channel_count) {
      printf("\n");
    } else {
      printf("  ");
    }
  }
}

/*
  Always-active UART4 RX decoder thread.
  Waits for interrupt/DMA completion and processes completed buffers.
*/
static __NO_RETURN void thread_crsf_rx(void *argument) {
  crsf_parser_t parser;
  crsf_frame_t frame;
  crsf_channel_data_t channels;
  uint32_t frame_count = 0U;
  uint32_t decoded_count = 0U;
  uint32_t last_overflow_count = 0U;
  uint32_t last_overrun_count = 0U;
  uint32_t buffer_count = 0U;

  (void)argument;

  printf("CRSF RX decoder active on UART4 (interrupt/DMA mode)\n");

  crsf_parser_init(&parser);

  for (;;) {
    /* Wait for buffer ready flag from interrupt (atomically clears flag) */
    uint32_t flags = osThreadFlagsWait(RX_BUFFER_READY_FLAG, osFlagsWaitAny, osWaitForever);
    
    /* Mark buffer as busy - callback will detect overrun if it fires now */
    rx_buffer_busy = 1U;
    
    /* Check if we're falling behind (multiple buffers queued) */
    if (flags & osFlagsError) {
      printf("!!! Thread flag error !!!\n");
    }
    
    buffer_count++;

    /* Process completed buffer (next receive already started in callback) */
    for (uint32_t i = 0U; i < CRSF_RX_CHUNK_SIZE; i++) {
      if (crsf_parser_feed_byte(&parser, rx_buffer_process[i], &frame)) {
        frame_count++;
        
        /* Throttle printf - only print every 20th frame to avoid slowing down */
        const uint32_t print_every = 20U;  /* Reduce to 1 to test overrun detection */
        bool should_print = ((frame_count % print_every) == 0U) || (frame_count <= 6U);

        if (should_print) {
          printf("RX frame #%u: addr=0x%02X type=0x%02X len=%u crc=OK\n",
                 (unsigned int)frame_count,
                 (unsigned int)frame.device_address,
                 (unsigned int)frame.type,
                 (unsigned int)frame.frame_length);
        }

        if (crsf_decode_channels(&frame, &channels)) {
          decoded_count++;
          if (should_print) {
            printf("  Decoded %u channel(s), bits=%u\n", 
                   (unsigned int)channels.channel_count,
                   (unsigned int)channels.bits_per_channel);
            printf("    CH1=%u CH2=%u CH3=%u CH4=%u\n",
                   (unsigned int)channels.channel[0],
                   (unsigned int)channels.channel[1],
                   (unsigned int)channels.channel[2],
                   (unsigned int)channels.channel[3]);
          }
        } else if (should_print) {
          printf("  Frame type 0x%02X not decoded\n", (unsigned int)frame.type);
        }
      }
    }

    /* Clear busy flag - buffer processing complete */
    rx_buffer_busy = 0U;

    /* Print buffer processing rate and overflow check every 50 buffers */
    if ((buffer_count % 50U) == 0U) {
      printf("[Buffers=%u Frames=%u Decoded=%u HW_Overflow=%u SW_Overrun=%u]\n",
             (unsigned int)buffer_count,
             (unsigned int)frame_count,
             (unsigned int)decoded_count,
             (unsigned int)rx_overflow_count,
             (unsigned int)rx_overrun_count);
    }

    /* Check for overflow/overrun errors */
    if (rx_overflow_count != last_overflow_count) {
      printf("!!! RX HARDWARE OVERFLOW detected (total=%u) !!!\n", (unsigned int)rx_overflow_count);
      last_overflow_count = rx_overflow_count;
    }
    
    if (rx_overrun_count != last_overrun_count) {
      printf("!!! BUFFER OVERRUN - Thread too slow! (total=%u) !!!\n", (unsigned int)rx_overrun_count);
      last_overrun_count = rx_overrun_count;
    }
  }
}

/*
  UART4 dummy CRSF transmitter thread.
*/
static __NO_RETURN void thread_crsf_tx(void *argument) {
  uint32_t tx_count = 0U;

  (void)argument;

  printf("CRSF dummy TX active on UART4\n");
  printf("Short UART4 TX (PA0) to UART4 RX (PA1) to self-test parser\n");

  osDelay(100U);

  for (;;) {
    if (ptrUART4->Send(crsf_dummy_stream, (uint32_t)sizeof(crsf_dummy_stream)) != ARM_DRIVER_OK) {
      printf("Dummy TX start failed\n");
      osDelay(100U);
      continue;
    }

    while (ptrUART4->GetStatus().tx_busy != 0U) {
      osDelay(1U);
    }

    tx_count++;
    if ((tx_count % 20U) == 0U) {
      printf("Dummy TX frames sent=%u\n", (unsigned int)tx_count);
    }

    osDelay(20U);
  }
}

/**
  Initialize and start CRSF parser test (UART RX decode + dummy UART TX)

  \return          0 on success, or -1 on error.
*/
int crsf_test_start(void) {
  /* Initialize UART4 hardware (but don't start receive yet) */
  if (uart4_init() != 0) {
    printf("UART4 init failed for CRSF test\n");
    return -1;
  }

  /* Create threads FIRST so tid_crsf_rx is valid for callback */
  tid_crsf_rx = osThreadNew(thread_crsf_rx, NULL, &thread_attr_crsf_rx);
  tid_crsf_tx = osThreadNew(thread_crsf_tx, NULL, &thread_attr_crsf_tx);

  if ((tid_crsf_rx == NULL) || (tid_crsf_tx == NULL)) {
    return -1;
  }

  /* Now start reception - tid_crsf_rx is valid for callback */
  if (uart4_start_receive() != 0) {
    printf("UART4 start receive failed\n");
    return -1;
  }

  return 0;
}
