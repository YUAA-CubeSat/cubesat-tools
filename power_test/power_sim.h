/*!
 * @file power_sim.h
 * @brief Simulation stubs for hardware / FreeRTOS / SysMan dependencies
 *
 * Replaces the following production headers used by power_dist.c:
 *   - FreeRTOS.h, task.h      (vTaskDelay / vTaskDelete / pdMS_TO_TICKS)
 *   - EPS.h                   (EPS_VoltageType, EPS_R_Voltages, HAL_StatusTypeDef)
 *   - SunDetect.h             (RunSunDetect)
 *   - system_manager.h        (Sys_PowerLevel_e, Sys_TryDev, Sys_GetPowerLevel, Sys_X_SetPowerLevel, panic)
 *
 * All values are driven by test code via the PowerSim_* API below so the
 * power distribution logic can be exercised deterministically off-target.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* HAL / EPS stubs                                                     */
/* ------------------------------------------------------------------ */

typedef enum {
    HAL_OK       = 0x00,
    HAL_ERROR    = 0x01,
    HAL_BUSY     = 0x02,
    HAL_TIMEOUT  = 0x03
} HAL_StatusTypeDef;

typedef struct {
    float V_Batt;   /* in Volts, expected range 3.0 - 4.2 */
    float V_3V3;
    float V_5V;
} EPS_VoltageType;

HAL_StatusTypeDef EPS_R_Voltages(EPS_VoltageType *retval);

/* ------------------------------------------------------------------ */
/* System manager stubs                                                */
/* ------------------------------------------------------------------ */

typedef enum {
    L0 = 0,
    L1,
    L2,
    L3,
    L4,
    NumPwrLvls
} Sys_PowerLevel_e;

/* Device handle enum subset (only DEV_EPS is used by power_dist.c). */
typedef enum { DEV_EPS = 0 } Sys_Device_e;

/* Real macro has retry / logging; the sim just runs `operation` once
 * and captures its return into `retstatVar`. Unused params are kept for
 * call-site compatibility. */
#define Sys_TryDev(retstatVar, operation, successVal, dev) \
    do { (void)(successVal); (void)(dev); (retstatVar) = (operation); } while (0)

Sys_PowerLevel_e Sys_GetPowerLevel(void);
void             Sys_X_SetPowerLevel(Sys_PowerLevel_e lvl);

/* Real firmware aborts; the sim just prints and marks a flag. */
void panic(const char *fmt, ...);

/* ------------------------------------------------------------------ */
/* SunDetect stubs                                                     */
/* ------------------------------------------------------------------ */

/* ORBIT_PHASE used to live in power_dist.h; hoisted here so it is
 * visible to every stub prototype without pulling power_dist.h into
 * power_sim.h. power_dist.h re-exports it by including this file. */
typedef enum {
    PHASE_UNKNOWN,  /* (0) Determination of satellite's position hasn't been run yet */
    PHASE_DARK,     /* (1) Satellite is currently on the shadow side */
    PHASE_SUN,      /* (2) Satellite is currently on the sun side  */
    PHASE_ERR       /* (3) Determination of satellite's position returned error */
} ORBIT_PHASE;

ORBIT_PHASE RunSunDetect(void);

/* ------------------------------------------------------------------ */
/* FreeRTOS stubs                                                      */
/* ------------------------------------------------------------------ */

/* pdMS_TO_TICKS is a pass-through: ticks == ms in the sim. */
#define pdMS_TO_TICKS(ms)  (ms)

/* vTaskDelay simulates one "tick" worth of elapsed time. The sim
 * advances its internal clock + orbit phase + battery model by the
 * requested number of ms (which is normally TASK_PERIOD == 60_000). */
void vTaskDelay(uint32_t ticks_ms);

/* vTaskDelete is a no-op; the task function will return normally. */
void vTaskDelete(void *handle);

/* ------------------------------------------------------------------ */
/* Simulation control API                       */
/* ------------------------------------------------------------------ */

typedef struct {
    /* Starting battery voltage (V). Drained/charged by the simulator
     * as the sim advances minute-by-minute. */
    float    initial_vbatt;
    /* Battery capacity in Watt-minutes (matches EPS_CAPACITY in
     * power_dist.c). Used to convert SoC delta <-> energy. */
    float    capacity_wmin;

    /* Orbit model: `sun_minutes' of SUN phase followed by
     * `dark_minutes' of DARK phase, repeated forever. */
    uint32_t sun_minutes;
    uint32_t dark_minutes;

    /* Average power drawn by the satellite at each power level (W). */
    float    pwrlvl_draw_W[NumPwrLvls];

    /* Solar generation while in sun (W). 0 while in dark. */
    float    solar_gen_W;

    /* If non-zero, EPS_R_Voltages() returns HAL_ERROR on this call. */
    uint32_t fail_eps_on_call;

    /* If non-zero, RunSunDetect returns PHASE_ERR on this call. */
    uint32_t fail_sun_on_call;

    /* Stop the task after this many simulated minutes (0 == run forever). */
    uint32_t stop_after_minutes;
} PowerSim_Config_t;

typedef struct {
    /* State read back by the test harness. */
    uint32_t         elapsed_minutes;
    float            vbatt;
    Sys_PowerLevel_e current_level;
    uint32_t         eps_call_count;
    uint32_t         sun_call_count;
    uint32_t         set_level_count;
    uint32_t         panic_count;
    int              task_exited;
} PowerSim_State_t;

void PowerSim_Reset(const PowerSim_Config_t *cfg);
const PowerSim_State_t *PowerSim_GetState(void);

/* Convenience: current simulated orbit phase (based on elapsed_minutes). */
ORBIT_PHASE PowerSim_CurrentPhase(void);
