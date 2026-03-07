/*---------------------------------------------------------------------------
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 *---------------------------------------------------------------------------*/

#include <string.h>

#include "crsf_parser.h"

static uint8_t crsf_crc8_dvb_s2_step(uint8_t crc, uint8_t value) {
  crc ^= value;
  for (uint8_t bit = 0U; bit < 8U; bit++) {
    if ((crc & 0x80U) != 0U) {
      crc = (uint8_t)((crc << 1U) ^ 0xD5U);
    } else {
      crc <<= 1U;
    }
  }
  return crc;
}

static uint8_t crsf_crc8_dvb_s2_buffer(const uint8_t *data, uint8_t length) {
  uint8_t crc = 0U;

  for (uint8_t i = 0U; i < length; i++) {
    crc = crsf_crc8_dvb_s2_step(crc, data[i]);
  }

  return crc;
}

static uint16_t crsf_extract_bits(const uint8_t *buffer, uint16_t bit_offset, uint8_t bit_count) {
  uint32_t value = 0U;

  for (uint8_t bit = 0U; bit < bit_count; bit++) {
    const uint16_t absolute_bit = (uint16_t)(bit_offset + bit);
    const uint8_t byte_index = (uint8_t)(absolute_bit / 8U);
    const uint8_t bit_index = (uint8_t)(absolute_bit % 8U);
    const uint32_t bit_value = (uint32_t)((buffer[byte_index] >> bit_index) & 0x01U);
    value |= (bit_value << bit);
  }

  return (uint16_t)value;
}

void crsf_parser_init(crsf_parser_t *parser) {
  if (parser == NULL) {
    return;
  }

  parser->index = 0U;
  parser->expected_length = 0U;
}

bool crsf_parser_feed_byte(crsf_parser_t *parser, uint8_t byte, crsf_frame_t *frame_out) {
  uint8_t frame_length;
  uint8_t payload_length;
  uint8_t frame_size;
  uint8_t crc_rx;
  uint8_t crc_calc;
  uint8_t trailing;

  if ((parser == NULL) || (frame_out == NULL)) {
    return false;
  }

  if (parser->index >= CRSF_FRAME_SIZE_MAX) {
    /* Buffer is full of undecodable bytes: drop oldest byte and keep sliding. */
    (void)memmove(parser->buffer, &parser->buffer[1], (size_t)(CRSF_FRAME_SIZE_MAX - 1U));
    parser->index = (uint8_t)(CRSF_FRAME_SIZE_MAX - 1U);
  }

  parser->buffer[parser->index++] = byte;

  /*
    Sliding parser:
    - Byte 0 is candidate address.
    - Byte 1 is candidate length.
    - On invalid length or CRC failure, slide by one byte and retry.
  */
  while (parser->index >= 2U) {
    frame_length = parser->buffer[1];

    if ((frame_length < CRSF_FRAME_LENGTH_MIN) || (frame_length > CRSF_FRAME_LENGTH_MAX)) {
      (void)memmove(parser->buffer, &parser->buffer[1], (size_t)(parser->index - 1U));
      parser->index--;
      parser->expected_length = 0U;
      continue;
    }

    frame_size = (uint8_t)(frame_length + 2U);
    parser->expected_length = frame_size;

    if (parser->index < frame_size) {
      return false;
    }

    payload_length = (uint8_t)(frame_length - 2U);
    crc_rx = parser->buffer[frame_size - 1U];
    crc_calc = crsf_crc8_dvb_s2_buffer(&parser->buffer[2], (uint8_t)(frame_length - 1U));

    if ((payload_length <= CRSF_PAYLOAD_SIZE_MAX) && (crc_rx == crc_calc)) {
      frame_out->device_address = parser->buffer[0];
      frame_out->frame_length = frame_length;
      frame_out->type = parser->buffer[2];
      frame_out->payload_length = payload_length;
      frame_out->crc = crc_rx;

      if (payload_length > 0U) {
        memcpy(frame_out->payload, &parser->buffer[3], payload_length);
      }

      trailing = (uint8_t)(parser->index - frame_size);
      if (trailing > 0U) {
        (void)memmove(parser->buffer, &parser->buffer[frame_size], trailing);
      }

      parser->index = trailing;
      parser->expected_length = 0U;
      return true;
    }

    /* CRC failed: slide by one byte and retry lock on next candidate. */
    (void)memmove(parser->buffer, &parser->buffer[1], (size_t)(parser->index - 1U));
    parser->index--;
    parser->expected_length = 0U;
  }

  parser->expected_length = 0U;
  return false;
}

bool crsf_decode_channels(const crsf_frame_t *frame, crsf_channel_data_t *channels_out) {
  uint8_t payload_bytes;
  uint8_t config;
  uint8_t resolution_cfg;
  uint8_t bits_per_channel;
  uint8_t channel_count;
  uint8_t decode_count;

  if ((frame == NULL) || (channels_out == NULL)) {
    return false;
  }

  memset(channels_out, 0, sizeof(*channels_out));
  channels_out->frame_type = frame->type;

  if (frame->type == CRSF_FRAMETYPE_RC_CHANNELS_PACKED) {
    if (frame->payload_length < 22U) {
      return false;
    }

    channels_out->start_channel = 0U;
    channels_out->bits_per_channel = 11U;
    channels_out->channel_count = 16U;

    for (uint8_t ch = 0U; ch < 16U; ch++) {
      channels_out->channel[ch] = crsf_extract_bits(frame->payload, (uint16_t)(ch * 11U), 11U);
    }

    return true;
  }

  if (frame->type == CRSF_FRAMETYPE_SUBSET_RC_CHANNELS_PACKED) {
    if (frame->payload_length < 2U) {
      return false;
    }

    config = frame->payload[0];
    channels_out->start_channel = (uint8_t)(config & 0x1FU);
    resolution_cfg = (uint8_t)((config >> 5U) & 0x03U);

    switch (resolution_cfg) {
      case 0U:
        bits_per_channel = 10U;
        break;
      case 1U:
        bits_per_channel = 11U;
        break;
      case 2U:
        bits_per_channel = 12U;
        break;
      default:
        bits_per_channel = 13U;
        break;
    }

    if (channels_out->start_channel >= CRSF_MAX_CHANNELS) {
      return false;
    }

    channels_out->bits_per_channel = bits_per_channel;

    payload_bytes = (uint8_t)(frame->payload_length - 1U);
    channel_count = (uint8_t)((payload_bytes * 8U) / bits_per_channel);

    if (channel_count == 0U) {
      return false;
    }

    decode_count = channel_count;
    if ((uint8_t)(channels_out->start_channel + decode_count) > CRSF_MAX_CHANNELS) {
      decode_count = (uint8_t)(CRSF_MAX_CHANNELS - channels_out->start_channel);
    }

    channels_out->channel_count = decode_count;

    for (uint8_t i = 0U; i < decode_count; i++) {
      const uint16_t bit_offset = (uint16_t)(i * bits_per_channel);
      const uint8_t ch = (uint8_t)(channels_out->start_channel + i);
      channels_out->channel[ch] = crsf_extract_bits(&frame->payload[1], bit_offset, bits_per_channel);
    }

    return true;
  }

  return false;
}
