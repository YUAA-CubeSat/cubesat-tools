#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
    HAL_OK       = 0x00,
    HAL_ERROR    = 0x01,
    HAL_BUSY     = 0x02,
    HAL_TIMEOUT  = 0x03
} HAL_StatusTypeDef;

typedef struct {
    float V_Batt;
    float V_3V3;
    float V_5V;
} EPS_VoltageType;

/* Mirror of flight Drivers/YUAA/EPS.h::EPS_PanelType. One voltage per
 * axis (post-MPPT bus side; the +face and -face panels on each axis are
 * paralleled through a single MPPT channel) and one current per face.
 * Voltages in V, currents in A. */
typedef struct {
    float X_V;
    float X_minusI;
    float X_plusI;
    float Y_V;
    float Y_minusI;
    float Y_plusI;
    float Z_V;
    float Z_minusI;
    float Z_plusI;
} EPS_PanelType;

HAL_StatusTypeDef EPS_R_Voltages(EPS_VoltageType *retval);
HAL_StatusTypeDef EPS_R_Panels  (EPS_PanelType   *retval);

typedef enum {
    L0 = 0,
    L1,
    L2,
    L3,
    L4,
    NumPwrLvls
} Sys_PowerLevel_e;

typedef enum { DEV_EPS = 0 } Sys_Device_e;

#define Sys_TryDev(retstatVar, operation, successVal, dev) \
    do { (void)(successVal); (void)(dev); (retstatVar) = (operation); } while (0)

Sys_PowerLevel_e Sys_GetPowerLevel(void);
void             Sys_X_SetPowerLevel(Sys_PowerLevel_e lvl);

void panic(const char *fmt, ...);

typedef enum {
    PHASE_UNKNOWN,
    PHASE_DARK,
    PHASE_SUN,
    PHASE_ERR
} ORBIT_PHASE;

ORBIT_PHASE RunSunDetect(void);

#define pdMS_TO_TICKS(ms)  (ms)

void vTaskDelay(uint32_t ticks_ms);
void vTaskDelete(void *handle);
void PowerSim_AdvanceOneMinute(void);

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

    /* Raw panel capability while in sun (W), before MPPT/charger throttle.
     * 0 while in dark. The MPPT only pulls what the bus + battery charger
     * can absorb — the *delivered* power reported by EPS_R_InstGen is
     * usually lower, see sim_compute_bus() in power_sim.c. */
    float    solar_gen_W;

    /* Battery charger model (EPS I User Manual, Table 1).
     *   eoc_voltage:      Constant-voltage cut-off (V). Typical 4.10 V,
     *                     spec 4.08 / 4.10 / 4.12. Above this the
     *                     charger stops absorbing current from the bus.
     *   charge_current_A: CC-regime current limit (A). Selectable modes:
     *                     Mode 1 = 0.230, Mode 2 = 0.460 (default),
     *                     Mode 3 = 0.690. Max charge power ≈ I * V_batt.
     * Zero / negative values mean "use the simulator default". */
    float    eoc_voltage;
    float    charge_current_A;

    /* If non-zero, the Nth EPS read (across EPS_R_Voltages /
     * EPS_R_Panels combined) returns HAL_ERROR. The counter is the
     * shared eps_call_count in PowerSim_State_t. */
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
ORBIT_PHASE PowerSim_CurrentPhase(void);
