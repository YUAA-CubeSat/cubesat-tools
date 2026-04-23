/*!
 * @file power_sim.c
 * @brief Simulation stubs for PowerDist off-target testing.
 */
#include "power_dist.h"   /* for ORBIT_PHASE values */
#include "power_sim.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static PowerSim_Config_t s_cfg;
static PowerSim_State_t  s_state;
static float s_battery_soc;  /* 0..100 */

/* SoC lookup – mirrors the table in power_dist.c so VBatt <-> SoC is
 * self-consistent inside the sim. Only used to initialize the battery
 * model; once running, we drain energy in W·min and back-map to SoC. */
static const float SIM_SOC_VOLTAGES[] = { 
    3.0f, 3.5f, 3.6f, 
    3.7f, 3.8f, 3.9f, 
    4.0f, 4.1f, 4.2f 
};
static const float SIM_SOC_VALS[] = { 
    0.0f, 2.16f, 3.57f, 
    8.98f, 23.15f, 58.87f, 
    77.91f, 90.91f, 100.0f 
};

static float sim_voltage_to_soc(float voltage)
{
    if (voltage <= SIM_SOC_VOLTAGES[0]) return SIM_SOC_VALS[0];
    if (voltage >= SIM_SOC_VOLTAGES[8]) return SIM_SOC_VALS[8];
    int i = 1;
    while (voltage >= SIM_SOC_VOLTAGES[i]) i++;
    return SIM_SOC_VALS[i - 1] +
           (voltage - SIM_SOC_VOLTAGES[i - 1]) *
           (SIM_SOC_VALS[i] - SIM_SOC_VALS[i - 1]) /
           (SIM_SOC_VOLTAGES[i] - SIM_SOC_VOLTAGES[i - 1]);
}

static float sim_soc_to_voltage(float soc)
{
    if (soc <= SIM_SOC_VALS[0]) return SIM_SOC_VOLTAGES[0];
    if (soc >= SIM_SOC_VALS[8]) return SIM_SOC_VOLTAGES[8];
    int i = 1;
    while (soc >= SIM_SOC_VALS[i]) i++;
    return SIM_SOC_VOLTAGES[i - 1] +
           (soc - SIM_SOC_VALS[i - 1]) *
           (SIM_SOC_VOLTAGES[i] - SIM_SOC_VOLTAGES[i - 1]) /
           (SIM_SOC_VALS[i] - SIM_SOC_VALS[i - 1]);
}

static void advance_battery_one_minute(void)
{
    /* Lookup draw based on current power level. */
    Sys_PowerLevel_e lvl = s_state.current_level;
    if (lvl >= NumPwrLvls) lvl = (Sys_PowerLevel_e)(NumPwrLvls - 1);
    float draw_W = s_cfg.pwrlvl_draw_W[lvl];

    /* Generation: only while in sun phase. */
    float gen_W = (PowerSim_CurrentPhase() == PHASE_SUN) ? s_cfg.solar_gen_W : 0.0f;

    /* Net energy delta over 1 minute, in W·min. */
    float delta_wmin = (gen_W - draw_W) /* positive = charging */;

    /* Convert to SoC delta: delta_SoC (percent) = delta_wmin / capacity * 100 */
    float delta_soc = (s_cfg.capacity_wmin > 0.0f)
                      ? (delta_wmin / s_cfg.capacity_wmin) * 100.0f
                      : 0.0f;

    s_battery_soc += delta_soc;
    if (s_battery_soc < 0.0f)   s_battery_soc = 0.0f;
    if (s_battery_soc > 100.0f) s_battery_soc = 100.0f;

    s_state.vbatt = sim_soc_to_voltage(s_battery_soc);
}

/* ------------------------------------------------------------------ */
/* Public sim control                                                  */
/* ------------------------------------------------------------------ */

void PowerSim_Reset(const PowerSim_Config_t *cfg)
{
    assert(cfg);
    memset(&s_state, 0, sizeof(s_state));    
    s_cfg = *cfg;
    s_state.current_level = L4; /* default at boot */
    s_state.vbatt         = s_cfg.initial_vbatt;
    s_battery_soc         = sim_voltage_to_soc(s_state.vbatt);
}

const PowerSim_State_t *PowerSim_GetState(void) { return &s_state; }

ORBIT_PHASE PowerSim_CurrentPhase(void)
{
    uint32_t period = s_cfg.sun_minutes + s_cfg.dark_minutes;
    if (period == 0) return PHASE_SUN; /* default sun-only world */
    uint32_t t = s_state.elapsed_minutes % period;
    return (t < s_cfg.sun_minutes) ? PHASE_SUN : PHASE_DARK;
}

/* ------------------------------------------------------------------ */
/* HAL / EPS                                                           */
/* ------------------------------------------------------------------ */

HAL_StatusTypeDef EPS_R_Voltages(EPS_VoltageType *retval)
{
    s_state.eps_call_count++;
    /* Force task exit once the scripted run-length is reached. The task
     * treats a HAL_ERROR from EPS as unrecoverable and terminates. */
    if (s_cfg.stop_after_minutes &&
        s_state.elapsed_minutes >= s_cfg.stop_after_minutes) {
        return HAL_ERROR;
    }
    if (s_cfg.fail_eps_on_call && s_cfg.fail_eps_on_call == s_state.eps_call_count) {
        return HAL_ERROR;
    }
    if (retval == NULL) return HAL_ERROR;
    retval->V_Batt = s_state.vbatt;
    retval->V_3V3  = 3.3f;
    retval->V_5V   = 5.0f;
    return HAL_OK;
}

/* ------------------------------------------------------------------ */
/* SysMan                                                              */
/* ------------------------------------------------------------------ */

Sys_PowerLevel_e Sys_GetPowerLevel(void)         { return s_state.current_level; }
void Sys_X_SetPowerLevel(Sys_PowerLevel_e lvl)
{
    s_state.set_level_count++;
    if (lvl < NumPwrLvls) s_state.current_level = lvl;
}

void panic(const char *fmt, ...)
{
    s_state.panic_count++;
    fprintf(stderr, "[SIM panic] ");
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    /* Do NOT abort – let the task continue so tests can observe. */
}

/* ------------------------------------------------------------------ */
/* SunDetect                                                           */
/* ------------------------------------------------------------------ */

ORBIT_PHASE RunSunDetect(void)
{
    s_state.sun_call_count++;
    if (s_cfg.fail_sun_on_call && s_cfg.fail_sun_on_call == s_state.sun_call_count) {
        return PHASE_ERR;
    }
    return PowerSim_CurrentPhase();
}

/* ------------------------------------------------------------------ */
/* FreeRTOS                                                            */
/* ------------------------------------------------------------------ */

void vTaskDelay(uint32_t ticks_ms)
{
    /* Advance simulated time in whole minutes. Any TASK_PERIOD rounding
     * below 1 minute is treated as "1 minute" to keep the model simple. */
    uint32_t minutes = ticks_ms / (60u * 1000u);
    if (minutes == 0) minutes = 1;
    for (uint32_t i = 0; i < minutes; ++i) {
        advance_battery_one_minute();
        s_state.elapsed_minutes++;
        if (s_cfg.stop_after_minutes &&
            s_state.elapsed_minutes >= s_cfg.stop_after_minutes) {
            /* Nothing to do here – the task loop checks this in
             * vTaskDelete via s_state.task_exited. We set a flag and
             * rely on the task structure; see power_dist.c. */
            break;
        }
    }
}

void vTaskDelete(void *handle)
{
    (void)handle;
    s_state.task_exited = 1;
}
