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
#ifndef _SC8810_REG_ISP_H_
#define _SC8810_REG_ISP_H_

#ifdef __BIG_ENDIAN
typedef struct _ISP_REG_T {
	union _DCAM_CFG_TAG {
		struct _DCAM_CFG_MAP {
			volatile uint32_t reserved:25;
			volatile uint32_t path2_clock_status:1;
			volatile uint32_t path1_clock_status:1;
			volatile uint32_t path2_clock_switch:1;
			volatile uint32_t path1_clock_switch:1;
			volatile uint32_t review_path_eb:1;
			volatile uint32_t cam_path2_eb:1;
			volatile uint32_t cam_path1_eb:1;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_cfg_u;

	union _DCAM_PATH_CFG_TAG {
		struct _DCAM_PATH_CFG_MAP {
			volatile uint32_t reserved:16;
			volatile uint32_t cam1_uv420_avg_eb:1;
			volatile uint32_t auto_copy_cap:1;
			volatile uint32_t frc_copy_cap:1;
			volatile uint32_t cam1_dither_eb:1;
			volatile uint32_t cam1_odata_format:2;
			volatile uint32_t cam2_deci_eb:1;
			volatile uint32_t cam1_trim_eb:1;
			volatile uint32_t ver_down_tap:4;
			volatile uint32_t scale_bypass:1;
			volatile uint32_t cam1_deci_eb:1;
			volatile uint32_t cap_mode:1;
			volatile uint32_t cap_eb:1;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_path_cfg_u;

	union _DCAM_SRC_SIZE_TAG {
		struct _DCAM_SRC_SIZE_MAP {
			volatile uint32_t reserved1:4;
			volatile uint32_t src_size_y:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t src_size_x:12;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_src_size_u;

	union _DCAM_DES_SIZE_TAG {
		struct _DCAM_DES_SIZE_MAP {
			volatile uint32_t reserved1:4;
			volatile uint32_t des_size_y:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t des_size_x:12;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_des_size_u;

	union _DCAM_TRIM_START_TAG {
		struct _DCAM_TRIM_START_MAP {
			volatile uint32_t reserved1:4;
			volatile uint32_t start_y:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t start_x:12;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_trim_start_u;

	union _DCAM_TRIM_SIZE_TAG {
		struct _DCAM_TRIM_SIZE_MAP {
			volatile uint32_t reserved1:4;
			volatile uint32_t size_y:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t size_x:12;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_trim_size_u;

	union _REV_PATH_CFG_TAG {
		struct _REV_PATH_CFG_MAP {
			volatile uint32_t reserved1:5;
			volatile uint32_t rot_mode:2;
			volatile uint32_t rot_eb:1;
			volatile uint32_t reserved0:5;
			volatile uint32_t uv420_avg_eb:1;
			volatile uint32_t ver_down_tap:4;
			volatile uint32_t rgb_input_format:1;
			volatile uint32_t yuv_input_format:2;
			volatile uint32_t sub_sample_mode:2;
			volatile uint32_t dither_eb:1;
			volatile uint32_t output_format:2;
			volatile uint32_t input_format:1;
			volatile uint32_t scale_mode:1;
			volatile uint32_t scale_bypass:1;
			volatile uint32_t sub_sample_eb:1;
			volatile uint32_t trim_eb:1;
			volatile uint32_t review_start:1;
		} mBits;
		volatile uint32_t dwValue;
	} rev_path_cfg_u;

	union _REV_SRC_SIZE_TAG {
		struct _REV_SRC_SIZE_MAP {
			volatile uint32_t reserved1:4;
			volatile uint32_t src_size_y:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t src_size_x:12;
		} mBits;
		volatile uint32_t dwValue;
	} rev_src_size_u;

	union _REV_DES_SIZE_TAG {
		struct _REV_DES_SIZE_MAP {
			volatile uint32_t reserved1:4;
			volatile uint32_t des_size_y:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t des_size_x:12;
		} mBits;
		volatile uint32_t dwValue;
	} rev_des_size_u;

	union _REV_TRIM_START_TAG {
		struct _REV_TRIM_START_MAP {
			volatile uint32_t reserved1:4;
			volatile uint32_t start_y:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t start_x:12;
		} mBits;
		volatile uint32_t dwValue;
	} rev_trim_start_u;

	union _REV_TRIM_SIZE_TAG {
		struct _REV_TRIM_SIZE_MAP {
			volatile uint32_t reserved1:4;
			volatile uint32_t size_y:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t size_x:12;
		} mBits;
		volatile uint32_t dwValue;
	} rev_trim_size_u;

	union _SLICE_VER_CNT_TAG {
		struct _SLICE_VER_CNT_MAP {
			volatile uint32_t reserved1:4;
			volatile uint32_t slice_line_output:12;
			volatile uint32_t reserved0:3;
			volatile uint32_t last_slice:1;
			volatile uint32_t slice_line_input:12;
		} mBits;
		volatile uint32_t dwValue;
	} slice_ver_cnt_u;

	union _DCAM_INT_STS_TAG {
		struct _DCAM_INT_STS_MAP {
			volatile uint32_t reserved:22;
			volatile uint32_t review_done:1;
			volatile uint32_t jpg_buf_ovf:1;
			volatile uint32_t sensor_frame_err:1;
			volatile uint32_t sensor_line_err:1;
			volatile uint32_t cap_buf_ovf:1;
			volatile uint32_t isp_tx_done:1;
			volatile uint32_t cap_eof:1;
			volatile uint32_t cap_sof:1;
			volatile uint32_t sensor_eof:1;
			volatile uint32_t sensor_sof:1;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_int_stat_u;

	union _DCAM_INT_MASK_TAG {
		struct _DCAM_INT_MASK_MAP {
			volatile uint32_t reserved:22;
			volatile uint32_t review_done:1;
			volatile uint32_t jpg_buf_ovf:1;
			volatile uint32_t sensor_frame_err:1;
			volatile uint32_t sensor_line_err:1;
			volatile uint32_t cap_buf_ovf:1;
			volatile uint32_t isp_tx_done:1;
			volatile uint32_t cap_eof:1;
			volatile uint32_t cap_sof:1;
			volatile uint32_t sensor_eof:1;
			volatile uint32_t sensor_sof:1;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_int_mask_u;

	union _DCAM_INT_CLR_TAG {
		struct _DCAM_INT_CLR_MAP {
			volatile uint32_t reserved:22;
			volatile uint32_t review_done:1;
			volatile uint32_t jpg_buf_ovf:1;
			volatile uint32_t sensor_frame_err:1;
			volatile uint32_t sensor_line_err:1;
			volatile uint32_t cap_buf_ovf:1;
			volatile uint32_t isp_tx_done:1;
			volatile uint32_t cap_eof:1;
			volatile uint32_t cap_sof:1;
			volatile uint32_t sensor_eof:1;
			volatile uint32_t sensor_sof:1;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_int_clr_u;

	union _DCAM_INT_RAW_TAG {
		struct _DCAM_INT_RAW_MAP {
			volatile uint32_t reserved:22;
			volatile uint32_t review_done:1;
			volatile uint32_t jpg_buf_ovf:1;
			volatile uint32_t sensor_frame_err:1;
			volatile uint32_t sensor_line_err:1;
			volatile uint32_t cap_buf_ovf:1;
			volatile uint32_t isp_tx_done:1;
			volatile uint32_t cap_eof:1;
			volatile uint32_t cap_sof:1;
			volatile uint32_t sensor_eof:1;
			volatile uint32_t sensor_sof:1;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_int_raw_u;

	union _FRM_ADDR_0_TAG {
		struct _FRM_ADDR_0_MAP {
			volatile uint32_t frm_addr_0:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_0_u;

	union _FRM_ADDR_1_TAG {
		struct _FRM_ADDR_1_MAP {
			volatile uint32_t frm_addr_1:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_1_u;

	union _FRM_ADDR_2_TAG {
		struct _FRM_ADDR_2_MAP {
			volatile uint32_t frm_addr_2:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_2_u;

	union _FRM_ADDR_3_TAG {
		struct _FRM_ADDR_3_MAP {
			volatile uint32_t frm_addr_3:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_3_u;

	union _FRM_ADDR_4_TAG {
		struct _FRM_ADDR_4_MAP {
			volatile uint32_t frm_addr_4:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_4_u;

	union _FRM_ADDR_5_TAG {
		struct _FRM_ADDR_5_MAP {
			volatile uint32_t frm_addr_5:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_5_u;

	union _FRM_ADDR_6_TAG {
		struct _FRM_ADDR_6_MAP {
			volatile uint32_t frm_addr_6:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_6_u;

	uint32_t reserved_5c;
	union _BURST_GAP_TAG {
		struct _BURST_GAP_MAP {
			volatile uint32_t reserved:26;
			volatile uint32_t ahbm_hold:1;
			volatile uint32_t burst_gap:5;
		} mBits;
		volatile uint32_t dwValue;
	} burst_gap_u;

	union _ENDIAN_SEL_TAG {
		struct _ENDIAN_SEL_MAP {
			volatile uint32_t reserved:20;
			volatile uint32_t dcam_output_endian_uv:2;
			volatile uint32_t dcam_output_endian_y:2;
			volatile uint32_t review_output_endian_uv:2;
			volatile uint32_t review_output_endian_y:2;
			volatile uint32_t review_input_endian_uv:2;
			volatile uint32_t review_input_endian_y:2;
		} mBits;
		volatile uint32_t dwValue;
	} endian_sel_u;

	union _AHBM_STS_TAG {
		struct _AHBM_STS_MAP {
			volatile uint32_t reserved:31;
			volatile uint32_t ahbm_busy:1;
		} mBits;
		volatile uint32_t dwValue;
	} ahbm_sts_u;

	union _FRM_ADDR_7_TAG {
		struct _FRM_ADDR_7_MAP {
			volatile uint32_t frm_addr_7:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_7_u;

	union _FRM_ADDR_8_TAG {
		struct _FRM_ADDR_8_MAP {
			volatile uint32_t frm_addr_8:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_8_u;

	uint32_t reserved_70_100[(0x100 - 0x74) / 4];

	union _CAP_CTRL_TAG {
		struct _CAP_CTRL_MAP {
			volatile uint32_t reserved1:18;
			volatile uint32_t cap_ccir_pd:2;
			volatile uint32_t cap_ccir_rst:1;
			volatile uint32_t cap_if_mode:2;
			volatile uint32_t yuv_type:2;
			volatile uint32_t reserved0:1;
			volatile uint32_t cap_if_endian:1;
			volatile uint32_t vsync_pol:1;
			volatile uint32_t hsync_pol:1;
			volatile uint32_t sensor_mode:2;
			volatile uint32_t ccir_656:1;
		} mBits;
		volatile uint32_t dwValue;
	} cap_ctrl_u;

	union _CAP_FRM_CTRL_TAG {
		struct _CAP_FRM_CTRL_MAP {
			volatile uint32_t reserved1:9;
			volatile uint32_t cap_frm_clr:1;
			volatile uint32_t cap_frm_cnt:6;
			volatile uint32_t reserved0:10;
			volatile uint32_t cap_frm_deci:2;
			volatile uint32_t pre_skip_cnt:4;
		} mBits;
		volatile uint32_t dwValue;
	} cap_frm_ctrl_u;

	union _CAP_START_TAG {
		struct _CAP_START_MAP {
			volatile uint32_t reserved1:4;
			volatile uint32_t start_y:12;
			volatile uint32_t reserved0:3;
			volatile uint32_t start_x:13;
		} mBits;
		volatile uint32_t dwValue;
	} cap_start_u;

	union _CAP_END_TAG {
		struct _CAP_END_MAP {
			volatile uint32_t reserved1:4;
			volatile uint32_t end_y:12;
			volatile uint32_t reserved0:3;
			volatile uint32_t end_x:13;
		} mBits;
		volatile uint32_t dwValue;
	} cap_end_u;

	union _CAP_IMG_DECI_TAG {
		struct _CAP_IMG_DECI_MAP {
			volatile uint32_t reserved:28;
			volatile uint32_t cap_deci_y:2;
			volatile uint32_t cap_deci_x:2;
		} mBits;
		volatile uint32_t dwValue;
	} cap_img_deci_u;

	uint32 atv_mode_fix;

	union _CAP_OBSERVE_TAG {
		struct _CAP_OBSERVE_MAP {
			volatile uint32_t reserved:31;
		volatile uint32_t cap_observe:1} mBits;
		volatile uint32_t dwValue;
	} cap_observe_u;

	union _CAP_JPG_CTL_TAG {
		struct _CAP_JPG_CTL_MAP {
			volatile uint32_t reserved:22;
			volatile uint32_t jpg_buf_size:10;
		} mBits;
		volatile uint32_t dwValue;
	} cap_jpg_ctl_u;

	union _CAP_FRM_SIZE_TAG {
		struct _CAP_FRM_SIZE_MAP {
			volatile uint32_t reserved:8;
			volatile uint32_t cap_frm_size:24;
		} mBits;
		volatile uint32_t dwValue;
	} cap_frm_size_u;

	union _CAP_SPI_CFG_TAG {
		struct _CAP_SPI_CFG_MAP {
			volatile uint32_t reserved:19;
			volatile uint32_t spi_orig_width:13;
		} mBits;
		volatile uint32_t dwValue;
	} cap_spi_cfg_u;
} ISP_REG_T;
#else //little endian
typedef struct _ISP_REG_T {
	union _DCAM_CFG_TAG {
		struct _DCAM_CFG_MAP {
			volatile uint32_t cam_path1_eb:1;
			volatile uint32_t cam_path2_eb:1;
			volatile uint32_t review_path_eb:1;
			volatile uint32_t path1_clock_switch:1;
			volatile uint32_t path2_clock_switch:1;
			volatile uint32_t path1_clock_status:1;
			volatile uint32_t path2_clock_status:1;
			volatile uint32_t reserved:25;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_cfg_u;

	union _DCAM_PATH_CFG_TAG {
		struct _DCAM_PATH_CFG_MAP {
			volatile uint32_t cap_eb:1;
			volatile uint32_t cap_mode:1;
			volatile uint32_t cam1_deci_eb:1;
			volatile uint32_t scale_bypass:1;
			volatile uint32_t ver_down_tap:4;
			volatile uint32_t cam1_trim_eb:1;
			volatile uint32_t cam2_deci_eb:1;
			volatile uint32_t cam1_odata_format:2;
			volatile uint32_t cam1_dither_eb:1;
			volatile uint32_t frc_copy_cap:1;
			volatile uint32_t auto_copy_cap:1;
			volatile uint32_t cam1_uv420_avg_eb:1;
			volatile uint32_t reserved:16;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_path_cfg_u;

	union _DCAM_SRC_SIZE_TAG {
		struct _DCAM_SRC_SIZE_MAP {
			volatile uint32_t src_size_x:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t src_size_y:12;
			volatile uint32_t reserved1:4;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_src_size_u;

	union _DCAM_DES_SIZE_TAG {
		struct _DCAM_DES_SIZE_MAP {
			volatile uint32_t des_size_x:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t des_size_y:12;
			volatile uint32_t reserved1:4;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_des_size_u;

	union _DCAM_TRIM_START_TAG {
		struct _DCAM_TRIM_START_MAP {
			volatile uint32_t start_x:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t start_y:12;
			volatile uint32_t reserved1:4;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_trim_start_u;

	union _DCAM_TRIM_SIZE_TAG {
		struct _DCAM_TRIM_SIZE_MAP {
			volatile uint32_t size_x:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t size_y:12;
			volatile uint32_t reserved1:4;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_trim_size_u;

	union _REV_PATH_CFG_TAG {
		struct _REV_PATH_CFG_MAP {
			volatile uint32_t review_start:1;
			volatile uint32_t trim_eb:1;
			volatile uint32_t sub_sample_eb:1;
			volatile uint32_t scale_bypass:1;
			volatile uint32_t scale_mode:1;
			volatile uint32_t input_format:1;
			volatile uint32_t output_format:2;
			volatile uint32_t dither_eb:1;
			volatile uint32_t sub_sample_mode:2;
			volatile uint32_t yuv_input_format:2;
			volatile uint32_t rgb_input_format:1;
			volatile uint32_t ver_down_tap:4;
			volatile uint32_t uv420_avg_eb:1;
			volatile uint32_t reserved0:5;
			volatile uint32_t rot_eb:1;
			volatile uint32_t rot_mode:2;
			volatile uint32_t reserved1:5;
		} mBits;
		volatile uint32_t dwValue;
	} rev_path_cfg_u;

	union _REV_SRC_SIZE_TAG {
		struct _REV_SRC_SIZE_MAP {
			volatile uint32_t src_size_x:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t src_size_y:12;
			volatile uint32_t reserved1:4;
		} mBits;
		volatile uint32_t dwValue;
	} rev_src_size_u;

	union _REV_DES_SIZE_TAG {
		struct _REV_DES_SIZE_MAP {
			volatile uint32_t des_size_x:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t des_size_y:12;
			volatile uint32_t reserved1:4;
		} mBits;
		volatile uint32_t dwValue;
	} rev_des_size_u;

	union _REV_TRIM_START_TAG {
		struct _REV_TRIM_START_MAP {
			volatile uint32_t start_x:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t start_y:12;
			volatile uint32_t reserved1:4;
		} mBits;
		volatile uint32_t dwValue;
	} rev_trim_start_u;

	union _REV_TRIM_SIZE_TAG {
		struct _REV_TRIM_SIZE_MAP {
			volatile uint32_t size_x:12;
			volatile uint32_t reserved0:4;
			volatile uint32_t size_y:12;
			volatile uint32_t reserved1:4;
		} mBits;
		volatile uint32_t dwValue;
	} rev_trim_size_u;

	union _SLICE_VER_CNT_TAG {
		struct _SLICE_VER_CNT_MAP {
			volatile uint32_t slice_line_input:12;
			volatile uint32_t last_slice:1;
			volatile uint32_t reserved0:3;
			volatile uint32_t slice_line_output:12;
			volatile uint32_t reserved1:4;
		} mBits;
		volatile uint32_t dwValue;
	} slice_ver_cnt_u;

	union _DCAM_INT_STS_TAG {
		struct _DCAM_INT_STS_MAP {
			volatile uint32_t sensor_sof:1;
			volatile uint32_t sensor_eof:1;
			volatile uint32_t cap_sof:1;
			volatile uint32_t cap_eof:1;
			volatile uint32_t isp_tx_done:1;
			volatile uint32_t cap_buf_ovf:1;
			volatile uint32_t sensor_line_err:1;
			volatile uint32_t sensor_frame_err:1;
			volatile uint32_t jpg_buf_ovf:1;
			volatile uint32_t review_done:1;
			volatile uint32_t reserved:22;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_int_stat_u;

	union _DCAM_INT_MASK_TAG {
		struct _DCAM_INT_MASK_MAP {
			volatile uint32_t sensor_sof:1;
			volatile uint32_t sensor_eof:1;
			volatile uint32_t cap_sof:1;
			volatile uint32_t cap_eof:1;
			volatile uint32_t isp_tx_done:1;
			volatile uint32_t cap_buf_ovf:1;
			volatile uint32_t sensor_line_err:1;
			volatile uint32_t sensor_frame_err:1;
			volatile uint32_t jpg_buf_ovf:1;
			volatile uint32_t review_done:1;
			volatile uint32_t reserved:22;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_int_mask_u;

	union _DCAM_INT_CLR_TAG {
		struct _DCAM_INT_CLR_MAP {
			volatile uint32_t sensor_sof:1;
			volatile uint32_t sensor_eof:1;
			volatile uint32_t cap_sof:1;
			volatile uint32_t cap_eof:1;
			volatile uint32_t isp_tx_done:1;
			volatile uint32_t cap_buf_ovf:1;
			volatile uint32_t sensor_line_err:1;
			volatile uint32_t sensor_frame_err:1;
			volatile uint32_t jpg_buf_ovf:1;
			volatile uint32_t review_done:1;
			volatile uint32_t reserved:22;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_int_clr_u;

	union _DCAM_INT_RAW_TAG {
		struct _DCAM_INT_RAW_MAP {
			volatile uint32_t sensor_sof:1;
			volatile uint32_t sensor_eof:1;
			volatile uint32_t cap_sof:1;
			volatile uint32_t cap_eof:1;
			volatile uint32_t isp_tx_done:1;
			volatile uint32_t cap_buf_ovf:1;
			volatile uint32_t sensor_line_err:1;
			volatile uint32_t sensor_frame_err:1;
			volatile uint32_t jpg_buf_ovf:1;
			volatile uint32_t review_done:1;
			volatile uint32_t reserved:22;
		} mBits;
		volatile uint32_t dwValue;
	} dcam_int_raw_u;

	union _FRM_ADDR_0_TAG {
		struct _FRM_ADDR_0_MAP {
			volatile uint32_t frm_addr_0:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_0_u;

	union _FRM_ADDR_1_TAG {
		struct _FRM_ADDR_1_MAP {
			volatile uint32_t frm_addr_1:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_1_u;

	union _FRM_ADDR_2_TAG {
		struct _FRM_ADDR_2_MAP {
			volatile uint32_t frm_addr_2:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_2_u;

	union _FRM_ADDR_3_TAG {
		struct _FRM_ADDR_3_MAP {
			volatile uint32_t frm_addr_3:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_3_u;

	union _FRM_ADDR_4_TAG {
		struct _FRM_ADDR_4_MAP {
			volatile uint32_t frm_addr_4:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_4_u;

	union _FRM_ADDR_5_TAG {
		struct _FRM_ADDR_5_MAP {
			volatile uint32_t frm_addr_5:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_5_u;

	union _FRM_ADDR_6_TAG {
		struct _FRM_ADDR_6_MAP {
			volatile uint32_t frm_addr_6:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_6_u;

	uint32_t reserved_5c;

	union _BURST_GAP_TAG {
		struct _BURST_GAP_MAP {
			volatile uint32_t burst_gap:5;
			volatile uint32_t ahbm_hold:1;
			volatile uint32_t reserved:26;
		} mBits;
		volatile uint32_t dwValue;
	} burst_gap_u;

	union _ENDIAN_SEL_TAG {
		struct _ENDIAN_SEL_MAP {
			volatile uint32_t review_input_endian_y:2;
			volatile uint32_t review_input_endian_uv:2;
			volatile uint32_t review_output_endian_y:2;
			volatile uint32_t review_output_endian_uv:2;
			volatile uint32_t dcam_output_endian_y:2;
			volatile uint32_t dcam_output_endian_uv:2;
			volatile uint32_t reserved:20;
		} mBits;
		volatile uint32_t dwValue;
	} endian_sel_u;

	union _AHBM_STS_TAG {
		struct _AHBM_STS_MAP {
			volatile uint32_t ahbm_busy:1;
			volatile uint32_t reserved:31;
		} mBits;
		volatile uint32_t dwValue;
	} ahbm_sts_u;

	union _FRM_ADDR_7_TAG {
		struct _FRM_ADDR_7_MAP {
			volatile uint32_t frm_addr_7:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_7_u;

	union _FRM_ADDR_8_TAG {
		struct _FRM_ADDR_8_MAP {
			volatile uint32_t frm_addr_8:32;
		} mBits;
		volatile uint32_t dwValue;
	} frm_addr_8_u;

	uint32_t reserved_70_100[(0x100 - 0x74) / 4];

	union _CAP_CTRL_TAG {
		struct _CAP_CTRL_MAP {
			volatile uint32_t ccir_656:1;
			volatile uint32_t sensor_mode:2;
			volatile uint32_t hsync_pol:1;
			volatile uint32_t vsync_pol:1;
			volatile uint32_t cap_if_endian:1;
			volatile uint32_t reserved0:1;
			volatile uint32_t yuv_type:2;
			volatile uint32_t cap_if_mode:2;
			volatile uint32_t cap_ccir_rst:1;
			volatile uint32_t cap_ccir_pd:2;
			volatile uint32_t reserved1:18;
		} mBits;
		volatile uint32_t dwValue;
	} cap_ctrl_u;

	union _CAP_FRM_CTRL_TAG {
		struct _CAP_FRM_CTRL_MAP {
			volatile uint32_t pre_skip_cnt:4;
			volatile uint32_t cap_frm_deci:2;
			volatile uint32_t reserved0:10;
			volatile uint32_t cap_frm_cnt:6;
			volatile uint32_t cap_frm_clr:1;
			volatile uint32_t reserved1:9;
		} mBits;
		volatile uint32_t dwValue;
	} cap_frm_ctrl_u;

	union _CAP_START_TAG {
		struct _CAP_START_MAP {
			volatile uint32_t start_x:13;
			volatile uint32_t reserved0:3;
			volatile uint32_t start_y:12;
			volatile uint32_t reserved1:4;
		} mBits;
		volatile uint32_t dwValue;
	} cap_start_u;

	union _CAP_END_TAG {
		struct _CAP_END_MAP {
			volatile uint32_t end_x:13;
			volatile uint32_t reserved0:3;
			volatile uint32_t end_y:12;
			volatile uint32_t reserved1:4;
		} mBits;
		volatile uint32_t dwValue;
	} cap_end_u;

	union _CAP_IMG_DECI_TAG {
		struct _CAP_IMG_DECI_MAP {
			volatile uint32_t cap_deci_x:2;
			volatile uint32_t cap_deci_y:2;
			volatile uint32_t reserved:28;
		} mBits;
		volatile uint32_t dwValue;
	} cap_img_deci_u;

	uint32_t atv_mode_fix;

	union _CAP_OBSERVE_TAG {
		struct _CAP_OBSERVE_MAP {
			volatile uint32_t cap_observe:1;
			volatile uint32_t reserved:31;
		} mBits;
		volatile uint32_t dwValue;
	} cap_observe_u;

	union _CAP_JPG_CTL_TAG {
		struct _CAP_JPG_CTL_MAP {
			volatile uint32_t jpg_buf_size:10;
			volatile uint32_t reserved:22;
		} mBits;
		volatile uint32_t dwValue;
	} cap_jpg_ctl_u;

	union _CAP_FRM_SIZE_TAG {
		struct _CAP_FRM_SIZE_MAP {
			volatile uint32_t cap_frm_size:24;
			volatile uint32_t reserved:8;
		} mBits;
		volatile uint32_t dwValue;
	} cap_frm_size_u;

	union _CAP_SPI_CFG_TAG {
		struct _CAP_SPI_CFG_MAP {
			volatile uint32_t spi_orig_width:13;
			volatile uint32_t reserved:19;
		} mBits;
		volatile uint32_t dwValue;
	} cap_spi_cfg_u;
} ISP_REG_T;
#endif
#endif
