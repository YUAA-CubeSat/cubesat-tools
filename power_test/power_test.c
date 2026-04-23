/*!
 * @file power_test.c
 * @brief Single-scenario driver that demonstrates every parameter you
 *        can tweak to exercise power_dist.c off-target.
 *
 * The point of this file is to show the full "interface" between a
 * test and the power-distribution module:
 *
 *   1. PowerSim_Config_t  – all the simulated-hardware inputs you
 *                           can script BEFORE running the task.
 *   2. PowerDist_X_Task() – the unit-under-test. In the real firmware
 *                           it runs forever under FreeRTOS; here it
 *                           returns when the sim forces an EPS error
 *                           at cfg.stop_after_minutes.
 *   3. PowerSim_State_t   – everything the sim observed WHILE the
 *                           task ran (battery voltage, how many times
 *                           each stub was called, which level the
 *                           task last commanded, etc.).
 *   4. PowerDist_Test*()  – direct read-only handles on the module's
 *                           internal static state (consumption buffer,
 *                           cached SoC, current orbit phase) so you
 *                           can assert on internals after the run.
 *
 * To add more scenarios: copy `run_scenario()`, change the config,
 * call it from main(). No other files need to change.
 */

#include "power_dist.h"
#include "power_sim.h"

#include <stdio.h>
#include <string.h>

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
    printf("  solar_gen_W         = %.2f W        (panel power while in sun)\n",
           (double)c->solar_gen_W);
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

static const char *phase_name(ORBIT_PHASE p)
{
    switch (p) {
    case PHASE_UNKNOWN: return "UNKNOWN";
    case PHASE_DARK:    return "DARK";
    case PHASE_SUN:     return "SUN";
    case PHASE_ERR:     return "ERR";
    default:            return "?";
    }
}

static void print_state_and_internals(void)
{
    const PowerSim_State_t              *s   = PowerSim_GetState();
    const PowerDist_ConsumptionBuffer_t  *cb = PowerDist_TestGetConsBuf();

    printf("\nSimulation outputs (PowerSim_State_t):\n");
    printf("  elapsed_minutes     = %u min\n", s->elapsed_minutes);
    printf("  vbatt (final)       = %.3f V\n", (double)s->vbatt);
    printf("  current_level       = L%d\n",    (int)s->current_level);
    printf("  eps_call_count      = %u\n",     s->eps_call_count);
    printf("  sun_call_count      = %u\n",     s->sun_call_count);
    printf("  set_level_count     = %u\n",     s->set_level_count);
    printf("  panic_count         = %u\n",     s->panic_count);
    printf("  task_exited         = %d\n",     s->task_exited);

    printf("\nPowerDist internal state (via PowerDist_Test* hooks):\n");
    printf("  battChg_cache       = %.3f %%\n", (double)PowerDist_TestGetBattChgCache());
    printf("  currPhase           = %s\n",    phase_name(PowerDist_TestGetCurrPhase()));
    printf("  consumptionBuffer.len   = %u\n", cb->len);
    printf("  consumptionBuffer.index = %u\n", cb->index);
    printf("  consumptionBuffer.five_min_consumption[] =");
    for (int i = 0; i < POWERDIST_CONSBUF_NUM_ELTS; ++i) {
        if (i % 6 == 0) printf("\n    ");
        printf(" %+8.3f", (double)cb->five_min_consumption[i]);
    }
    printf("\n");
}

int main(void)
{
    printf("YUAA CubeSat – power_dist single-scenario driver\n");
    printf("=================================================\n\n");

    /* ---- 1. Build the full scenario configuration. -------------- */
    /* Every field of PowerSim_Config_t is listed explicitly so it's
     * obvious which inputs drive the module. Change any value and
     * rerun `make run` to see how the task responds.              */
    PowerSim_Config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.initial_vbatt       = 4.0f;            /* ≈ 77 %% SoC at start       */
    cfg.capacity_wmin       = 10.2f * 60.0f;   /* matches EPS_CAPACITY       */
    cfg.sun_minutes         = 60;              /* 1 hour sun per orbit       */
    cfg.dark_minutes        = 30;              /* 30 min eclipse per orbit   */
    cfg.solar_gen_W         = 6.0f;            /* panel power while in sun   */
    cfg.pwrlvl_draw_W[L0]   = 0.5f;
    cfg.pwrlvl_draw_W[L1]   = 1.0f;
    cfg.pwrlvl_draw_W[L2]   = 2.0f;
    cfg.pwrlvl_draw_W[L3]   = 4.0f;
    cfg.pwrlvl_draw_W[L4]   = 7.0f;            /* satellite busy at L4       */
    cfg.fail_eps_on_call    = 0;               /* no injected EPS fault      */
    cfg.fail_sun_on_call    = 0;               /* no injected SunDetect err. */
    cfg.stop_after_minutes  = 120;             /* run for 2 orbits = 120 min */

    print_config(&cfg);

    /* ---- 2. Reset the simulator to that config. ----------------- */
    PowerSim_Reset(&cfg);

    /* ---- 3. Run the unit under test. ---------------------------- */
    /* The task loops internally. It terminates when the sim returns
     * HAL_ERROR from EPS_R_Voltages, which happens automatically
     * once cfg.stop_after_minutes is reached.                     */
    PowerDist_X_Task(NULL);

    /* ---- 4. Read back what happened. ---------------------------- */
    print_state_and_internals();

    return 0;
}
