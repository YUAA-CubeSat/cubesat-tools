#include "power_dist.h"

#include "power_sim.h"      /* replaces FreeRTOS.h, task.h, EPS.h, SunDetect.h, system_manager.h */

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* INTERNAL DEFINES
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
#define CONSBUF_NUM_ELTS  POWERDIST_CONSBUF_NUM_ELTS   /* 18 slots → 90 min */
#define CONSBUF_MIN_ELTS  CONSBUF_NUM_ELTS

#define EPS_CAPACITY      (10.2f * 60)   /* Watt-minutes (TBR) */
#define EPS_CRIT_SOC      (0.05f)        /* critical battery threshold, fraction */
#define GLOBAL_PWRLVL_FLOOR ((Sys_PowerLevel_e)L2)

#define TASK_PERIOD                    (1 * 60 * 1000)  /* ms */
#define TASK_ALGO_PERIOD_MINUTES       (5)
#define SUN_PHASE_MAX_TRAILING_MINUTES (3)

/*
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* INTERNAL TYPES
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
typedef PowerDist_ConsumptionBuffer_t ConsumptionBuffer;

typedef struct {
    uint8_t duration;   /* minutes */
    float   totalGen;   /* W·min */
} SolarData_t;

/*
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* INTERNAL CONSTANTS
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
static const float PWRLVL_EXP_CONS[NumPwrLvls] = {
    [L0] = 0,
    [L1] = 0,
    [L2] = 0,
    [L3] = 0,
    [L4] = 0,
};

static const float SOC_VOLTAGES[] = { 3.0f, 3.5f, 3.6f, 3.7f, 3.8f, 3.9f, 4.0f, 4.1f, 4.2f };
static const float SOC_VALS[]     = { 0.0f, 2.16f, 3.57f, 8.98f, 23.15f, 58.87f, 77.91f, 90.91f, 100.0f };

/*
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* INTERNAL MEASUREMENTS CACHE
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
static ConsumptionBuffer consumptionBuffer;
static float              battChg_cache;
static ORBIT_PHASE        currPhase;
static SolarData_t        lastFullSolar, currSolar;
static uint8_t            trailingDarkMinutes;

/*
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* INTERNAL ROUTINES
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

static void insertConsumption(float batteryChargeBefore, float batteryChargeAfter) {
    float difference = (batteryChargeAfter - batteryChargeBefore) * EPS_CAPACITY / 5.f;
    consumptionBuffer.five_min_consumption[consumptionBuffer.index] = difference;
    if (consumptionBuffer.len < CONSBUF_NUM_ELTS) consumptionBuffer.len++;
    if (consumptionBuffer.index < CONSBUF_NUM_ELTS - 1) consumptionBuffer.index++;
    else consumptionBuffer.index = 0;
}

static void resetConsBuff(void)
{
    consumptionBuffer.index = 0;
    consumptionBuffer.len = 0;
    memset(&(consumptionBuffer.five_min_consumption), 0,
           sizeof(consumptionBuffer.five_min_consumption));
}

static void discardMeasurements(void) {
    resetConsBuff();
    battChg_cache = FLT_MAX;
    currPhase = PHASE_UNKNOWN;
    memset(&currSolar, 0, sizeof(currSolar));
    trailingDarkMinutes = 0;
}

static void initStaticData(void) {
    discardMeasurements();
    memset(&lastFullSolar, 0, sizeof(lastFullSolar));
}

/* Calculate the mean of all data in the consumption buffer.
 *
 * NOTE: This function carries a pre-existing bug from the production
 * source: when `index == 0' the else-branch sets `ptr = CONSBUF_NUM_ELTS'
 * which is one past the end of the array, so the next iteration reads
 * out of bounds. The test harness can exercise this to reproduce. */
static float calculateBufAvg(void)
{
    float sum = 0; uint8_t num_read = 0;
    for (uint8_t ptr = consumptionBuffer.index;
         num_read < consumptionBuffer.len;
         num_read++, ptr > 0 ? ptr-- : (ptr = CONSBUF_NUM_ELTS))
    {
        sum += consumptionBuffer.five_min_consumption[ptr];
    }
    return (num_read > 0) ? (sum / num_read) : 0.0f;
}

static float calculateTotalConsumption90(void) {
    float averageConsumptionSum = 0;
    for (int i = 0; i < CONSBUF_NUM_ELTS; i++) {
        averageConsumptionSum += consumptionBuffer.five_min_consumption[i];
    }
    return averageConsumptionSum;
}

static Sys_PowerLevel_e calculateBasePwrLvl(Sys_PowerLevel_e currPwrLvl, float avgCons, float avgGen) {

    float power_diff = avgGen - avgCons;
    Sys_PowerLevel_e targetLvl = currPwrLvl;

    if (power_diff < 0) {
        float returned_power = 0;
        while (targetLvl > GLOBAL_PWRLVL_FLOOR) {
            returned_power += PWRLVL_EXP_CONS[targetLvl--];
            if (returned_power > power_diff) break;
        }
    } else {
        float spent_power = 0;
        while (targetLvl < NumPwrLvls) {
            spent_power += PWRLVL_EXP_CONS[++targetLvl];
            if (spent_power > power_diff) {
                targetLvl--;
                break;
            }
        }
    }
    return targetLvl;
}

/*
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* RESTRICTED ROUTINES
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
void PowerDist_X_Task(void *argument)
{
    (void)argument;
    initStaticData();

    HAL_StatusTypeDef EPS_stat; EPS_VoltageType V; float battChg;

    uint8_t minuteCt = 0;

    while (1)
    {
        if (battChg_cache > 100)
        {
            Sys_TryDev(EPS_stat, EPS_R_Voltages(&V), HAL_OK, DEV_EPS);
            if (EPS_stat != HAL_OK) break;
            battChg_cache = PowerDist_GetSoC(V.V_Batt);
            minuteCt = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_PERIOD)); minuteCt++;

        /* Critical check every minute. */
        do
        {
            Sys_TryDev(EPS_stat, EPS_R_Voltages(&V), HAL_OK, DEV_EPS);
            if (EPS_stat != HAL_OK) goto end;
            battChg = PowerDist_GetSoC(V.V_Batt);
            if (battChg < EPS_CRIT_SOC)
            {
                discardMeasurements();
                Sys_X_SetPowerLevel(L0);
                continue;
            }
        } while (0);

        /* Sun check every minute. */
        ORBIT_PHASE newPhase = RunSunDetect();
        if (newPhase == PHASE_SUN)
        {
            trailingDarkMinutes = 0;
            if (currPhase != PHASE_SUN)
            {
                currSolar.duration = 0; currSolar.totalGen = 0;
            }
            /* TODO update currSolar duration and generation integral */
            /* TODO check for case duration >= 90 min */
            currPhase = PHASE_SUN;
        }
        else if (newPhase == PHASE_DARK)
        {
            if (currPhase == PHASE_SUN)
            {
                if (++trailingDarkMinutes >= SUN_PHASE_MAX_TRAILING_MINUTES)
                {
                    currPhase = PHASE_DARK;
                    currSolar.duration -= trailingDarkMinutes;
                    trailingDarkMinutes = 0;
                    memcpy(&lastFullSolar, &currSolar, sizeof(SolarData_t));
                }
                /* else: sun phase maybe not yet over; TODO integrate */
            }
        }
        else panic("unexpected SunDeterm result %d", newPhase);

        /* Distribution algo every 5th minute. */
        if (minuteCt >= TASK_ALGO_PERIOD_MINUTES)
        {
            minuteCt = 0;

            insertConsumption(battChg_cache, battChg);

            if (consumptionBuffer.len >= CONSBUF_MIN_ELTS)
            {
                float consAvg = calculateBufAvg();

                float solarAvg = 0; /* TODO real computation from lastFullSolar */

                Sys_PowerLevel_e currPwrLvl = Sys_GetPowerLevel();
                Sys_PowerLevel_e baseLvl = calculateBasePwrLvl(currPwrLvl, consAvg, solarAvg);

                if (baseLvl != currPwrLvl)
                {
                    resetConsBuff();
                    Sys_X_SetPowerLevel(baseLvl);
                }
            }

            battChg_cache = battChg;
        }
    }
end:
    vTaskDelete(NULL);
}

/*
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* EXTERNAL ROUTINES
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
float PowerDist_GetSoC(float voltage)
{
    if (voltage <= SOC_VOLTAGES[0]) return SOC_VALS[0];
    if (voltage >= SOC_VOLTAGES[8]) return SOC_VALS[8];

    int i = 0;
    while (voltage >= SOC_VOLTAGES[i]) i++;
    float charge = SOC_VALS[i - 1] +
                   (voltage - SOC_VOLTAGES[i - 1]) *
                   (SOC_VALS[i] - SOC_VALS[i - 1]) /
                   (SOC_VOLTAGES[i] - SOC_VOLTAGES[i - 1]);
    return charge;
}

HAL_StatusTypeDef PowerDist_IsBattCritical(uint8_t *isCrit)
{
    HAL_StatusTypeDef EPS_stat; EPS_VoltageType V;
    Sys_TryDev(EPS_stat, EPS_R_Voltages(&V), HAL_OK, DEV_EPS);
    if (EPS_stat == HAL_OK)
    {
        float SoC = PowerDist_GetSoC(V.V_Batt);
        *isCrit = SoC < EPS_CRIT_SOC;
    }
    return EPS_stat;
}

/*
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* TEST HOOKS
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
const PowerDist_ConsumptionBuffer_t *PowerDist_TestGetConsBuf(void)  { return &consumptionBuffer; }
float                                 PowerDist_TestGetBattChgCache(void) { return battChg_cache; }
ORBIT_PHASE                           PowerDist_TestGetCurrPhase(void)    { return currPhase; }

void PowerDist_TestResetState(void) { initStaticData(); }

Sys_PowerLevel_e PowerDist_TestCalcBasePwrLvl(Sys_PowerLevel_e currLvl,
                                              float avgCons,
                                              float avgGen)
{
    return calculateBasePwrLvl(currLvl, avgCons, avgGen);
}

void PowerDist_TestInsertConsumption(float battChgBefore, float battChgAfter)
{
    insertConsumption(battChgBefore, battChgAfter);
}

float PowerDist_TestCalcBufAvg(void)            { return calculateBufAvg(); }
float PowerDist_TestCalcTotalConsumption90(void){ return calculateTotalConsumption90(); }
