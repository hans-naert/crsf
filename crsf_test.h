/*---------------------------------------------------------------------------
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 *---------------------------------------------------------------------------*/

#ifndef CRSF_TEST_H_
#define CRSF_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
  Initialize and start CRSF parser test with interrupt/DMA-based UART4.
  
  Uses event-driven continuous reception with double-buffering:
  - RX thread processes completed buffers while next buffer fills in background
  - No data loss between receive operations (interrupt/DMA handles gaps)
  - TX thread sends periodic dummy CRSF frames for loopback testing
  
  Test modes:
  - Short UART4 TX-RX (PA0-PA1): Decoder shows dummy data
  - Connect external CRSF/ELRS receiver to RX: Decoder shows real data

  \return          0 on success, or -1 on error.
*/
extern int crsf_test_start(void);

#ifdef __cplusplus
}
#endif

#endif /* CRSF_TEST_H_ */
