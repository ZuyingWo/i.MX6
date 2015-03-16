/*
 * Copyright 2011-2013 Bluetechnix GmbH. All Rights Reserved.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Defines for Bluetechnix ISM-MT9M024 camera module
 * Author: Harald Krapfenbauer
 */

#ifndef __ISM_MT9M024_H__
#define __ISM_MT9M024_H__

// default
#define APT_MT9M024_DEFAULT_FPS                 45
#define APT_MT9M024_DEFAULT_PIXEL		0x672
#define APT_MT9M024_DEFAULT_LINES		0x3de
#define APT_MT9M024_CLOCK			50000000
#define APT_MT9M024_DEFAULT_LUMA_TARGET         0x666
#define APT_MT9M024_MAX_FPS                     (us_PllLutMax*1000000/(APT_MT9M024_DEFAULT_PIXEL*APT_MT9M024_DEFAULT_LINES))
#define APT_MT9M024_MIN_FPS                     (us_PllLutMin*1000000/(APT_MT9M024_DEFAULT_PIXEL*APT_MT9M024_DEFAULT_LINES))


// max resolution
#define APT_MT9M024_MAX_X_RES                   1280
#define APT_MT9M024_MAX_Y_RES                   960

// default values for windowing
#define APT_MT9M024_Y_ADDR_START_DEFAULT        0x2
#define APT_MT9M024_X_ADDR_START_DEFAULT        0x0
#define APT_MT9M024_Y_ADDR_END_DEFAULT          0x3C1
#define APT_MT9M024_X_ADDR_END_DEFAULT          0x4FF

// register definitions
#define APT_MT9M024_MODEL_ID_                   0x3000
#define APT_MT9M024_Y_ADDR_START_               0x3002
#define APT_MT9M024_X_ADDR_START_               0x3004
#define APT_MT9M024_Y_ADDR_END_                 0x3006
#define APT_MT9M024_X_ADDR_END_                 0x3008
#define APT_MT9M024_FRAME_LENGTH_LINES_         0x300a
#define APT_MT9M024_LINE_LENGTH_PCK_            0x300c
#define APT_MT9M024_REVISION_NUMBER             0x300e
#define APT_MT9M024_LOCK_CONTROL                0x3010
#define APT_MT9M024_COARSE_INTEGRATION_TIME_    0x3012
#define APT_MT9M024_FINE_INTEGRATION_TIME_      0x3014
#define APT_MT9M024_COARSE_INTEGRATION_TIME_CB  0x3016
#define APT_MT9M024_FINE_INTEGRATION_TIME_CB    0x3018
#define APT_MT9M024_RESET_REGISTER              0x301A
#define APT_MT9M024_DATA_PEDESTAL_              0x301E
#define APT_MT9M024_GPI_STATUS                  0x3026
#define APT_MT9M024_ROW_SPEED                   0x3028
#define APT_MT9M024_VT_PIX_CLK_DIV              0x302A
#define APT_MT9M024_VT_SYS_CLK_DIV              0x302C
#define APT_MT9M024_PRE_PLL_CLK_DIV             0x302E
#define APT_MT9M024_PLL_MULTIPLIER              0x3030
#define APT_MT9M024_DIGITAL_BINNING             0x3032
#define APT_MT9M024_FRAME_COUNT_                0x303A
#define APT_MT9M024_FRAME_STATUS                0x303C
#define APT_MT9M024_READ_MODE                   0x3040
#define APT_MT9M024_DARK_CONTROL                0x3044
#define APT_MT9M024_FLASH                       0x3046
#define APT_MT9M024_GREEN1_GAIN                 0x3056
#define APT_MT9M024_BLUE_GAIN                   0x3058
#define APT_MT9M024_RED_GAIN                    0x305A
#define APT_MT9M024_GREEN2_GAIN                 0x305C
#define APT_MT9M024_GLOBAL_GAIN                 0x305E
#define APT_MT9M024_EMBEDDED_DATA_CTRL          0x3064
#define APT_MT9M024_DATAPATH_SELECT             0x306E
#define APT_MT9M024_TEST_PATTERN_MODE_          0x3070
#define APT_MT9M024_TEST_DATA_RED_              0x3072
#define APT_MT9M024_TEST_DATA_GREENR_           0x3074
#define APT_MT9M024_TEST_DATA_BLUE_             0x3076
#define APT_MT9M024_TEST_DATA_GREENB_           0x3078
#define APT_MT9M024_TEST_RAW_MODE               0x307A
#define APT_MT9M024_EXPOSURE_T2                 0x307C
#define APT_MT9M024_EXPOSURE_T3                 0x3080
#define APT_MT9M024_OPERATION_MODE_CTRL         0x3082
#define APT_MT9M024_OPERATION_MODE_CTRL_CB      0x3084
#define APT_MT9M024_SEQ_DATA_PORT               0x3086
#define APT_MT9M024_SEQ_CTRL_PORT               0x3088
#define APT_MT9M024_X_ADDR_START_CB             0x308A
#define APT_MT9M024_Y_ADDR_START_CB             0x308C
#define APT_MT9M024_X_ADDR_END_CB               0x308E
#define APT_MT9M024_Y_ADDR_END_CB               0x3090
#define APT_MT9M024_X_EVEN_INC_                 0x30A0
#define APT_MT9M024_X_ODD_INC_                  0x30A2
#define APT_MT9M024_Y_EVEN_INC_                 0x30A4
#define APT_MT9M024_Y_ODD_INC_                  0x30A6
#define APT_MT9M024_Y_ODD_INC_CB                0x30A8
#define APT_MT9M024_FRAME_LENGTH_LINES_CB       0x30AA
#define APT_MT9M024_EXPOSURE_T1                 0x30AC
#define APT_MT9M024_DIGITAL_TEST                0x30B0
#define APT_MT9M024_TEMPSENS_DATA               0x30B2
#define APT_MT9M024_TEMPSENS_CTRL               0x30B4
#define APT_MT9M024_DIGITAL_CTRL                0x30BA
#define APT_MT9M024_GREEN1_GAIN_CB              0x30BC
#define APT_MT9M024_BLUE_GAIN_CB                0x30BE
#define APT_MT9M024_RED_GAIN_CB                 0x30C0
#define APT_MT9M024_GREEN2_GAIN_CB              0x30C2
#define APT_MT9M024_GLOBAL_GAIN_CB              0x30C4
#define APT_MT9M024_TEMPSENS_CALIB1             0x30C6
#define APT_MT9M024_TEMPSENS_CALIB2             0x30C8
#define APT_MT9M024_TEMPSENS_CALIB3             0x30CA
#define APT_MT9M024_TEMPSENS_CALIB4             0x30CC
#define APT_MT9M024_COLUMN_CORRECTION           0x30D4
#define APT_MT9M024_GAIN_OFFSET_CTRL            0x30EA
#define APT_MT9M024_AE_CTRL_REG                 0x3100
#define APT_MT9M024_AE_LUMA_TARGET_REG          0x3102
#define APT_MT9M024_AE_HIST_TARGET_REG          0x3104
#define APT_MT9M024_AE_HYSTERESIS_REG           0x3106
#define APT_MT9M024_AE_MIN_EV_STEP_REG          0x3108
#define APT_MT9M024_AE_MAX_EV_STEP_REG          0x310A
#define APT_MT9M024_AE_DAMP_OFFSET_REG          0x310C
#define APT_MT9M024_AE_DAMP_GAIN_REG            0x310E
#define APT_MT9M024_AE_DAMP_MAX_REG             0x3110
#define APT_MT9M024_AE_DCG_EXPOSURE_HIGH_REG    0x3112
#define APT_MT9M024_AE_DCG_EXPOSURE_LOW_REG     0x3114
#define APT_MT9M024_AE_DCG_GAIN_FACTOR_REG      0x3116
#define APT_MT9M024_AE_DCG_GAIN_FACTOR_INV_REG  0x3118
#define APT_MT9M024_AE_MAX_EXPOSURE_REG         0x311C
#define APT_MT9M024_AE_MIN_EXPOSURE_REG         0x311E
#define APT_MT9M024_AE_LOW_MEAN_TARGET_REG      0x3120
#define APT_MT9M024_AE_HIST_LOW_THRESH_REG      0x3122
#define APT_MT9M024_AE_DARK_CUR_THRESH_REG      0x3124
#define APT_MT9M024_AE_ALPHA_V1_REG             0x3126
#define APT_MT9M024_AE_ALPHA_COEF_REG           0x3128
#define APT_MT9M024_AE_CURRENT_GAINS            0x312A
#define APT_MT9M024_AE_ROI_X_START_OFFSET       0x3140
#define APT_MT9M024_AE_ROI_Y_START_OFFSET       0x3142
#define APT_MT9M024_AE_ROI_X_SIZE               0x3144
#define APT_MT9M024_AE_ROI_Y_SIZE               0x3146
#define APT_MT9M024_AE_HIST_BEGIN_PERC          0x3148
#define APT_MT9M024_AE_HIST_END_PERC            0x314A
#define APT_MT9M024_AE_HIST_DIV                 0x314C
#define APT_MT9M024_AE_NORM_WIDTH_MIN           0x314E
#define APT_MT9M024_AE_MEAN_H                   0x3150
#define APT_MT9M024_AE_MEAN_L                   0x3152
#define APT_MT9M024_AE_HIST_BEGIN_H             0x3154
#define APT_MT9M024_AE_HIST_BEGIN_L             0x3156
#define APT_MT9M024_AE_HIST_END_H               0x3158
#define APT_MT9M024_AE_HIST_END_L               0x315A
#define APT_MT9M024_AE_HIST_END_MEAN_H          0x315C
#define APT_MT9M024_AE_HIST_END_MEAN_L          0x315E
#define APT_MT9M024_AE_PERC_LOW_END             0x3160
#define APT_MT9M024_AE_NORM_ABS_DEV             0x3162
#define APT_MT9M024_AE_COARSE_INTEGRATION_TIME  0x3164
#define APT_MT9M024_AE_AG_EXPOSURE_HI           0x3166
#define APT_MT9M024_AE_AG_EXPOSURE_LO           0x3168
#define APT_MT9M024_AE_AG_GAIN1                 0x316A
#define APT_MT9M024_AE_AG_GAIN2                 0x316C
#define APT_MT9M024_AE_AG_GAIN3                 0x316E
#define APT_MT9M024_AE_INV_AG_GAIN1             0x3170
#define APT_MT9M024_AE_INV_AG_GAIN2             0x3172
#define APT_MT9M024_AE_INV_AG_GAIN3             0x3174
#define APT_MT9M024_DELTA_DK_CONTROL            0x3180
#define APT_MT9M024_DELTA_DK_CLIP               0x3182
#define APT_MT9M024_DELTA_DK_T1                 0x3184
#define APT_MT9M024_DELTA_DK_T2                 0x3186
#define APT_MT9M024_DELTA_TK_T3                 0x3188
#define APT_MT9M024_HDR_MC_CTRL1                0x318A
#define APT_MT9M024_HDR_MC_CTRL2                0x318C
#define APT_MT9M024_HDR_MC_CTRL3                0x318E
#define APT_MT9M024_HDR_MC_CTRL4                0x3190
#define APT_MT9M024_HDR_MC_CTRL5                0x3192
#define APT_MT9M024_HDR_MC_CTRL6                0x3194
#define APT_MT9M024_HDR_MC_CTRL7                0x3196
#define APT_MT9M024_HDR_MC_CTRL8                0x3198
#define APT_MT9M024_HDR_COMP_KNEE1              0x319A
#define APT_MT9M024_HDR_COMP_KNEE2              0x319C
#define APT_MT9M024_HDR_MC_CTRL9                0x319E
#define APT_MT9M024_HDR_MC_CTRL10               0x31A0
#define APT_MT9M024_HDR_MC_CTRL11               0x31A2
#define APT_MT9M024_HISPI_TIMING                0x31C0
#define APT_MT9M024_HISPI_CONTROL_STATUS        0x31C6
#define APT_MT9M024_HISPI_CRC_0                 0x31C8
#define APT_MT9M024_HISPI_CRC_1                 0x31CA
#define APT_MT9M024_HISPI_CRC_2                 0x31CC
#define APT_MT9M024_HISPI_CRC_3                 0x31CE
#define APT_MT9M024_HDR_COMP                    0x31D0
#define APT_MT9M024_STAT_FRAME_ID               0x31D2
#define APT_MT9M024_I2C_WRT_CHECKSUM            0x31D6
#define APT_MT9M024_PIX_DEF_ID                  0x31E0
#define APT_MT9M024_PIX_DEF_ID_BASE_RAM         0x31E2
#define APT_MT9M024_PIX_DEF_ID_STREAM_RAM       0x31E4
#define APT_MT9M024_PIX_DEF_RAM_RD_ADDR         0x31E6
#define APT_MT9M024_HORIZONAL_CURSOR_POSITION_  0x31E8
#define APT_MT9M024_VERTICAL_CURSOR_POSITION_   0x31EA
#define APT_MT9M024_HORIZONTAL_CURSOR_WIDTH_    0x31EC
#define APT_MT9M024_VERTICAL_CURSOR_WIDTH_      0x31EE
#define APT_MT9M024_FUSE_ID1                    0x31F4
#define APT_MT9M024_FUSE_ID2                    0x31F6
#define APT_MT9M024_FUSE_ID3                    0x31F8
#define APT_MT9M024_FUSE_ID4                    0x31FA
#define APT_MT9M024_I2C_IDS                     0x31FC
#define APT_MT9M024_DAC_LD_24_25                0x3EE4
#define APT_MT9M024_BIST_BUFFERS_CONTROL1       0x3FD0
#define APT_MT9M024_BIST_BUFFERS_CONTROL2       0x3FD2
#define APT_MT9M024_BIST_BUFFERS_STATUS1        0x3FD4
#define APT_MT9M024_BIST_BUFFERS_STATUS2        0x3FD6
#define APT_MT9M024_BIST_BUFFERS_DATA1          0x3FD8
#define APT_MT9M024_BIST_BUFFERS_DATA2          0x3FDA

#endif
