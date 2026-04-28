#include "eps_extra.h"

HAL_StatusTypeDef EPS_R_InstGen(float *w_out)
{
    if (w_out == NULL) {
        return HAL_ERROR;
    }

    EPS_PanelType panels;
    const HAL_StatusTypeDef st = EPS_R_Panels(&panels);
    if (st != HAL_OK) {
        return st;
    }

    /* Per-axis instantaneous panel power.
     *
     *  EnduroSat EPS-I reports a
     *  single voltage per axis — the +face and -face panels on each
     *  axis are wired in parallel through one MPPT channel — and an
     *  individual current for each face. The product
     *
     *    V_axis * (I_minus + I_plus)
     *
     *  is the power flowing into the EPS bus from that axis at the
     *  MPPT operating point. Summing across all three axes gives the
     *  total instantaneous solar generation, which is exactly the
     *  quantity the power-distribution sun-phase tracker integrates.
     *
     *  Returned value is always non-negative because both the EPS
     *  voltage and current channels are unsigned 
     */
    const float P_x = panels.X_V * (panels.X_minusI + panels.X_plusI);
    const float P_y = panels.Y_V * (panels.Y_minusI + panels.Y_plusI);
    const float P_z = panels.Z_V * (panels.Z_minusI + panels.Z_plusI);

    *w_out = P_x + P_y + P_z;
    return HAL_OK;
}
