/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _GEN_SCALE_COEF_H_
#define _GEN_SCALE_COEF_H_

#include <linux/types.h>

#define SCALER_COEF_TAP_NUM_HOR	48
#define SCALER_COEF_TAP_NUM_VER	68

uint8_t GenScaleCoeff(int16_t i_w, int16_t i_h, int16_t o_w, int16_t o_h,
		      uint32_t * coeff_h_ptr, uint32_t * coeff_v_ptr,
		      void *temp_buf_ptr, uint32_t temp_buf_size);
#endif
