#pragma once

#include "power_sim.h"

/*!
 * @brief  Read instantaneous solar generation power from the EPS.
 *
 * Sums the per-axis panel power as reported by the EPS ADCs.
 *
 * @param[out] w_out  Receives the total instantaneous panel power
 *                    delivered to the EPS bus, in Watts. Always
 *                    non-negative; reads ~0 W during eclipse. Left
 *                    untouched on a failed EPS read.
 * @return  HAL_OK on success, otherwise the HAL_StatusTypeDef
 *          returned by the underlying EPS_R_Panels call.
 */
HAL_StatusTypeDef EPS_R_InstGen(float *w_out);
