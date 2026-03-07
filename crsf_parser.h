/*---------------------------------------------------------------------------
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 *---------------------------------------------------------------------------*/

#ifndef CRSF_PARSER_H_
#define CRSF_PARSER_H_

#include <stdbool.h>
#include <stdint.h>

#define CRSF_FRAME_SIZE_MAX                 64U
#define CRSF_FRAME_LENGTH_MIN               2U
#define CRSF_FRAME_LENGTH_MAX               (CRSF_FRAME_SIZE_MAX - 2U)
#define CRSF_PAYLOAD_SIZE_MAX               60U
#define CRSF_MAX_CHANNELS                   24U

#define CRSF_FRAMETYPE_RC_CHANNELS_PACKED        0x16U
#define CRSF_FRAMETYPE_SUBSET_RC_CHANNELS_PACKED 0x17U

typedef struct {
  uint8_t device_address;
  uint8_t frame_length;
  uint8_t type;
  uint8_t payload_length;
  uint8_t payload[CRSF_PAYLOAD_SIZE_MAX];
  uint8_t crc;
} crsf_frame_t;

typedef struct {
  uint8_t buffer[CRSF_FRAME_SIZE_MAX];
  uint8_t index;
  uint8_t expected_length;
} crsf_parser_t;

typedef struct {
  uint8_t frame_type;
  uint8_t start_channel;
  uint8_t bits_per_channel;
  uint8_t channel_count;
  uint16_t channel[CRSF_MAX_CHANNELS];
} crsf_channel_data_t;

void crsf_parser_init(crsf_parser_t *parser);
bool crsf_parser_feed_byte(crsf_parser_t *parser, uint8_t byte, crsf_frame_t *frame_out);
bool crsf_decode_channels(const crsf_frame_t *frame, crsf_channel_data_t *channels_out);

#endif /* CRSF_PARSER_H_ */
