/*!
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* @file power_dist.h
* @brief Power Distribution Header File (standalone test copy)
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* @author            Elena W., Nich S., Anton M.
* @date              2023.03.31
*
* @details           Declares power distribution routines.
*                    The original file pulls in system_manager.h for
*                    HAL_StatusTypeDef; in this standalone build that
*                    type (and all other hardware/RTOS dependencies) is
*                    provided by power_sim.h.
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
#pragma once
#define POWER_DIST_H

#include <stdint.h>

/* HAL_StatusTypeDef, Sys_PowerLevel_e, ORBIT_PHASE, etc. come from
 * here in the standalone build instead of from system_manager.h. */
#include "power_sim.h"

/*!
 * @brief Compute EPS battery charge percentage from voltage
 * @param voltage  expected range 3.0 - 4.2
 * @return         percentage 0..100
 */
float PowerDist_GetSoC(float voltage);

/*!
 * @brief Check if EPS battery is critical
 * @param[out] isCrit  populated with 1 if SoC below threshold, 0 otherwise
 * @return HAL_StatusTypeDef containing result of EPS interface
 */
HAL_StatusTypeDef PowerDist_IsBattCritical(uint8_t *isCrit);

/*!
 * @brief Task entry point for the power distribution loop.
 *        In the standalone build this is a plain function that runs
 *        until the simulated EPS reports an unrecoverable error
 *        (driven by PowerSim_Config_t::stop_after_minutes).
 */
void PowerDist_X_Task(void *argument);

/* -------------------------------------------------------------------- */
/* Test hooks – these are internal helpers exposed for the test harness */
/* -------------------------------------------------------------------- */

/* Number of consumption-buffer slots exposed so tests can size state. */
#define POWERDIST_CONSBUF_NUM_ELTS (90 / 5)

typedef struct {
    float    five_min_consumption[POWERDIST_CONSBUF_NUM_ELTS];
    uint8_t  index;
    uint8_t  len;
} PowerDist_ConsumptionBuffer_t;

/* Read-only snapshots of internal state for assertions. */
const PowerDist_ConsumptionBuffer_t *PowerDist_TestGetConsBuf(void);
float                                 PowerDist_TestGetBattChgCache(void);
ORBIT_PHASE                           PowerDist_TestGetCurrPhase(void);

/* Reset all internal caches – useful between test cases. */
void PowerDist_TestResetState(void);

/* Directly exercise the base-level algorithm without running the task. */
Sys_PowerLevel_e PowerDist_TestCalcBasePwrLvl(Sys_PowerLevel_e currLvl,
                                              float avgCons,
                                              float avgGen);

/* Directly exercise the consumption-buffer helpers. */
void  PowerDist_TestInsertConsumption(float battChgBefore, float battChgAfter);
float PowerDist_TestCalcBufAvg(void);
float PowerDist_TestCalcTotalConsumption90(void);
