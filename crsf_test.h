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
  Initialize and start CRSF parser test thread

  \return          0 on success, or -1 on error.
*/
extern int crsf_test_start(void);

#ifdef __cplusplus
}
#endif

#endif /* CRSF_TEST_H_ */
