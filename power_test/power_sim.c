#include "power_dist.h" 
#include "power_sim.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


#define SIM_EOC_VOLTAGE_DEFAULT_V      (4.10f)   /* 4.08 min / 4.10 typ / 4.12 max */
#define SIM_CHARGE_CURRENT_DEFAULT_A   (0.460f)  /* Charge Mode 2 (default)        */

static PowerSim_Config_t s_cfg;
static PowerSim_State_t  s_state;
static float s_battery_soc;      
static float s_last_gen_delivered_W;

/* Inverse of PowerDist_GetSoC: SoC(%) -> V_Batt.
 * Reads SOC_TABLE from power_dist.c (visible because of the unity
 * include order in power_test.c). Used to back-map the running SoC
 * integrator onto the voltage that EPS_R_Voltages reports. */
static float sim_soc_to_voltage(float soc_pct)
{
    if (soc_pct <= SOC_TABLE[0].soc_pct) {
        return SOC_TABLE[0].volts;
    }
    if (soc_pct >= SOC_TABLE[SOC_TABLE_LEN - 1].soc_pct) {
        return SOC_TABLE[SOC_TABLE_LEN - 1].volts;
    }

    size_t i = 1;
    while (soc_pct >= SOC_TABLE[i].soc_pct) {
        i++;
    }

    const float s_lo = SOC_TABLE[i - 1].soc_pct;
    const float s_hi = SOC_TABLE[i].soc_pct;
    const float v_lo = SOC_TABLE[i - 1].volts;
    const float v_hi = SOC_TABLE[i].volts;

    return v_lo + (soc_pct - s_lo) * (v_hi - v_lo) / (s_hi - s_lo);
}

typedef struct {
    float delivered_W;
    float batt_W;
} sim_bus_t;

/* CC/CV battery charger + bus balance.
 *   - Below EOC: charger accepts up to I_chg * V_batt W (linear CC).
 *   - At/above EOC: hard off. Real hardware tapers in CV, but for ladder
 *     tests a hard off captures the key behaviour (gen reported drops
 *     to load once the battery is full).
 *   - If panel can't cover load, the battery discharges to make up the
 *     difference; panels deliver everything they can produce.
 * Battery charger efficiency (~90%) and BCR losses are intentionally
 * ignored — the algorithm operates on orbit-averaged power, not
 * instantaneous, and 10% losses are second-order for the ladder. */
static sim_bus_t sim_compute_bus(
    float P_panel_W,
    float P_load_W,
    float V_batt
)
{
    const float I_chg_A = (s_cfg.charge_current_A > 0.0f)
                          ? s_cfg.charge_current_A
                          : SIM_CHARGE_CURRENT_DEFAULT_A;
    const float V_eoc   = (s_cfg.eoc_voltage > 0.0f)
                          ? s_cfg.eoc_voltage
                          : SIM_EOC_VOLTAGE_DEFAULT_V;

    const float P_chg_max_W = (V_batt >= V_eoc) ? 0.0f : (I_chg_A * V_batt);

    float surplus_W = P_panel_W - P_load_W;
    if (surplus_W < 0.0f) surplus_W = 0.0f;

    const float P_chg_W = (P_chg_max_W < surplus_W) ? P_chg_max_W : surplus_W;

    sim_bus_t out;
    if (P_panel_W >= P_load_W) {
        out.delivered_W = P_load_W + P_chg_W;
        out.batt_W      = +P_chg_W;
    } else {
        out.delivered_W = P_panel_W;
        out.batt_W      = -(P_load_W - P_panel_W);
    }
    return out;
}

void PowerSim_AdvanceOneMinute(void)
{
    Sys_PowerLevel_e lvl = s_state.current_level;
    if (lvl >= NumPwrLvls) {
        lvl = (Sys_PowerLevel_e)(NumPwrLvls - 1);
    }
    const float P_load_W  = s_cfg.pwrlvl_draw_W[lvl];
    const float P_panel_W = (PowerSim_CurrentPhase() == PHASE_SUN)
                            ? s_cfg.solar_gen_W : 0.0f;

    const sim_bus_t bus = sim_compute_bus(P_panel_W, P_load_W, s_state.vbatt);
    s_last_gen_delivered_W = bus.delivered_W;

    /* 1 min of batt_W equals that many W·min flowing in/out. */
    const float delta_soc = (s_cfg.capacity_wmin > 0.0f)
                            ? (bus.batt_W / s_cfg.capacity_wmin) * 100.0f
                            : 0.0f;

    s_battery_soc += delta_soc;
    if (s_battery_soc < 0.0f)   s_battery_soc = 0.0f;
    if (s_battery_soc > 100.0f) s_battery_soc = 100.0f;

    s_state.vbatt = sim_soc_to_voltage(s_battery_soc);
    s_state.elapsed_minutes++;
}

void PowerSim_Reset(const PowerSim_Config_t *cfg)
{
    assert(cfg);
    memset(&s_state, 0, sizeof(s_state));
    s_cfg                  = *cfg;
    s_state.current_level  = L4; /* default at boot */
    s_state.vbatt          = s_cfg.initial_vbatt;
    s_battery_soc          = PowerDist_GetSoC(s_state.vbatt);
    s_last_gen_delivered_W = 0.0f;
}

const PowerSim_State_t *PowerSim_GetState(void) { return &s_state; }

ORBIT_PHASE PowerSim_CurrentPhase(void)
{
    uint32_t period = s_cfg.sun_minutes + s_cfg.dark_minutes;
    if (period == 0) return PHASE_SUN; /* default sun-only world */
    uint32_t t = s_state.elapsed_minutes % period;
    return (t < s_cfg.sun_minutes) ? PHASE_SUN : PHASE_DARK;
}

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

/* Mock of the flight EPS_R_Panels. Back-computes a per-axis V/I pattern
 * whose V*(I- + I+) sum exactly equals the bus delivered_W for the
 * current state. That keeps EPS_R_InstGen (in eps_extra.c, which is
 * the real flight-portable implementation built on top of this) numerically
 * equivalent to the older sim that returned delivered_W directly.
 *
 * Distribution model: split the harvest evenly across the +X/+Y/+Z
 * faces at a fixed panel voltage. Real on-orbit illumination is much
 * less symmetric, but:
 *   - the algorithm only consumes the sum, so split shape doesn't bias
 *     any decisions;
 *   - exercising all three axis multiplications in EPS_R_InstGen
 *     catches per-axis typos in tests;
 *   - the planned panel-voltage SunDetect fallback (power_dist.c §
 *     pd_run_sun_detect) is keyed off "any axis above threshold", which
 *     this satisfies during sun and fails during dark.
 *
 * The -faces stay at zero amps. Treating the sat as having one
 * shadowed half is closer to flight than splitting +/- evenly. */
HAL_StatusTypeDef EPS_R_Panels(EPS_PanelType *retval)
{
    s_state.eps_call_count++;
    if (s_cfg.stop_after_minutes &&
        s_state.elapsed_minutes >= s_cfg.stop_after_minutes) {
        return HAL_ERROR;
    }
    if (s_cfg.fail_eps_on_call && s_cfg.fail_eps_on_call == s_state.eps_call_count) {
        return HAL_ERROR;
    }
    if (retval == NULL) return HAL_ERROR;

    /* Recompute against the current state so callers get a fresh answer
     * that reflects whatever the power level / orbit phase / V_batt are
     * at call time, not the value cached at the end of the previous
     * PowerSim_AdvanceOneMinute(). Matches the EPS flight behaviour
     * where EPS_R_Panels polls the ADCs on demand. */
    Sys_PowerLevel_e lvl = s_state.current_level;
    if (lvl >= NumPwrLvls) {
        lvl = (Sys_PowerLevel_e)(NumPwrLvls - 1);
    }
    const float P_load_W  = s_cfg.pwrlvl_draw_W[lvl];
    const float P_panel_W = (PowerSim_CurrentPhase() == PHASE_SUN)
                            ? s_cfg.solar_gen_W : 0.0f;
    const sim_bus_t bus   = sim_compute_bus(P_panel_W, P_load_W, s_state.vbatt);

    memset(retval, 0, sizeof(*retval));

    if (bus.delivered_W > 0.0f) {
        /* YUAA panel layout (S25 CDR):
         *   +X face:  4S1P  (4 cells in series)
         *   -X face:  4S1P
         *   +Y face:  3S1P  (only 3 cells fit because of XYZ harness /
         *                     deployable-antenna keep-out on this face)
         *   -Y face:  4S1P
         *    Z axis:  no panels (±Z host antennas / GGB / payload)
         * Total: 15 Spectrolab CIC XTJ Prime cells across 4 panels.
         *
         * Cell DC params (Pumpkin PMDSAS Design Guidelines rev E2, 2021,
         * §1 + §9 / Table 15):
         *   Voc ≈ 2.5 V/cell, Isc ≈ 400 mA, ~1 W BOL AM0.
         * Triple-junction Vmp ≈ 0.85 · Voc, so:
         *   4S1P: Voc 10.0 V, Vmp ≈ 8.5 V, ~4 W BOL
         *   3S1P: Voc  7.5 V, Vmp ≈ 6.4 V, ~3 W BOL
         *
         * Per-axis EPS voltage readback (assuming EnduroSat EPS-I default
         * per-axis MPPT, with both faces of an axis paralleled through
         * blocking diodes into a single MPPT input):
         *   X axis: both faces are 4S1P, matched. MPPT sits at 4S1P Vmp.
         *           -> X_V ≈ 8.5 V during sun.
         *   Y axis: +Y is 3S1P, -Y is 4S1P -- string-length mismatch!
         *           The MPPT picks one operating point for both. Above
         *           3S1P Voc (7.5 V) the +Y diode reverse-biases and only
         *           -Y contributes; near 6.4 V both contribute but -Y is
         *           well off its MPP. The optimal point is somewhere in
         *           between. We model this as ~7.5 V (the 3S1P Voc knee),
         *           which is the lowest V at which only -Y conducts and
         *           is also where total power is roughly maximised under
         *           the mismatch.  TBR with hardware test data.
         *   Z axis: no panels -> Z_V stays at 0 V always. Code that polls
         *           the Z axis for SunDetect MUST tolerate this (i.e.
         *           don't require *all three* axes above threshold; use
         *           "any populated axis above threshold" instead).
         *
         * Per-face current allocation: split bus.delivered_W across the
         * four real panels by cell-count weight, since BOL Imp is the
         * same for every Spectrolab CIC (~0.46 A) and a longer string
         * just gives more power, not more current. Weights: +X 4/15,
         * -X 4/15, +Y 3/15, -Y 4/15.  EPS_R_InstGen still recovers the
         * correct total because P_axis = V_axis * (I- + I+) with these
         * V/I assignments sums exactly to bus.delivered_W:
         *   P_X = 8.5 * (P*4/15 + P*4/15)/8.5 = P*8/15
         *   P_Y = 7.5 * (P*4/15 + P*3/15)/7.5 = P*7/15
         *   P_Z = 0 * 0                       = 0
         *   total = P. */
        const float V_x = 8.5f;
        const float V_y = 7.5f;
        const float P_total = bus.delivered_W;
        const float P_xp = P_total * (4.0f / 15.0f);
        const float P_xm = P_total * (4.0f / 15.0f);
        const float P_yp = P_total * (3.0f / 15.0f);
        const float P_ym = P_total * (4.0f / 15.0f);
        retval->X_V      = V_x;
        retval->Y_V      = V_y;
        retval->Z_V      = 0.0f;
        retval->X_plusI  = P_xp / V_x;
        retval->X_minusI = P_xm / V_x;
        retval->Y_plusI  = P_yp / V_y;
        retval->Y_minusI = P_ym / V_y;
        retval->Z_plusI  = 0.0f;
        retval->Z_minusI = 0.0f;
    }
    return HAL_OK;
}

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
}

ORBIT_PHASE RunSunDetect(void)
{
    s_state.sun_call_count++;
    if (s_cfg.fail_sun_on_call && s_cfg.fail_sun_on_call == s_state.sun_call_count) {
        return PHASE_ERR;
    }
    return PowerSim_CurrentPhase();
}

void vTaskDelete(void *handle)
{
    (void)handle;
    s_state.task_exited = 1;
}
