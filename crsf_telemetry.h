/*---------------------------------------------------------------------------
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 *---------------------------------------------------------------------------*/

#ifndef CRSF_TELEMETRY_H_
#define CRSF_TELEMETRY_H_

#include <stdint.h>

/* CRSF Addresses */
#define CRSF_ADDRESS_BROADCAST          0x00U
#define CRSF_ADDRESS_FLIGHT_CONTROLLER  0xC8U
#define CRSF_ADDRESS_RADIO_TRANSMITTER  0xEAU
#define CRSF_ADDRESS_CRSF_RECEIVER      0xECU

/* CRSF Frame Types (Telemetry) */
#define CRSF_FRAMETYPE_GPS              0x02U
#define CRSF_FRAMETYPE_BATTERY_SENSOR   0x08U
#define CRSF_FRAMETYPE_BARO_ALTITUDE    0x09U
#define CRSF_FRAMETYPE_HEARTBEAT        0x0BU
#define CRSF_FRAMETYPE_ATTITUDE         0x1EU
#define CRSF_FRAMETYPE_FLIGHT_MODE      0x21U

/* Battery telemetry data */
typedef struct {
  uint16_t voltage_dV;      /* Voltage in decivolts (e.g., 126 = 12.6V) */
  uint16_t current_dA;      /* Current in deciamps (e.g., 35 = 3.5A) */
  uint32_t capacity_mAh;    /* Consumed capacity in mAh */
  uint8_t  remaining_pct;   /* Remaining battery percentage (0-100) */
} crsf_battery_t;

/**
  Build a CRSF battery sensor frame.

  \param[out] frame_out     Buffer to write frame (min 12 bytes)
  \param[in]  battery       Battery telemetry data
  \return                   Frame length in bytes (12 for battery frame)
*/
uint8_t crsf_build_battery_frame(uint8_t *frame_out, const crsf_battery_t *battery);

/**
  Build a CRSF heartbeat frame.

  \param[out] frame_out     Buffer to write frame (min 5 bytes)
  \return                   Frame length in bytes (5 for heartbeat frame)
*/
uint8_t crsf_build_heartbeat_frame(uint8_t *frame_out);

#endif /* CRSF_TELEMETRY_H_ */
