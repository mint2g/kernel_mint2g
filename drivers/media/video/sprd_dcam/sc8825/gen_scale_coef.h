/******************************************************************************
 ** File Name:      gen_scale_coef.h                                          *
 ** Author:         shan.he                                                   *
 ** DATE:           2011-02-12                                                *
 ** Copyright:      2011 Spreadtrum, Incoporated. All Rights Reserved.        *
 ** Description:                                                              *
 ** Note:                                                      				  *
 *****************************************************************************/
/******************************************************************************
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------* 
 ** DATE              NAME            DESCRIPTION                             * 
 *****************************************************************************/
#ifndef _GEN_SCALE_COEF_H_
#define _GEN_SCALE_COEF_H_

/**---------------------------------------------------------------------------*
 **                         Dependencies                                      *
 **---------------------------------------------------------------------------*/
/**---------------------------------------------------------------------------*
 **                         Macros                                            *
 **---------------------------------------------------------------------------*/
#define SCALER_COEF_TAP_NUM_HOR			48
#define SCALER_COEF_TAP_NUM_VER			68
/**---------------------------------------------------------------------------*
 **                         Compiler Flag                                     *
 **---------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

#include <linux/types.h>

//typedef uint8_t		BOOLEAN;

/**---------------------------------------------------------------------------*
 **                         Public Functions                                  *
 **---------------------------------------------------------------------------*/
/****************************************************************************/
/* Purpose:	generate scale factor           							    */
/* Author:																	*/
/* Input:                                                                   */
/*          i_w:	source image width                                      */
/*          i_h:	source image height                                  	*/
/*          o_w:    target image width                                      */
/*          o_h:    target image height                						*/
/* Output:	    															*/
/*          coeff_h_ptr: pointer of horizontal coefficient buffer, the size of which must be at  */
/*					   least SCALER_COEF_TAP_NUM_HOR * 4 bytes				*/
/*					  the output coefficient will be located in coeff_h_ptr[0], ......,   */	
/*						coeff_h_ptr[SCALER_COEF_TAP_NUM_HOR-1]				*/
/*			coeff_v_ptr: pointer of vertical coefficient buffer, the size of which must be at      */
/*					   least (SCALER_COEF_TAP_NUM_VER + 1) * 4 bytes	    */
/*					  the output coefficient will be located in coeff_v_ptr[0], ......,   */	
/*					  coeff_h_ptr[SCALER_COEF_TAP_NUM_VER-1] and the tap number */
/*					  will be located in coeff_h_ptr[SCALER_COEF_TAP_NUM_VER] */
/*          temp_buf_ptr: temp buffer used while generate the coefficient   */
/*          temp_buf_size: temp buffer size, 6k is the suggest size         */
/* Return:					                    							*/  
/* Note:                                                                    */
/****************************************************************************/
uint8_t Dcam_GenScaleCoeff(int16_t	i_w, int16_t i_h, int16_t o_w,  int16_t o_h, 
					       uint32_t* coeff_h_ptr, uint32_t* coeff_v_ptr,
                           void *temp_buf_ptr, uint32_t temp_buf_size);

/**---------------------------------------------------------------------------*
 **                         Compiler Flag                                     *
 **---------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif
