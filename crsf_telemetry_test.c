/*---------------------------------------------------------------------------
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 *---------------------------------------------------------------------------*/

#include <stdio.h>

#include "cmsis_os2.h"
#include "Driver_USART.h"

#include "crsf_telemetry.h"
#include "crsf_telemetry_test.h"

/* CMSIS USART Driver for UART4 */
#define UART4_DRIVER_NUM 4
extern ARM_DRIVER_USART ARM_Driver_USART_(UART4_DRIVER_NUM);
#define ptrUART4 (&ARM_Driver_USART_(UART4_DRIVER_NUM))

/* Thread attributes */
static const osThreadAttr_t thread_attr_telemetry = { 
  .name = "CRSF_TELEMETRY",
  .priority = osPriorityNormal
};

/* Thread ID */
static osThreadId_t tid_telemetry;

/* Simulated battery state */
static crsf_battery_t battery_state = {
  .voltage_dV = 126,       /* 12.6V */
  .current_dA = 35,        /* 3.5A */
  .capacity_mAh = 850,     /* 850mAh consumed */
  .remaining_pct = 65      /* 65% remaining */
};

/*
  CRSF telemetry transmitter thread.
  Sends battery telemetry at 10 Hz and heartbeat at 1 Hz.
*/
static __NO_RETURN void thread_crsf_telemetry(void *argument) {
  uint8_t frame[16];
  uint8_t len;
  uint32_t tx_count = 0U;
  uint32_t heartbeat_count = 0U;

  (void)argument;

  printf("CRSF Telemetry TX active on UART4\n");
  printf("Battery: %.1fV %.1fA %umAh %u%%\n",
         (float)battery_state.voltage_dV / 10.0f,
         (float)battery_state.current_dA / 10.0f,
         (unsigned int)battery_state.capacity_mAh,
         (unsigned int)battery_state.remaining_pct);

  osDelay(200U);

  for (;;) {
    /* Send battery telemetry frame */
    len = crsf_build_battery_frame(frame, &battery_state);
    if (len > 0U) {
      if (ptrUART4->Send(frame, len) == ARM_DRIVER_OK) {
        /* Wait for transmission complete */
        while (ptrUART4->GetStatus().tx_busy != 0U) {
          osDelay(1U);
        }
        tx_count++;
      } else {
        printf("Battery TX failed\n");
      }
    }

    /* Send heartbeat every 10th frame (1 Hz when battery is 10 Hz) */
    if ((tx_count % 10U) == 0U) {
      len = crsf_build_heartbeat_frame(frame);
      if (len > 0U) {
        if (ptrUART4->Send(frame, len) == ARM_DRIVER_OK) {
          while (ptrUART4->GetStatus().tx_busy != 0U) {
            osDelay(1U);
          }
          heartbeat_count++;
        }
      }
    }

    /* Print status every 50 frames */
    if ((tx_count % 50U) == 0U) {
      printf("Telemetry TX: Battery=%u Heartbeat=%u\n",
             (unsigned int)tx_count,
             (unsigned int)heartbeat_count);
    }

    /* Simulate battery discharge (for demo purposes) */
    if ((tx_count % 100U) == 0U && battery_state.remaining_pct > 0U) {
      battery_state.voltage_dV = (uint16_t)(battery_state.voltage_dV > 100U ? battery_state.voltage_dV - 1U : battery_state.voltage_dV);
      battery_state.capacity_mAh += 10U;
      battery_state.remaining_pct = (uint8_t)(battery_state.remaining_pct > 1U ? battery_state.remaining_pct - 1U : 0U);
    }

    /* 10 Hz rate (100ms period) */
    osDelay(100U);
  }
}

static int uart4_init_telemetry(void) {
  if (ptrUART4->Initialize(NULL) != ARM_DRIVER_OK) {
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

  if (ptrUART4->Control(ARM_USART_CONTROL_TX, 1U) != ARM_DRIVER_OK) {
    return -1;
  }

  return 0;
}

/**
  Initialize and start CRSF telemetry test (transmit battery/heartbeat)

  \return          0 on success, or -1 on error.
*/
int crsf_telemetry_test_start(void) {
  /* Initialize UART4 hardware for TX only */
  if (uart4_init_telemetry() != 0) {
    printf("UART4 init failed for telemetry test\n");
    return -1;
  }

  /* Create telemetry thread */
  tid_telemetry = osThreadNew(thread_crsf_telemetry, NULL, &thread_attr_telemetry);

  if (tid_telemetry == NULL) {
    return -1;
  }

  return 0;
}

/**
  Update battery telemetry values.
  Call this from your flight controller to update real battery data.

  \param[in] battery      New battery telemetry values
*/
void crsf_telemetry_update_battery(const crsf_battery_t *battery) {
  if (battery != NULL) {
    battery_state = *battery;
  }
}
