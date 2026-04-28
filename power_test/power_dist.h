#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "power_sim.h"

/*!
 * @brief  Convert EPS battery terminal voltage into state-of-charge.
 * @param  voltage  Battery voltage in Volts, nominal range 3.0 .. 4.2.
 * @return          SoC as a percentage in [0, 100].
 */
float PowerDist_GetSoC(const float voltage);

/*!
 * @brief  Read the EPS battery voltages and test whether the derived SoC
 *         is below the critical threshold (CRITICAL_SOC_PERCENT).
 * @param[out] isCrit  Optional. If non-NULL, set to 1 when
 *                     SoC < CRITICAL_SOC_PERCENT, else 0. Left untouched
 *                     on a failed EPS read.
 * @param[out] V       Optional. If non-NULL, receives the full EPS
 *                     voltage struct (V_Batt, V_3V3, V_5V) on a
 *                     successful read. Left untouched on a failed EPS
 *                     read. Pass NULL if the caller only wants the flag.
 * @return  Status of the underlying EPS read
 */
HAL_StatusTypeDef PowerDist_IsBattCritical(uint8_t *const isCrit,
                                           EPS_VoltageType *const V);

/*!
 * @brief  Task entry point for the power-distribution loop.
 *
 * On flight this is an RTOS task body that never returns. In the standalone
 * build it runs until the simulated EPS reports an unrecoverable error
 * (PowerSim_Config_t::stop_after_minutes).
 */
void PowerDist_X_Task(void *argument);
