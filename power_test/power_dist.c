/*!
 * @file   power_dist.c
 * @brief  Power Distribution module.
 *
 *   Every tick (1 min):
 *     - Critical SoC check -> force L0 + reset state if below threshold.
 *     - Sun-phase tracker: integrate EPS_R_InstGen on SUN ticks, debounce
 *       SUN->DARK with SUN_PHASE_TRAILING_DARK_MIN, publish closed phases
 *       to last_full_phase. Clamp to ORBIT_MINUTES on over-long phases.
 *
 *   Every 5th tick (ALGO_PERIOD_MINUTES):
 *     - Push one 5-min consumption sample into the ring buffer.
 *     - Once the ring spans a full orbit, short-circuit to L0 on
 *       (a) no sun ever seen or (b) last phase < DANGER_ZONE_PHASE_WMIN;
 *       otherwise walk the marginal-W ladder via decide_base_level().
 *     - On level change: call Sys_X_SetPowerLevel + clear the ring.
 */

#include "power_dist.h"
#include "power_sim.h"
#include "eps_extra.h"

#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/*! Nominal orbit length in minutes.
 *  Source: Blender & STK Power Estimate
 */
#define ORBIT_MINUTES                  (90u)

/*! Nominal sun-phase duration in minutes. Used for extrapolation when we
 *  have a partial in-progress phase but no completed one yet.
 *  Source: Blender & STK Power Estimate (Sun phase durations)
 */
#define SUN_PHASE_NOMINAL_MINUTES      (60u)

/*! Period at which we sample consumption and run the level-decision algo.
 *  Source: Jira Power Distribution  
 */
#define ALGO_PERIOD_MINUTES            (5u)

/*! Task wake period in milliseconds. One tick == one minute. */
#define TASK_TICK_MS                   (60u * 1000u)

/*! Consecutive DARK readings required to declare the current sun phase ended */
#define SUN_PHASE_TRAILING_DARK_MIN    (10u)

/*! Ring-buffer depth = ORBIT_MINUTES / ALGO_PERIOD_MINUTES = 18 slots.
 *  Exposed so the test harness can size expectations against it. */
#define POWERDIST_CONSBUF_NUM_ELTS  (ORBIT_MINUTES / ALGO_PERIOD_MINUTES)

/*! Circular buffer of 5-min average consumption samples (Watts). */
typedef struct {
    float   samples_W[POWERDIST_CONSBUF_NUM_ELTS];
    uint8_t head;
    uint8_t len;
} PowerDist_ConsumptionBuffer_t;

_Static_assert(TASK_TICK_MS == 60u * 1000u,
               "Power-dist loop math assumes a 1-minute task tick.");
_Static_assert((ORBIT_MINUTES % ALGO_PERIOD_MINUTES) == 0u,
               "ORBIT_MINUTES must be a whole multiple of ALGO_PERIOD_MINUTES.");
_Static_assert(POWERDIST_CONSBUF_NUM_ELTS == (ORBIT_MINUTES / ALGO_PERIOD_MINUTES),
               "CONSBUF_SLOTS out of sync with orbit / algo period.");


/*! Nameplate battery capacity. Source: EPS_User_Manual */
#define EPS_BATT_CAPACITY_WH           (10.2f)
#define EPS_BATT_CAPACITY_WMIN         (EPS_BATT_CAPACITY_WH * 60.0f)

/*! Critical SoC, below which we force L0 immediately */
#define CRITICAL_SOC_PERCENT           (5.0f)

/*! Lowest level the ladder may recommend under normal surplus/deficit.
 *  Only the critical SoC path is allowed to go below this. 
 */
#define GLOBAL_PWRLVL_FLOOR            (L2)

/*! Extra margin for upward level change to account for ~25% inefficiency */
#define PWRLVL_CLIMB_HEADROOM_FRAC     (0.25f)

/*! Danger-zone threshold (W min per closed sun phase). Below this, even L0
 *  is net-deficit, so we force L0 to stretch freeze margin.
 *  Derivation: 0.6 Wh/orbit (L0 demand) * 60 min/h = 36 W min. 
 */
#define DANGER_ZONE_PHASE_WMIN         (36.0f)

/*! Cumulative per-orbit energy demand (Wh/orbit). 
 * Source: CDR Slide 10.
 */
static const float PWRLVL_CUMULATIVE_WH_PER_ORBIT[NumPwrLvls] = {
    [L0] = 0.6f,   /* EPS + SS baseline */
    [L1] = 1.1f,   /* + OBC CPU         */
    [L2] = 1.5f,   /* + TCV (radio)     */
    [L3] = 2.5f,   /* + MTQ (detumble)  */
    [L4] = 4.2f,   /* + CRD             */
};

/*! Orbit-averaged W saved by dropping Li -> Li-1 */
static const float PWRLVL_MARGINAL_W[NumPwrLvls] = {
    [L0] = 0.000f,  /* unused */
    [L1] = 0.333f,  /* 0.5 Wh/orbit */
    [L2] = 0.267f,  /* 0.4 Wh/orbit */
    [L3] = 0.667f,  /* 1.0 Wh/orbit */
    [L4] = 1.133f,  /* 1.7 Wh/orbit */
};

// TBR: taken from old source.
/*! Battery voltage -> SoC table */
struct {
    float volts;
    float soc_pct;
} static const SOC_TABLE[] = {
    { 3.0f,   0.00f },
    { 3.5f,   2.16f },
    { 3.6f,   3.57f },
    { 3.7f,   8.98f },
    { 3.8f,  23.15f },
    { 3.9f,  58.87f },
    { 4.0f,  77.91f },
    { 4.1f,  90.91f },
    { 4.2f, 100.00f }
};

#define SOC_TABLE_LEN  (sizeof(SOC_TABLE) / sizeof(SOC_TABLE[0]))

/*! Sun-phase accumulator. duration_min excludes trailing darks (they're
 *  subtracted at close_phase); total_gen_Wmin is the raw integral. */
typedef struct {
    uint16_t duration_min;
    float    total_gen_Wmin;
} SolarPhase_t;

/*! Task-level state machine.
 *    - CRITICAL_CHECK, SUN_TRACK : 1-min cadence (every tick)
 *    - CONSUME_DECIDE            : 5-min cadence (tick_min % 5 == 0)
 *
 *      INIT ──► CRITICAL_CHECK ──► SUN_TRACK ─┬─► CONSUME_DECIDE ─► SLEEP ─┐
 *                 │   │                       │                            │
 *                 │   │           non-5-min ──┴─────────────────► SLEEP    │
 *                 │   │                                             ▲      │
 *                 │   └──────── SoC < crit ────────────► SLEEP      │      │
 *                 │                                         ▲       │      │
 *                 └── EPS unrecoverable ──► EXIT            └─ next tick ──┘
 */
typedef enum {
    PD_STATE_INIT,              /*!< zero module state                         */
    PD_STATE_CRITICAL_CHECK,    /*!< read SoC; on critical force L0 + reset    */
    PD_STATE_SUN_TRACK,         /*!< read SunDetect, feed sun-phase tracker    */
    PD_STATE_CONSUME_DECIDE,    /*!< sample consumption; decide at 5-min gate  */
    PD_STATE_SLEEP,             /*!< vTaskDelay(TASK_TICK_MS); ++tick_min      */
    PD_STATE_EXIT               /*!< unrecoverable EPS error, leave for(;;)    */
} PowerDist_State_e;

/*! Task state */
typedef struct {
    PowerDist_ConsumptionBuffer_t cons_buf;

    /*!< Monotonic minute counter; gates the 5-min decision boundary
     *   without drifting relative to vTaskDelay. */
    uint32_t tick_min;

    float batt_soc_pct_last;        /*!< SoC at previous 5-min sample      */
    bool  batt_soc_valid;           /*!< true once we have a prior sample  */

    /*!< Per-tick voltage scratch: written by CRITICAL_CHECK, read by
     *   CONSUME_DECIDE. No _valid flag needed — CONSUME_DECIDE is only
     *   reachable via CRITICAL_CHECK, so the write always precedes. */
    EPS_VoltageType batt_V_tick;

    ORBIT_PHASE  curr_phase;
    SolarPhase_t curr_phase_acc;    /*!< integral for in-progress phase    */
    SolarPhase_t last_full_phase;   /*!< most recent closed phase          */
    bool         last_full_valid;   /*!< true once any phase has closed    */
    uint8_t      trailing_dark_min; /*!< SUN->DARK debounce counter        */

    /*!< Monotonic gen integral (W·min). cum_gen_Wmin_last is the snapshot
     *   taken at the previous 5-min sample, so CONSUME_DECIDE can net
     *   out gen from the SoC delta in its window. */
    float cum_gen_Wmin;
    float cum_gen_Wmin_last;
} PowerDist_t;

static PowerDist_t s_pd_state;

static void  ring_clear(PowerDist_ConsumptionBuffer_t *const r);
static void  ring_push (PowerDist_ConsumptionBuffer_t *const r, float sample_W);
static float ring_mean (const PowerDist_ConsumptionBuffer_t *const r);
static float ring_sum  (const PowerDist_ConsumptionBuffer_t *const r);
static bool  ring_full (const PowerDist_ConsumptionBuffer_t *const r);

static void  solar_phase_on_sun_tick (const float inst_gen_W);
static void  solar_phase_on_dark_tick(void);
static void  close_phase(void); 
static float solar_avg_W_orbit_averaged(void);

static float cons_sample_W_from_soc_delta(const float soc_before_pct,
                                          const float soc_after_pct,
                                          const float gen_Wmin_in_window);

static Sys_PowerLevel_e decide_base_level(const Sys_PowerLevel_e curr,
                                          const float avg_cons_W,
                                          const float avg_gen_W);

static void        reset_all_state(void);
static void        step_sun_tracker(ORBIT_PHASE observed);
static void        step_consumption_and_decide(const float battery_soc_pct_now);
static ORBIT_PHASE pd_run_sun_detect(void);

/* ======================== public API ========================= */

float PowerDist_GetSoC(const float voltage)
{
    if (voltage <= SOC_TABLE[0].volts) {
        return SOC_TABLE[0].soc_pct;
    }
    if (voltage >= SOC_TABLE[SOC_TABLE_LEN - 1].volts) {
        return SOC_TABLE[SOC_TABLE_LEN - 1].soc_pct;
    }

    uint8_t i = 1;
    while (voltage >= SOC_TABLE[i].volts) {
        i++;
    }

    const float v_lo = SOC_TABLE[i - 1].volts;
    const float v_hi = SOC_TABLE[i].volts;
    const float s_lo = SOC_TABLE[i - 1].soc_pct;
    const float s_hi = SOC_TABLE[i].soc_pct;

    return s_lo + (voltage - v_lo) * (s_hi - s_lo) / (v_hi - v_lo);
}

HAL_StatusTypeDef PowerDist_IsBattCritical(uint8_t *const isCrit, EPS_VoltageType *const V)
{
    HAL_StatusTypeDef EPS_stat;
    EPS_VoltageType temp_V;

    Sys_TryDev(EPS_stat, EPS_R_Voltages(&temp_V), HAL_OK, DEV_EPS);
    if (EPS_stat == HAL_OK)
    {
        const float SoC = PowerDist_GetSoC(temp_V.V_Batt);
        
        if (isCrit != NULL) {
            *isCrit = SoC < CRITICAL_SOC_PERCENT;
        }
        if (V != NULL) {
            *V = temp_V;
        }
    }

    return EPS_stat;
}

void PowerDist_X_Task(void *argument)
{
    (void)argument;

    PowerDist_State_e state = PD_STATE_INIT;

    while (state != PD_STATE_EXIT) {
        switch (state) {
            case PD_STATE_INIT: {
                reset_all_state();
                state = PD_STATE_CRITICAL_CHECK;
                break;
            }

            case PD_STATE_CRITICAL_CHECK: {
                /* Read + cache V_Batt for CONSUME_DECIDE to reuse later. */
                uint8_t is_crit = 0;
                const HAL_StatusTypeDef st = PowerDist_IsBattCritical(&is_crit, &s_pd_state.batt_V_tick);
                if (st != HAL_OK) {
                    state = PD_STATE_EXIT;
                    break;
                }
                if (is_crit == 1) {
                    Sys_X_SetPowerLevel(L0);
                    reset_all_state();
                    state = PD_STATE_SLEEP;
                } else {
                    state = PD_STATE_SUN_TRACK;
                }
                break;
            }

            case PD_STATE_SUN_TRACK: {
                step_sun_tracker(pd_run_sun_detect());
                /* 5-min gate. tick_min=0 is a boundary; first call just
                 * seeds batt_soc_pct_last (gated by batt_soc_valid). */
                state = ((s_pd_state.tick_min % ALGO_PERIOD_MINUTES) == 0)
                        ? PD_STATE_CONSUME_DECIDE
                        : PD_STATE_SLEEP;
                break;
            }

            case PD_STATE_CONSUME_DECIDE: {
                /* V_Batt already cached by CRITICAL_CHECK, no EPS read. */
                const float soc_pct = PowerDist_GetSoC(s_pd_state.batt_V_tick.V_Batt);
                step_consumption_and_decide(soc_pct);
                state = PD_STATE_SLEEP;
                break;
            }

            case PD_STATE_SLEEP: {
                vTaskDelay(pdMS_TO_TICKS(TASK_TICK_MS));
                s_pd_state.tick_min++;
                state = PD_STATE_CRITICAL_CHECK;
                break;
            }
            case PD_STATE_EXIT: { /* unreachable */ break; }
        }
    }

    vTaskDelete(NULL);
}

/* ======================= internal helpers ==================== */

static void ring_clear(PowerDist_ConsumptionBuffer_t *const r)
{
    memset(r->samples_W, 0, sizeof(r->samples_W));
    r->head = 0;
    r->len  = 0;
}

static void ring_push(PowerDist_ConsumptionBuffer_t *const r, float sample_W)
{
    r->samples_W[r->head] = sample_W;
    r->head = (uint8_t)((r->head + 1u) % POWERDIST_CONSBUF_NUM_ELTS);
    if (r->len < POWERDIST_CONSBUF_NUM_ELTS) {
        r->len++;
    }
}

static float ring_sum(const PowerDist_ConsumptionBuffer_t *const r)
{
    float s = 0.0f;
    for (uint8_t i = 0; i < r->len; ++i) {
        s += r->samples_W[i];
    }
    return s;
}

static float ring_mean(const PowerDist_ConsumptionBuffer_t *const r)
{
    if (r->len == 0)
    {
        return 0.0f;
    }
    return ring_sum(r) / (float)r->len;
}

static bool ring_full(const PowerDist_ConsumptionBuffer_t *const r)
{
    return r->len == POWERDIST_CONSBUF_NUM_ELTS;
}

/* --- state machine --------------------------------------------------- */

/* Clear algorithmic state (cons history, SoC memory, sun tracking) but
 * preserve:
 *   - tick_min: resetting it would re-phase the 5-min gate and re-trigger
 *               on every tick until it advanced past 0.
 *   - batt_V_tick: overwritten every tick by CRITICAL_CHECK; preserve
 *                  vs. zero is a no-op, kept for clarity.
 * Called from PD_STATE_INIT and from the critical-SoC path. */
static void reset_all_state(void)
{
    const uint32_t        saved_tick_min   = s_pd_state.tick_min;
    const EPS_VoltageType saved_batt_V     = s_pd_state.batt_V_tick;

    memset(&s_pd_state, 0, sizeof(s_pd_state));

    s_pd_state.tick_min          = saved_tick_min;
    s_pd_state.batt_V_tick       = saved_batt_V;
    s_pd_state.batt_soc_pct_last = FLT_MAX;
    s_pd_state.batt_soc_valid    = false;
    s_pd_state.curr_phase        = PHASE_UNKNOWN;
    s_pd_state.last_full_valid   = false;

    ring_clear(&s_pd_state.cons_buf);
}

/* PD_STATE_SUN_TRACK helpers: turn 1-min ORBIT_PHASE observations into
 * closed SolarPhase_t records. A phase closes on
 *   (a) SUN_PHASE_TRAILING_DARK_MIN consecutive DARK ticks, or
 *   (b) the phase has been open >= ORBIT_MINUTES. */

/* Publish curr_phase_acc as last_full_phase and reset. Trailing darks
 * are subtracted so duration_min reflects only sunlight; total_gen_Wmin
 * needs no correction (EPS_R_InstGen returns 0 in DARK). */
static void close_phase(void)
{
    if (s_pd_state.curr_phase_acc.duration_min >= s_pd_state.trailing_dark_min) {
        s_pd_state.curr_phase_acc.duration_min -= s_pd_state.trailing_dark_min;
    } else {
        /* Unreachable given the state machine */
        s_pd_state.curr_phase_acc.duration_min = 0;
    }

    s_pd_state.last_full_phase = s_pd_state.curr_phase_acc;
    s_pd_state.last_full_valid = true;

    s_pd_state.curr_phase_acc.duration_min   = 0;
    s_pd_state.curr_phase_acc.total_gen_Wmin = 0.0f;
    s_pd_state.trailing_dark_min             = 0;
}

static void solar_phase_on_sun_tick(const float inst_gen_W)
{
    s_pd_state.curr_phase_acc.total_gen_Wmin += inst_gen_W;
    s_pd_state.curr_phase_acc.duration_min   += 1;

    /* Mirror into cum_gen_Wmin — the monotonic counter CONSUME_DECIDE
     * uses to net gen out of each 5-min SoC delta. Not reset by
     * close_phase (different cadence than phase tracking). */
    s_pd_state.cum_gen_Wmin      += inst_gen_W;
    s_pd_state.trailing_dark_min  = 0;

    /* Long phase more than 1 orbit, clamp and restart a fresh integral. */
    if (s_pd_state.curr_phase_acc.duration_min >= (uint16_t)ORBIT_MINUTES) {
        s_pd_state.curr_phase_acc.duration_min = ORBIT_MINUTES;
        close_phase();

        /* TODO! TBR */
    }
}

static void solar_phase_on_dark_tick(void)
{
    /* SUN->DARK. curr_phase stays PHASE_SUN until close_phase
     * flips it — flipping early would make step_sun_tracker skip this
     * branch and freeze trailing_dark_min at 1.
     * duration_min keeps ticking so close_phase's subtraction works. */
    s_pd_state.trailing_dark_min           += 1;
    s_pd_state.curr_phase_acc.duration_min += 1;

    if (s_pd_state.trailing_dark_min >= SUN_PHASE_TRAILING_DARK_MIN) {
        close_phase();
        s_pd_state.curr_phase = PHASE_DARK;
    }
}

/* Orbit-averaged harvest for the decision. Prefers the last closed
 * phase; falls back to extrapolating the in-progress phase against
 * SUN_PHASE_NOMINAL_MINUTES; returns 0 if there's nothing at all. */
static float solar_avg_W_orbit_averaged(void)
{
    if (s_pd_state.last_full_valid) {
        return s_pd_state.last_full_phase.total_gen_Wmin / (float)ORBIT_MINUTES;
    }
    if (s_pd_state.curr_phase_acc.duration_min == 0) {
        return 0.0f;
    }

    const float extrapolated_Wmin =
        s_pd_state.curr_phase_acc.total_gen_Wmin *
        ((float)SUN_PHASE_NOMINAL_MINUTES /
         (float)s_pd_state.curr_phase_acc.duration_min);

    return extrapolated_Wmin / (float)ORBIT_MINUTES;
}

/* Consume one ORBIT_PHASE observation.
 *   prev     observed   action
 *   --------------------------------------------------------------
 *   *        ERR,UNK    ignore (miss one sample, retry next minute)
 *   any      SUN        accumulate one sun minute; set curr_phase=SUN
 *   SUN      DARK       trailing-dark debounce tick
 *   UNK/DARK DARK       nothing (no open phase to close)
 * TODO: consider adding a failed-sample counter. */
static void step_sun_tracker(ORBIT_PHASE observed)
{
    if (
        observed == PHASE_ERR || 
        observed == PHASE_UNKNOWN
    ) {
        return;
    }

    if (observed == PHASE_SUN) {
        /* On EPS fault, count the tick as sun with g_W=0 so duration_min
         * stays synchronous with ticks (the extrapolation in
         * solar_avg_W_orbit_averaged depends on it). */
        float g_W = 0.0f;
        HAL_StatusTypeDef st;
        Sys_TryDev(st, EPS_R_InstGen(&g_W), HAL_OK, DEV_EPS);
        if (st != HAL_OK) {
            g_W = 0.0f;
        }
        solar_phase_on_sun_tick(g_W);
        s_pd_state.curr_phase = PHASE_SUN;
    } else {
        /* PHASE_DARK. Only do anything if we had an open SUN phase;
         * UNKNOWN/DARK -> DARK has nothing to close. TBR. */
        if (s_pd_state.curr_phase == PHASE_SUN) {
            solar_phase_on_dark_tick();
        }
    }
}

/* PD_STATE_CONSUME_DECIDE
 *
 * Every 5-min tick:
 *   - Push a consumption sample = (SoC delta over the window, converted
 *     to Wmin, with cum_gen_Wmin subtracted out).
 *   - If the ring is full (spans one orbit), decide:
 *       - force L0 if no sun was ever seen (long blackout), or
 *         last_full_phase < DANGER_ZONE_PHASE_WMIN (even L0 is deficit);
 *       - else run decide_base_level() on ring-mean cons vs
 *         orbit-averaged gen.
 *     Both L0 short-circuits bypass GLOBAL_PWRLVL_FLOOR, like the
 *     critical-SoC path in CRITICAL_CHECK. */

/* 5-min SoC delta -> average consumption (W).
 *   SoC delta is net = gen - draw:
 *     net_Wmin  = (soc_after - soc_before)/100 * EPS_BATT_CAPACITY_WMIN
 *     draw_Wmin = gen_Wmin_in_window - net_Wmin
 *   cons_W = draw_Wmin / ALGO_PERIOD_MINUTES.
 *
 * Sim-only artefact: at 100% SoC the real EPS charger throttles and
 * EPS_R_InstGen reports the lower accepted W, keeping this math
 * consistent; our ideal-panel sim does not, so saturated runs show
 * inflated cons samples. TBR in the harness, not an algo bug. */
static float cons_sample_W_from_soc_delta(
    const float soc_before_pct,
    const float soc_after_pct,
    const float gen_Wmin_in_window
)
{
    const float net_Wmin = (
        (soc_after_pct - soc_before_pct) * 
        (EPS_BATT_CAPACITY_WMIN / 100.0f)
    );
    const float draw_Wmin = gen_Wmin_in_window - net_Wmin;
    return draw_Wmin / (float)ALGO_PERIOD_MINUTES;
}

/* Walk the marginal-W ladder. Driven by surplus_W = avg_gen_W - avg_cons_W:
 *   - Deficit: step down from `curr`, summing PWRLVL_MARGINAL_W[target]
 *     into recovered_W; stop when it covers -surplus_W or we reach
 *     GLOBAL_PWRLVL_FLOOR. No headroom — drops act immediately.
 *   - Surplus: climb only if surplus >= marginal * (1 + HEADROOM_FRAC),
 *     then subtract the *nominal* marginal from remaining_W (the 25%
 *     is a safety buffer, not real extra draw) and retry. Stop at L4.
 * Asymmetry gives hysteresis and implements the CDR's ~25% thermal
 * derate. */
static Sys_PowerLevel_e decide_base_level(
    const Sys_PowerLevel_e curr,
    const float avg_cons_W,
    const float avg_gen_W
)
{
    const float surplus_W = avg_gen_W - avg_cons_W;
    Sys_PowerLevel_e target = curr;

    if (surplus_W < 0.0f) {
        float recovered_W = 0.0f;
        while (target > GLOBAL_PWRLVL_FLOOR) {
            recovered_W += PWRLVL_MARGINAL_W[target];
            target = (Sys_PowerLevel_e)(target - 1);
            if (recovered_W >= -surplus_W) {
                break;
            }
        }
    } else if (surplus_W > 0.0f) {
        float remaining_W = surplus_W;
        while (target < (Sys_PowerLevel_e)(NumPwrLvls - 1)) {
            const Sys_PowerLevel_e next =
                (Sys_PowerLevel_e)(target + 1);
            const float required_W =
                PWRLVL_MARGINAL_W[next] * (1.0f + PWRLVL_CLIMB_HEADROOM_FRAC);
            if (remaining_W < required_W) {
                break;
            }
            remaining_W -= PWRLVL_MARGINAL_W[next];
            target = next;
        }
    }

    return target;
}

static void step_consumption_and_decide(const float battery_soc_pct_now)
{
    /* First sample after INIT or level change: seed the snapshots and
     * return. Next 5-min tick produces the first real sample, so the
     * ring starts clean under the current level. */
    if (!s_pd_state.batt_soc_valid) {
        s_pd_state.batt_soc_pct_last = battery_soc_pct_now;
        s_pd_state.cum_gen_Wmin_last = s_pd_state.cum_gen_Wmin;
        s_pd_state.batt_soc_valid    = true;
        return;
    }

    const float gen_Wmin_in_window = s_pd_state.cum_gen_Wmin - s_pd_state.cum_gen_Wmin_last;
    const float cons_sample_W = cons_sample_W_from_soc_delta(
        s_pd_state.batt_soc_pct_last,
        battery_soc_pct_now,
        gen_Wmin_in_window
    );

    ring_push(&s_pd_state.cons_buf, cons_sample_W);

    s_pd_state.batt_soc_pct_last = battery_soc_pct_now;
    s_pd_state.cum_gen_Wmin_last = s_pd_state.cum_gen_Wmin;

    if (!ring_full(&s_pd_state.cons_buf)) {
        return;
    }

    const Sys_PowerLevel_e curr_level = Sys_GetPowerLevel();
    Sys_PowerLevel_e       new_lvl    = curr_level;

    /* Short-circuits: ring-full guarantees >= ORBIT_MINUTES of wall time,
     * so these two conditions are informative:
     *   - no_gen_ever:     we've been in the dark for a full orbit
     *   - last_phase_dead: last closed phase was too weak to feed L0. 
     */
    const bool no_gen_ever =
        !s_pd_state.last_full_valid &&
        s_pd_state.curr_phase_acc.duration_min == 0;
    const bool last_phase_dead =
        s_pd_state.last_full_valid &&
        s_pd_state.last_full_phase.total_gen_Wmin < DANGER_ZONE_PHASE_WMIN;

    if (no_gen_ever || last_phase_dead) {
        new_lvl = L0;
    } else {
        const float avg_cons_W = ring_mean(&s_pd_state.cons_buf);
        const float avg_gen_W  = solar_avg_W_orbit_averaged();
        new_lvl = decide_base_level(curr_level, avg_cons_W, avg_gen_W);
    }

    if (new_lvl != curr_level) {
        Sys_X_SetPowerLevel(new_lvl);
        /* Start a fresh 90-min window under the new level. Keep the
         * snapshots so the next sample correctly covers the first 5 min
         * under the new draw instead of being skipped by re-seeding. */
        ring_clear(&s_pd_state.cons_buf);
    }
}


/* Orbit phase determiner. TBR: No SunDetect exists yet? 
 * Brainstorm:
 *   1. Primary — sun sensor via ADCS_Meas_GetS(). A non-zero vector
 *      this minute is a hard-positive. A miss does NOT imply DARK.
 *   2. Fallback — solar panel voltages via EPS_R_Panels(). Declare SUN
 *      if any two POPULATED axes exceed a low threshold, or one
 *      populated axis exceeds a higher threshold. On YUAA only the X
 *      and Y axes have panels (+X/-X 4S1P, +Y 3S1P, -Y 4S1P, ±Z bare),
 *      so Z_V is always ~0 V — code that requires "all three axes lit"
 *      will never see sun. Datasheet-derived starting points (Pumpkin
 *      PMDSAS §9, Spectrolab XTJ Prime, Vmp ≈ 0.85·Voc):
 *           one-axis-bright  -> X_V > ~7.5 V or Y_V > ~6.0 V
 *           two-axes-split   -> X_V > ~4.5 V AND Y_V > ~4.0 V
 *           dark consensus   -> X_V < ~1.5 V AND Y_V < ~1.5 V
 *      The Y-axis thresholds run lower than X because +Y has only 3
 *      cells (Vmp ≈ 6.4 V vs 8.5 V) and the parallel-string mismatch
 *      with -Y depresses the MPPT operating point further. Thresholds
 *      TBR (jira:75); I-V curve is non-linear (jira:76), so fit
 *      empirically against on-orbit telemetry rather than from
 *      datasheet V_oc.
 *   3. Sanity — Orbit_GetSatEquatorial + Orbit_GetSunEquatorial plus
 *      an umbral-cone test. Cross-check only; fails silently when TLEs
 *      are stale, RTC has drifted, or the sun_eq SD file is missing. */
static ORBIT_PHASE pd_run_sun_detect(void)
{
    return RunSunDetect();
}
