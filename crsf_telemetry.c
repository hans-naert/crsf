/*---------------------------------------------------------------------------
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 *---------------------------------------------------------------------------*/

#include "crsf_telemetry.h"

/*
  CRC8-DVB-S2 calculation for CRSF frames.
  Polynomial: 0xD5
*/
static uint8_t crsf_crc8_dvb_s2(const uint8_t *data, uint8_t length) {
  uint8_t crc = 0U;

  for (uint8_t i = 0U; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0U; bit < 8U; bit++) {
      if ((crc & 0x80U) != 0U) {
        crc = (uint8_t)((crc << 1U) ^ 0xD5U);
      } else {
        crc <<= 1U;
      }
    }
  }

  return crc;
}

/**
  Build a CRSF battery sensor frame.

  Frame structure:
  [0] Address (0xC8 = Flight Controller)
  [1] Length (10 = type + payload + crc)
  [2] Type (0x08 = Battery Sensor)
  [3-4] Voltage (big-endian, decivolts)
  [5-6] Current (big-endian, deciamps)
  [7-9] Capacity (big-endian 24-bit, mAh)
  [10] Remaining (percent)
  [11] CRC8

  \param[out] frame_out     Buffer to write frame (min 12 bytes)
  \param[in]  battery       Battery telemetry data
  \return                   Frame length in bytes (12)
*/
uint8_t crsf_build_battery_frame(uint8_t *frame_out, const crsf_battery_t *battery) {
  if ((frame_out == NULL) || (battery == NULL)) {
    return 0U;
  }

  frame_out[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
  frame_out[1] = 10U;  /* Length: type(1) + payload(8) + crc(1) */
  frame_out[2] = CRSF_FRAMETYPE_BATTERY_SENSOR;

  /* Voltage (big-endian) */
  frame_out[3] = (uint8_t)(battery->voltage_dV >> 8U);
  frame_out[4] = (uint8_t)(battery->voltage_dV);

  /* Current (big-endian) */
  frame_out[5] = (uint8_t)(battery->current_dA >> 8U);
  frame_out[6] = (uint8_t)(battery->current_dA);

  /* Capacity - 24-bit big-endian */
  frame_out[7] = (uint8_t)(battery->capacity_mAh >> 16U);
  frame_out[8] = (uint8_t)(battery->capacity_mAh >> 8U);
  frame_out[9] = (uint8_t)(battery->capacity_mAh);

  /* Remaining percentage */
  frame_out[10] = battery->remaining_pct;

  /* CRC over type + payload (bytes 2-10) */
  frame_out[11] = crsf_crc8_dvb_s2(&frame_out[2], 9U);

  return 12U;
}

/**
  Build a CRSF heartbeat frame.

  Frame structure:
  [0] Address (0xC8 = Flight Controller)
  [1] Length (4 = type + payload + crc)
  [2] Type (0x0B = Heartbeat)
  [3-4] Origin address (big-endian, 0xC8 = Flight Controller)
  [5] CRC8

  \param[out] frame_out     Buffer to write frame (min 6 bytes)
  \return                   Frame length in bytes (6)
*/
uint8_t crsf_build_heartbeat_frame(uint8_t *frame_out) {
  if (frame_out == NULL) {
    return 0U;
  }

  frame_out[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
  frame_out[1] = 4U;  /* Length: type(1) + payload(2) + crc(1) */
  frame_out[2] = CRSF_FRAMETYPE_HEARTBEAT;

  /* Origin address (big-endian) */
  frame_out[3] = 0x00U;
  frame_out[4] = CRSF_ADDRESS_FLIGHT_CONTROLLER;

  /* CRC over type + payload (bytes 2-4) */
  frame_out[5] = crsf_crc8_dvb_s2(&frame_out[2], 3U);

  return 6U;
}
