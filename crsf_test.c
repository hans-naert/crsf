/*---------------------------------------------------------------------------
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 *---------------------------------------------------------------------------*/

#include <stdio.h>

#include "cmsis_os2.h"

#include "crsf_parser.h"
#include "crsf_test.h"

/* Thread attributes for the parser test thread */
static const osThreadAttr_t thread_attr_crsf_test = { .name = "CRSF_Test" };

/* Thread ID for the parser test thread */
static osThreadId_t tid_crsf_test;

/* Fixed CRSF test stream (contains two valid 0x16 RC frames with noise bytes) */
static const uint8_t crsf_test_stream[] = {
  0x55U, 0xA5U,
  0x00U, 0x18U, 0x16U, 0xBDU, 0x08U, 0x9FU, 0xF4U, 0xAEU, 0xF7U,
  0xBDU, 0xEFU, 0x7DU, 0xEFU, 0xFBU, 0xADU, 0xFDU, 0x45U, 0x2BU,
  0x5AU, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x6CU,
  0x13U,
  0x00U, 0x18U, 0x16U, 0xBDU, 0x08U, 0x9FU, 0xF4U, 0xAAU, 0xF7U,
  0xBDU, 0xEFU, 0x7DU, 0xEFU, 0xFBU, 0xADU, 0xFDU, 0x45U, 0x2BU,
  0x5AU, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x94U
};

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
  Thread that tests the clean-room CRSF parser with fixed data.
*/
static __NO_RETURN void thread_crsf_test(void *argument) {
  crsf_parser_t parser;
  crsf_frame_t frame;
  crsf_channel_data_t channels;
  uint32_t cycle = 0U;
  uint32_t total_frames = 0U;
  uint32_t total_decoded = 0U;

  (void)argument;

  printf("CRSF parser test started (fixed byte stream)\n");

  for (;;) {
    uint32_t cycle_frames = 0U;
    uint32_t cycle_decoded = 0U;

    cycle++;
    crsf_parser_init(&parser);

    for (uint32_t i = 0U; i < (uint32_t)(sizeof(crsf_test_stream) / sizeof(crsf_test_stream[0])); i++) {
      if (crsf_parser_feed_byte(&parser, crsf_test_stream[i], &frame)) {
        cycle_frames++;
        total_frames++;

        printf("Cycle %u Frame %u: addr=0x%02X type=0x%02X len=%u crc=OK\n",
               (unsigned int)cycle,
               (unsigned int)cycle_frames,
               (unsigned int)frame.device_address,
               (unsigned int)frame.type,
               (unsigned int)frame.frame_length);

        if (crsf_decode_channels(&frame, &channels)) {
          cycle_decoded++;
          total_decoded++;
          print_decoded_channels(&channels);
        }
      }
    }

    printf("Cycle %u summary: frames=%u decoded=%u (total frames=%u decoded=%u)\n\n",
           (unsigned int)cycle,
           (unsigned int)cycle_frames,
           (unsigned int)cycle_decoded,
           (unsigned int)total_frames,
           (unsigned int)total_decoded);

    osDelay(3000U);
  }
}

/**
  Initialize and start CRSF parser test

  \return          0 on success, or -1 on error.
*/
int crsf_test_start(void) {
  tid_crsf_test = osThreadNew(thread_crsf_test, NULL, &thread_attr_crsf_test);

  if (tid_crsf_test == NULL) {
    return -1;
  }

  return 0;
}
