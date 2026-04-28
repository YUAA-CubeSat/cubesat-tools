#include "power_dist.h"
#include "power_sim.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "power_dist.c"
#include "power_sim.c"
#include "eps_extra.c"

static const char *phase_name(ORBIT_PHASE p)
{
    switch (p) {
    case PHASE_UNKNOWN: return "UNK";
    case PHASE_DARK:    return "DARK";
    case PHASE_SUN:     return "SUN";
    case PHASE_ERR:     return "ERR";
    default:            return "?";
    }
}

static void print_config(const PowerSim_Config_t *c)
{
    printf("Simulation inputs (PowerSim_Config_t):\n");
    printf("  initial_vbatt       = %.3f V       (starting battery voltage)\n",
           (double)c->initial_vbatt);
    printf("  capacity_wmin       = %.1f W*min    (battery capacity)\n",
           (double)c->capacity_wmin);
    printf("  sun_minutes         = %u min        (length of SUN phase per orbit)\n",
           c->sun_minutes);
    printf("  dark_minutes        = %u min        (length of DARK phase per orbit)\n",
           c->dark_minutes);
    printf("  solar_gen_W         = %.2f W        (raw panel capability in sun)\n",
           (double)c->solar_gen_W);
    printf("  eoc_voltage         = %.2f V        (charger CV cut-off, EPS Table 1)\n",
           (double)c->eoc_voltage);
    printf("  charge_current_A    = %.3f A       (CC current limit, charge mode)\n",
           (double)c->charge_current_A);
    printf("  pwrlvl_draw_W       = L0:%.2f L1:%.2f L2:%.2f L3:%.2f L4:%.2f W\n",
           (double)c->pwrlvl_draw_W[L0], (double)c->pwrlvl_draw_W[L1],
           (double)c->pwrlvl_draw_W[L2], (double)c->pwrlvl_draw_W[L3],
           (double)c->pwrlvl_draw_W[L4]);
    printf("  fail_eps_on_call    = %u           (0 = never, N = Nth EPS read returns HAL_ERROR)\n",
           c->fail_eps_on_call);
    printf("  fail_sun_on_call    = %u           (0 = never, N = Nth SunDetect returns PHASE_ERR)\n",
           c->fail_sun_on_call);
    printf("  stop_after_minutes  = %u min        (sim forces task to exit at this deadline)\n",
           c->stop_after_minutes);
}

static void print_final_state(void)
{
    const PowerSim_State_t              *s  = PowerSim_GetState();
    const PowerDist_ConsumptionBuffer_t *cb = &s_pd_state.cons_buf;

    printf("\nSimulation outputs (PowerSim_State_t):\n");
    printf("  elapsed_minutes     = %u min\n", s->elapsed_minutes);
    printf("  vbatt (final)       = %.3f V\n", (double)s->vbatt);
    printf("  current_level       = L%d\n",    (int)s->current_level);
    printf("  eps_call_count      = %u\n",     s->eps_call_count);
    printf("  sun_call_count      = %u\n",     s->sun_call_count);
    printf("  set_level_count     = %u\n",     s->set_level_count);
    printf("  panic_count         = %u\n",     s->panic_count);
    printf("  task_exited         = %d\n",     s->task_exited);

    printf("\nPowerDist internal state (s_pd_state):\n");
    printf("  currPhase              = %s\n", phase_name(s_pd_state.curr_phase));
    printf("  consumptionBuffer.len  = %u\n", cb->len);
    printf("  consumptionBuffer.head = %u\n", cb->head);
    printf("  consumptionBuffer.samples_W[] =");
    for (int i = 0; i < POWERDIST_CONSBUF_NUM_ELTS; ++i) {
        if (i % 6 == 0) printf("\n    ");
        printf(" %+8.3f", (double)cb->samples_W[i]);
    }
    printf("\n");
}

static void print_trace_header(void)
{
    printf("\n--- per-minute trace (logged at each vTaskDelay call) ---\n");
    printf("Columns:\n");
    printf("  tick     s_pd_state.tick_min at end of iteration\n");
    printf("  sim      PowerSim elapsed_minutes (sim clock)\n");
    printf("  vbatt    V_Batt cached by CRITICAL_CHECK this tick\n");
    printf("  soc%%     SoC derived from vbatt via PowerDist_GetSoC\n");
    printf("  phase    s_pd_state.curr_phase AFTER step_sun_tracker\n");
    printf("  accDur   curr_phase_acc.duration_min (includes trailing darks)\n");
    printf("  tdark    trailing_dark_min (debounce counter)\n");
    printf("  accWmin  curr_phase_acc.total_gen_Wmin (running integral)\n");
    printf("  lfv      last_full_valid flag\n");
    printf("  lfDur    last_full_phase.duration_min (pure sun minutes)\n");
    printf("  lfWmin   last_full_phase.total_gen_Wmin\n");
    printf("  avgW     solar_avg_W_orbit_averaged() (what CONSUME_DECIDE sees)\n");
    printf("  lvl      current power level commanded to SysMan\n");
    printf("  cLen     consumption ring-buffer length\n");
    printf("\n");
    printf("%4s %4s %7s %6s %5s %6s %5s %8s %3s %5s %8s %7s %3s %4s\n",
           "tick", "sim", "vbatt", "soc%", "phase",
           "accDur", "tdark", "accWmin",
           "lfv", "lfDur", "lfWmin", "avgW",
           "lvl", "cLen");
    printf("---- ---- ------- ------ ----- ------ ----- -------- --- ----- -------- ------- --- ----\n");
}

static void log_tick(void)
{
    const PowerSim_State_t *s   = PowerSim_GetState();
    const SolarPhase_t     *acc = &s_pd_state.curr_phase_acc;
    const SolarPhase_t     *lf  = &s_pd_state.last_full_phase;
    const float             avg = solar_avg_W_orbit_averaged();
    const float             vb  = s_pd_state.batt_V_tick.V_Batt;
    const float             soc = PowerDist_GetSoC(vb);

    printf("%4u %4u %7.3f %6.2f %5s %6u %5u %8.2f %3d %5u %8.2f %7.3f L%d %4u\n",
           s_pd_state.tick_min,
           s->elapsed_minutes,
           (double)vb,
           (double)soc,
           phase_name(s_pd_state.curr_phase),
           (unsigned)acc->duration_min,
           (unsigned)s_pd_state.trailing_dark_min,
           (double)acc->total_gen_Wmin,
           (int)s_pd_state.last_full_valid,
           (unsigned)lf->duration_min,
           (double)lf->total_gen_Wmin,
           (double)avg,
           (int)s->current_level,
           (unsigned)s_pd_state.cons_buf.len);
}

void vTaskDelay(uint32_t ticks_ms)
{
    uint32_t minutes = ticks_ms / (60u * 1000u);
    if (minutes == 0) minutes = 1;

    for (uint32_t i = 0; i < minutes; ++i) {
        log_tick();
        PowerSim_AdvanceOneMinute();
    }
}

typedef struct {
    const char       *name;
    const char       *description;
    PowerSim_Config_t cfg;
    Sys_PowerLevel_e start_level;
} Scenario_t;

#define DRAW_L0_W   0.400f
#define DRAW_L1_W   0.733f
#define DRAW_L2_W   1.000f
#define DRAW_L3_W   1.667f
#define DRAW_L4_W   2.800f

static void scenario_base_cfg(PowerSim_Config_t *c)
{
    memset(c, 0, sizeof(*c));
    c->initial_vbatt       = 3.9f;                    /* ~58.87% SoC        */
    c->capacity_wmin       = EPS_BATT_CAPACITY_WMIN;  /* 10.2 Wh (CDR sl.3) */
    c->sun_minutes         = 60;                      /* context L315       */
    c->dark_minutes        = 30;                      /* 90-min orbit       */
    c->pwrlvl_draw_W[L0]   = DRAW_L0_W;
    c->pwrlvl_draw_W[L1]   = DRAW_L1_W;
    c->pwrlvl_draw_W[L2]   = DRAW_L2_W;
    c->pwrlvl_draw_W[L3]   = DRAW_L3_W;
    c->pwrlvl_draw_W[L4]   = DRAW_L4_W;
    /* Charger model: EPS I defaults (User Manual Table 1). */
    c->eoc_voltage         = 4.10f;   /* End-of-charge cut-off        */
    c->charge_current_A    = 0.460f;  /* Mode 2 default (230/460/690) */
    c->fail_eps_on_call    = 0;
    c->fail_sun_on_call    = 0;
}

static void run_scenario(const Scenario_t *sc)
{
    printf("\n");
    printf("==================================================================\n");
    printf("Scenario: %s\n", sc->name);
    printf("------------------------------------------------------------------\n");
    printf("%s\n", sc->description);
    printf("==================================================================\n");

    memset(&s_pd_state, 0, sizeof(s_pd_state));

    print_config(&sc->cfg);
    PowerSim_Reset(&sc->cfg);
    if (sc->start_level < NumPwrLvls) {
        Sys_X_SetPowerLevel(sc->start_level);
        printf("  start_level         = L%d (override; default is L4)\n",
               (int)sc->start_level);
    }
    print_trace_header();

    PowerDist_X_Task(NULL);

    print_final_state();
}

static Scenario_t scenarios_build(int which)
{
    Scenario_t s;
    scenario_base_cfg(&s.cfg);
    s.start_level = NumPwrLvls;   /* default: use sim's boot level (L4) */

    switch (which) {
    case 0:
        s.name = "S1 nominal tidal-lock w/ spin (2.7 Wh/orbit harvest)";
        s.description =
            "  solar_gen_W = 4.00 W in sun -> orbit-avg 2.67 W.\n"
            "  L4 draws 2.80 W -> deficit ~0.13 W.\n"
            "  Expected: at the first decision tick (~tick 90) the ladder\n"
            "  drops L4 -> L3. Thereafter, surplus at L3 is 1.00 W, which is\n"
            "  below the 1.133 * 1.25 = 1.42 W needed to climb back: stable.";
        s.cfg.solar_gen_W        = 4.00f;
        s.cfg.stop_after_minutes = 300;   /* ~3 orbits */
        break;
    case 1:
        s.name = "S2 rich harvest climb (6 W panel, start at L2)";
        s.description =
            "  solar_gen_W = 6.00 W raw panel capability in sun. The EPS\n"
            "  charger caps absorption at charge_current_A * V_batt \n"
            "  (Mode 2: ~1.84 W @ 4.0 V), so the MPPT throttles delivered\n"
            "  power to load + charge_accepted. start_level = L2 (override).\n"
            "\n"
            "  Expected at first decision (tick 90):\n"
            "    avgW ~ (load + charge_accepted) * 60/90\n"
            "         = (1.00 + 1.79) * 0.667 ~ 1.86 W\n"
            "    cons ~ 1.00 W -> surplus ~ 0.86 W.\n"
            "    L2 -> L3 climb needs 0.667*1.25 = 0.83 W  (fits, climbs).\n"
            "    L3 -> L4 climb needs 1.133*1.25 = 1.42 W (does not fit).\n"
            "  So ladder lands on L3, not L4. Demonstrates that CC charger\n"
            "  limits the orbit-averaged harvest even when panels are rich.";
        s.cfg.solar_gen_W        = 6.00f;
        s.cfg.stop_after_minutes = 300;
        s.start_level            = L2;
        break;
    case 2:
        s.name = "S3 lean month / edge-on orbit (1.33 W orbit-avg)";
        s.description =
            "  solar_gen_W = 2.00 W in sun -> orbit-avg 1.33 W.\n"
            "  L4 draws 2.80 W -> deficit ~1.47 W.\n"
            "  Expected: single decision walks L4 -> L3 -> L2 in one step\n"
            "  (recovered 1.133+0.667 = 1.80 W covers the deficit), then\n"
            "  sits on the GLOBAL_PWRLVL_FLOOR at L2.";
        s.cfg.solar_gen_W        = 2.00f;
        s.cfg.stop_after_minutes = 300;
        break;
    case 3:
        s.name = "S4 danger-zone blackout (phase harvest < 36 W*min)";
        s.description =
            "  solar_gen_W = 0.50 W in sun -> phase harvest 30 W*min,\n"
            "  below DANGER_ZONE_PHASE_WMIN (36). Even L0 is net-deficit.\n"
            "  Expected: once the first sun phase closes and the ring fills,\n"
            "  the danger-zone short-circuit forces L0 (bypasses the L2 floor).";
        s.cfg.solar_gen_W        = 0.50f;
        s.cfg.stop_after_minutes = 220;
        break;
    default:
        s.name        = "(invalid)";
        s.description = "";
        s.cfg.stop_after_minutes = 0;
    }
    return s;
}

#define NUM_SCENARIOS  4

int main(void)
{
    for (int i = 0; i < NUM_SCENARIOS; ++i) {
        Scenario_t sc = scenarios_build(i);
        run_scenario(&sc);
    }

    return 0;
}
