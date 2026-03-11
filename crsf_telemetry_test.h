/*---------------------------------------------------------------------------
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 *---------------------------------------------------------------------------*/

#ifndef CRSF_TELEMETRY_TEST_H_
#define CRSF_TELEMETRY_TEST_H_

#include "crsf_telemetry.h"

/**
  Initialize and start CRSF telemetry test.
  
  Creates a thread that transmits battery telemetry at 10 Hz
  and heartbeat frames at 1 Hz on UART4.
  
  \return          0 on success, or -1 on error.
*/
int crsf_telemetry_test_start(void);

/**
  Update battery telemetry values.
  
  Call this from your flight controller to update real battery data.
  The telemetry thread will transmit the updated values.
  
  \param[in] battery      New battery telemetry values
*/
void crsf_telemetry_update_battery(const crsf_battery_t *battery);

#endif /* CRSF_TELEMETRY_TEST_H_ */
