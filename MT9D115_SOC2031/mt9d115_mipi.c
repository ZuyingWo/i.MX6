/*
 * drivers/media/video/mt9d115_mipi.c
 *
 * Aptina MT9D115 sensor driver
 *
 * Copyright (C) 2013 Aptina Imaging
 * 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/videodev2.h>
#include <linux/sysfs.h>

#include <linux/fsl_devices.h>
 #include <mach/mipi_csi2.h>
#include <media/v4l2-int-device.h>
#include <media/v4l2-chip-ident.h>
#include "mxc_v4l2_capture.h"
#include "mt9d115_mipi.h"

#define MT9D115_PREVIEW_WIDTH			800
#define MT9D115_PREVIEW_HEIGHT			600
#define MT9D115_CAPTURE_WIDTH			1600
#define MT9D115_CAPTURE_HEIGHT			1200
#define MT9D115_DEFAULT_PIXEL_FORMAT    V4L2_PIX_FMT_YUYV

#define	MT9D115_ROW_START_MIN			0
#define	MT9D115_ROW_START_MAX			MT9D115_PREVIEW_HEIGHT
#define	MT9D115_ROW_START_DEF			0
#define	MT9D115_COLUMN_START_MIN		0
#define	MT9D115_COLUMN_START_MAX		MT9D115_PREVIEW_WIDTH
#define	MT9D115_COLUMN_START_DEF		0
#define	MT9D115_WINDOW_HEIGHT_MIN		2
#define	MT9D115_WINDOW_HEIGHT_MAX		MT9D115_PREVIEW_HEIGHT
#define	MT9D115_WINDOW_HEIGHT_DEF		MT9D115_PREVIEW_HEIGHT
#define	MT9D115_WINDOW_WIDTH_MIN		2
#define	MT9D115_WINDOW_WIDTH_MAX		MT9D115_PREVIEW_WIDTH
#define	MT9D115_WINDOW_WIDTH_DEF		MT9D115_PREVIEW_WIDTH
#define MT9D115_ENABLE					1
#define MT9D115_DISABLE					0

#define MIN_FPS							15
#define MAX_FPS 						30
#define DEFAULT_FPS 					30
#define MT9D115_XCLK_MIN 				6000000
#define MT9D115_XCLK_MAX 				24000000
#define I2C_BYTE_ACCESS					2

//#define I2C_FUNCTION_DEBUG
//#define FUNCTION_DEBUG
#ifdef FUNCTION_DEBUG
#define LOG_FUNCTION_NAME printk("[%d] : %s : %s() ENTER\n", __LINE__, __FILE__, __FUNCTION__)
#define LOG_FUNCTION_NAME_EXIT printk("[%d] : %s : %s() EXIT\n", __LINE__, __FILE__,  __FUNCTION__)
#else
#define LOG_FUNCTION_NAME (0)
#define LOG_FUNCTION_NAME_EXIT (0)
#endif

#define MT9D115_DEBUG
#ifdef MT9D115_DEBUG
#define mt9d115_dbg printk
#else
#define mt9d115_dbg //
#endif

#define mt9d115_err printk
#define MT9D115_ERR mt9d115_err("[Failed!!] %s : %d\n",__FILE__, __LINE__)

typedef enum interface_mode_t {
		MT9D115_PARALLEL_MODE	= 0,
		MT9D115_MIPI_MODE	= 1
} INTERFACE_MODE_T;


typedef enum pll_setting_t {
		MT9D115_PLL_MIPI_YUV422 = 0,
		MT9D115_PLL_MIPI_BAYER10 = 1
} PLL_SETTING_T;

typedef struct mt9d115_firmware_reg_t {
	unsigned short u16OffsetAddr 	: 8;	// 
	unsigned short u16VarAddr 	 	: 5;	//
	unsigned short u16Access 	 	: 2;	// 01 : logical access
	unsigned short u16BitLength 	: 1;	// 1: 8-bit access; 0: 16-bit access
} MT9D115_FIRMWARE_REG_T;

typedef union mt9d115_reg_t {
	MT9D115_FIRMWARE_REG_T stRegAccess;
	unsigned short u16RegAddr;
} MT9D115_REG_T;

typedef struct mt9d115_reg_access_t {
	MT9D115_REG_T stAddr;
	unsigned short u16Val;
} MT9D115_REG_ACCESS_T;

static MT9D115_REG_ACCESS_T mt9d115_gamma_data[] = {
	{{{0x4F, 0x0B, 0x01, 0x1}}, 0},
	{{{0x50, 0x0B, 0x01, 0x1}}, 19},
	{{{0x51, 0x0B, 0x01, 0x1}}, 39},
	{{{0x52, 0x0B, 0x01, 0x1}}, 67},
	{{{0x53, 0x0B, 0x01, 0x1}}, 104},
	{{{0x54, 0x0B, 0x01, 0x1}}, 129},
	{{{0x55, 0x0B, 0x01, 0x1}}, 147},
	{{{0x56, 0x0B, 0x01, 0x1}}, 163},
	{{{0x57, 0x0B, 0x01, 0x1}}, 176},
	{{{0x58, 0x0B, 0x01, 0x1}}, 188},
	{{{0x59, 0x0B, 0x01, 0x1}}, 199},
	{{{0x5A, 0x0B, 0x01, 0x1}}, 209},
	{{{0x5B, 0x0B, 0x01, 0x1}}, 218},
	{{{0x5C, 0x0B, 0x01, 0x1}}, 226},
	{{{0x5D, 0x0B, 0x01, 0x1}}, 233},
	{{{0x5E, 0x0B, 0x01, 0x1}}, 239},
	{{{0x5F, 0x0B, 0x01, 0x1}}, 244},
	{{{0x60, 0x0B, 0x01, 0x1}}, 250},
	{{{0x61, 0x0B, 0x01, 0x1}}, 255}
};

static MT9D115_REG_ACCESS_T mt9d115_CCM_data[] = {
	{{{0x06, 0x03, 0x01, 0x0}}, 0x01D6}, // AWB_CCM_L_0
	{{{0x08, 0x03, 0x01, 0x0}}, 0xFF89}, // AWB_CCM_L_1
	{{{0x0A, 0x03, 0x01, 0x0}}, 0xFFA1}, // AWB_CCM_L_2	
	{{{0x0C, 0x03, 0x01, 0x0}}, 0xFF73}, // AWB_CCM_L_3
	{{{0x0E, 0x03, 0x01, 0x0}}, 0x019C}, // AWB_CCM_L_4
	{{{0x10, 0x03, 0x01, 0x0}}, 0xFFF1}, // AWB_CCM_L_5
	{{{0x12, 0x03, 0x01, 0x0}}, 0xFFB0}, // AWB_CCM_L_6
	{{{0x14, 0x03, 0x01, 0x0}}, 0xFF2D}, // AWB_CCM_L_7
	{{{0x16, 0x03, 0x01, 0x0}}, 0x0223}, // AWB_CCM_L_8
	{{{0x18, 0x03, 0x01, 0x0}}, 0x001C}, //AWB_CCM_L_9
	{{{0x1A, 0x03, 0x01, 0x0}}, 0x0048}, //AWB_CCM_L_10
	{{{0x18, 0x03, 0x01, 0x0}}, 0x001C}, //AWB_CCM_L_9
	{{{0x1A, 0x03, 0x01, 0x0}}, 0x0038}, //AWB_CCM_L_10
	{{{0x18, 0x03, 0x01, 0x0}}, 0x001E}, //AWB_CCM_L_9
	{{{0x1A, 0x03, 0x01, 0x0}}, 0x0038}, //AWB_CCM_L_10
	{{{0x18, 0x03, 0x01, 0x0}}, 0x0022}, //AWB_CCM_L_9
	{{{0x1A, 0x03, 0x01, 0x0}}, 0x0038}, //AWB_CCM_L_10
	{{{0x18, 0x03, 0x01, 0x0}}, 0x002C}, //AWB_CCM_L_9
	{{{0x1A, 0x03, 0x01, 0x0}}, 0x0038}, //AWB_CCM_L_10
	{{{0x18, 0x03, 0x01, 0x0}}, 0x0024}, //AWB_CCM_L_9
	{{{0x1A, 0x03, 0x01, 0x0}}, 0x0038}, //AWB_CCM_L_10
	{{{0x1C, 0x03, 0x01, 0x0}}, 0xFFCD}, // AWB_CCM_RL_0
	{{{0x1E, 0x03, 0x01, 0x0}}, 0x0023}, // AWB_CCM_RL_1
	{{{0x20, 0x03, 0x01, 0x0}}, 0x0010}, // AWB_CCM_RL_2
	{{{0x22, 0x03, 0x01, 0x0}}, 0x0026}, // AWB_CCM_RL_3
	{{{0x24, 0x03, 0x01, 0x0}}, 0xFFE9}, // AWB_CCM_RL_4
	{{{0x26, 0x03, 0x01, 0x0}}, 0xFFF1}, // AWB_CCM_RL_5
	{{{0x28, 0x03, 0x01, 0x0}}, 0x003A}, // AWB_CCM_RL_6
	{{{0x2A, 0x03, 0x01, 0x0}}, 0x005D}, // AWB_CCM_RL_7
	{{{0x2C, 0x03, 0x01, 0x0}}, 0xFF69}, // AWB_CCM_RL_8
	{{{0x2E, 0x03, 0x01, 0x0}}, 0x000C}, //AWB_CCM_RL_9
	{{{0x30, 0x03, 0x01, 0x0}}, 0xFFE4}, //AWB_CCM_RL_10
	{{{0x2E, 0x03, 0x01, 0x0}}, 0x000C}, //AWB_CCM_RL_9
	{{{0x30, 0x03, 0x01, 0x0}}, 0xFFF4}, //AWB_CCM_RL_10
	{{{0x2E, 0x03, 0x01, 0x0}}, 0x000A}, //AWB_CCM_RL_9
	{{{0x30, 0x03, 0x01, 0x0}}, 0xFFF4}, //AWB_CCM_RL_10
	{{{0x2E, 0x03, 0x01, 0x0}}, 0x0006}, //AWB_CCM_RL_9
	{{{0x30, 0x03, 0x01, 0x0}}, 0xFFF4}, //AWB_CCM_RL_10
	{{{0x2E, 0x03, 0x01, 0x0}}, 0xFFFC}, //AWB_CCM_RL_9
	{{{0x30, 0x03, 0x01, 0x0}}, 0xFFF4}, //AWB_CCM_RL_10
	{{{0x2E, 0x03, 0x01, 0x0}}, 0x0004}, //AWB_CCM_RL_9
	{{{0x30, 0x03, 0x01, 0x0}}, 0xFFF4}, //AWB_CCM_RL_10
};

typedef struct mt9d115_patch_t {
	unsigned short u16RegAddr;
	unsigned short u16Val;
} MT9D115_PATCH_T;

static MT9D115_PATCH_T mt9d115_patch_data[] = {
	{MT9D115_MCU_ADDRESS, 0x415},
	{MT9D115_MCU_DATA_0, 0xF601}, 
	{MT9D115_MCU_DATA_1, 0x42C1}, 
	{MT9D115_MCU_DATA_2, 0x326}, 
	{MT9D115_MCU_DATA_3, 0x11F6}, 
	{MT9D115_MCU_DATA_4, 0x143},
	{MT9D115_MCU_DATA_5, 0xC104}, 
	{MT9D115_MCU_DATA_6, 0x260A}, 
	{MT9D115_MCU_DATA_7, 0xCC04},
	
	{MT9D115_MCU_ADDRESS, 0x425},
	{MT9D115_MCU_DATA_0, 0x33BD}, 
	{MT9D115_MCU_DATA_1, 0xA362}, 
	{MT9D115_MCU_DATA_2, 0xBD04}, 
	{MT9D115_MCU_DATA_3, 0x3339}, 
	{MT9D115_MCU_DATA_4, 0xC6FF},
	{MT9D115_MCU_DATA_5, 0xF701}, 
	{MT9D115_MCU_DATA_6, 0x6439}, 
	{MT9D115_MCU_DATA_7, 0xDE5D},

	{MT9D115_MCU_ADDRESS, 0x435},
	{MT9D115_MCU_DATA_0, 0x18CE}, 
	{MT9D115_MCU_DATA_1, 0x325}, 
	{MT9D115_MCU_DATA_2, 0xCC00}, 
	{MT9D115_MCU_DATA_3, 0x27BD}, 
	{MT9D115_MCU_DATA_4, 0xC2B8},
	{MT9D115_MCU_DATA_5, 0xCC04}, 
	{MT9D115_MCU_DATA_6, 0xBDFD}, 
	{MT9D115_MCU_DATA_7, 0x33B},

	{MT9D115_MCU_ADDRESS, 0x445},
	{MT9D115_MCU_DATA_0, 0xCC06}, 
	{MT9D115_MCU_DATA_1, 0x6BFD}, 
	{MT9D115_MCU_DATA_2, 0x32F}, 
	{MT9D115_MCU_DATA_3, 0xCC03},
	{MT9D115_MCU_DATA_4, 0x25DD},
	{MT9D115_MCU_DATA_5, 0x5DC6}, 
	{MT9D115_MCU_DATA_6, 0x1ED7}, 
	{MT9D115_MCU_DATA_7, 0x6CD7},

	{MT9D115_MCU_ADDRESS, 0x455},
	{MT9D115_MCU_DATA_0, 0x6D5F}, 
	{MT9D115_MCU_DATA_1, 0xD76E}, 
	{MT9D115_MCU_DATA_2, 0xD78D}, 
	{MT9D115_MCU_DATA_3, 0x8620}, 
	{MT9D115_MCU_DATA_4, 0x977A},
	{MT9D115_MCU_DATA_5, 0xD77B}, 
	{MT9D115_MCU_DATA_6, 0x979A}, 
	{MT9D115_MCU_DATA_7, 0xC621},

	{MT9D115_MCU_ADDRESS, 0x465},
	{MT9D115_MCU_DATA_0, 0xD79B}, 
	{MT9D115_MCU_DATA_1, 0xFE01}, 
	{MT9D115_MCU_DATA_2, 0x6918}, 
	{MT9D115_MCU_DATA_3, 0xCE03}, 
	{MT9D115_MCU_DATA_4, 0x4DCC},
	{MT9D115_MCU_DATA_5, 0x13}, 
	{MT9D115_MCU_DATA_6, 0xBDC2}, 
	{MT9D115_MCU_DATA_7, 0xB8CC},

	{MT9D115_MCU_ADDRESS, 0x475},
	{MT9D115_MCU_DATA_0, 0x5E9}, 
	{MT9D115_MCU_DATA_1, 0xFD03}, 
	{MT9D115_MCU_DATA_2, 0x4FCC}, 
	{MT9D115_MCU_DATA_3, 0x34D}, 
	{MT9D115_MCU_DATA_4, 0xFD01},
	{MT9D115_MCU_DATA_5, 0x69FE}, 
	{MT9D115_MCU_DATA_6, 0x2BD}, 
	{MT9D115_MCU_DATA_7, 0x18CE},

	{MT9D115_MCU_ADDRESS, 0x485},
	{MT9D115_MCU_DATA_0, 0x361}, 
	{MT9D115_MCU_DATA_1, 0xCC00}, 
	{MT9D115_MCU_DATA_2, 0x11BD}, 
	{MT9D115_MCU_DATA_3, 0xC2B8}, 
	{MT9D115_MCU_DATA_4, 0xCC06},
	{MT9D115_MCU_DATA_5, 0x28FD}, 
	{MT9D115_MCU_DATA_6, 0x36F}, 
	{MT9D115_MCU_DATA_7, 0xCC03},

	{MT9D115_MCU_ADDRESS, 0x495},
	{MT9D115_MCU_DATA_0, 0x61FD}, 
	{MT9D115_MCU_DATA_1, 0x2BD}, 
	{MT9D115_MCU_DATA_2, 0xDE00}, 
	{MT9D115_MCU_DATA_3, 0x18CE}, 
	{MT9D115_MCU_DATA_4, 0xC2},
	{MT9D115_MCU_DATA_5, 0xCC00}, 
	{MT9D115_MCU_DATA_6, 0x37BD}, 
	{MT9D115_MCU_DATA_7, 0xC2B8},

	{MT9D115_MCU_ADDRESS, 0x4A5},
	{MT9D115_MCU_DATA_0, 0xCC06}, 
	{MT9D115_MCU_DATA_1, 0x4FDD}, 
	{MT9D115_MCU_DATA_2, 0xE6CC}, 
	{MT9D115_MCU_DATA_3, 0xC2}, 
	{MT9D115_MCU_DATA_4, 0xDD00},
	{MT9D115_MCU_DATA_5, 0xC601}, 
	{MT9D115_MCU_DATA_6, 0xF701}, 
	{MT9D115_MCU_DATA_7, 0x64C6},

	{MT9D115_MCU_ADDRESS, 0x4B5},
	{MT9D115_MCU_DATA_0, 0x5F7}, 
	{MT9D115_MCU_DATA_1, 0x165}, 
	{MT9D115_MCU_DATA_2, 0x7F01}, 
	{MT9D115_MCU_DATA_3, 0x6639}, 
	{MT9D115_MCU_DATA_4, 0x373C},
	{MT9D115_MCU_DATA_5, 0x3C3C}, 
	{MT9D115_MCU_DATA_6, 0x3C3C}, 
	{MT9D115_MCU_DATA_7, 0x30EC},

	{MT9D115_MCU_ADDRESS, 0x4C5},
	{MT9D115_MCU_DATA_0, 0x11ED}, 
	{MT9D115_MCU_DATA_1, 0x2EC}, 
	{MT9D115_MCU_DATA_2, 0xFED}, 
	{MT9D115_MCU_DATA_3, 0x8F}, 
	{MT9D115_MCU_DATA_4, 0x30ED},
	{MT9D115_MCU_DATA_5, 0x4EC}, 
	{MT9D115_MCU_DATA_6, 0xDEE}, 
	{MT9D115_MCU_DATA_7, 0x4BD},

	{MT9D115_MCU_ADDRESS, 0x4D5},
	{MT9D115_MCU_DATA_0, 0xA406}, 
	{MT9D115_MCU_DATA_1, 0x30EC}, 
	{MT9D115_MCU_DATA_2, 0x2ED}, 
	{MT9D115_MCU_DATA_3, 0x6FC}, 
	{MT9D115_MCU_DATA_4, 0x10C0},
	{MT9D115_MCU_DATA_5, 0x2705}, 
	{MT9D115_MCU_DATA_6, 0xCCFF}, 
	{MT9D115_MCU_DATA_7, 0xFFED},

	{MT9D115_MCU_ADDRESS, 0x4E5},
	{MT9D115_MCU_DATA_0, 0x6F6}, 
	{MT9D115_MCU_DATA_1, 0x256}, 
	{MT9D115_MCU_DATA_2, 0x8616}, 
	{MT9D115_MCU_DATA_3, 0x3DC3}, 
	{MT9D115_MCU_DATA_4, 0x261},
	{MT9D115_MCU_DATA_5, 0x8FE6}, 
	{MT9D115_MCU_DATA_6, 0x9C4}, 
	{MT9D115_MCU_DATA_7, 0x7C1},

	{MT9D115_MCU_ADDRESS, 0x4F5},
	{MT9D115_MCU_DATA_0, 0x226}, 
	{MT9D115_MCU_DATA_1, 0x1DFC}, 
	{MT9D115_MCU_DATA_2, 0x10C2}, 
	{MT9D115_MCU_DATA_3, 0x30ED}, 
	{MT9D115_MCU_DATA_4, 0x2FC},
	{MT9D115_MCU_DATA_5, 0x10C0}, 
	{MT9D115_MCU_DATA_6, 0xED00}, 
	{MT9D115_MCU_DATA_7, 0xC602},

	{MT9D115_MCU_ADDRESS, 0x505},
	{MT9D115_MCU_DATA_0, 0xBDC2}, 
	{MT9D115_MCU_DATA_1, 0x5330}, 
	{MT9D115_MCU_DATA_2, 0xEC00}, 
	{MT9D115_MCU_DATA_3, 0xFD10}, 
	{MT9D115_MCU_DATA_4, 0xC0EC},
	{MT9D115_MCU_DATA_5, 0x2FD}, 
	{MT9D115_MCU_DATA_6, 0x10C2}, 
	{MT9D115_MCU_DATA_7, 0x201B},

	{MT9D115_MCU_ADDRESS, 0x515},
	{MT9D115_MCU_DATA_0, 0xFC10}, 
	{MT9D115_MCU_DATA_1, 0xC230}, 
	{MT9D115_MCU_DATA_2, 0xED02}, 
	{MT9D115_MCU_DATA_3, 0xFC10}, 
	{MT9D115_MCU_DATA_4, 0xC0ED},
	{MT9D115_MCU_DATA_5, 0xC6}, 
	{MT9D115_MCU_DATA_6, 0x1BD}, 
	{MT9D115_MCU_DATA_7, 0xC253},

	{MT9D115_MCU_ADDRESS, 0x525},
	{MT9D115_MCU_DATA_0, 0x30EC}, 
	{MT9D115_MCU_DATA_1, 0xFD}, 
	{MT9D115_MCU_DATA_2, 0x10C0}, 
	{MT9D115_MCU_DATA_3, 0xEC02}, 
	{MT9D115_MCU_DATA_4, 0xFD10},
	{MT9D115_MCU_DATA_5, 0xC2C6}, 
	{MT9D115_MCU_DATA_6, 0x80D7}, 
	{MT9D115_MCU_DATA_7, 0x85C6},

	{MT9D115_MCU_ADDRESS, 0x535},
	{MT9D115_MCU_DATA_0, 0x40F7}, 
	{MT9D115_MCU_DATA_1, 0x10C4}, 
	{MT9D115_MCU_DATA_2, 0xF602}, 
	{MT9D115_MCU_DATA_3, 0x5686}, 
	{MT9D115_MCU_DATA_4, 0x163D},
	{MT9D115_MCU_DATA_5, 0xC302}, 
	{MT9D115_MCU_DATA_6, 0x618F}, 
	{MT9D115_MCU_DATA_7, 0xEC14},

	{MT9D115_MCU_ADDRESS, 0x545},
	{MT9D115_MCU_DATA_0, 0xFD10}, 
	{MT9D115_MCU_DATA_1, 0xC501}, 
	{MT9D115_MCU_DATA_2, 0x101}, 
	{MT9D115_MCU_DATA_3, 0x101}, 
	{MT9D115_MCU_DATA_4, 0xFC10},
	{MT9D115_MCU_DATA_5, 0xC2DD}, 
	{MT9D115_MCU_DATA_6, 0x7FFC}, 
	{MT9D115_MCU_DATA_7, 0x10C7},

	{MT9D115_MCU_ADDRESS, 0x555},
	{MT9D115_MCU_DATA_0, 0xDD76}, 
	{MT9D115_MCU_DATA_1, 0xF602}, 
	{MT9D115_MCU_DATA_2, 0x5686}, 
	{MT9D115_MCU_DATA_3, 0x163D}, 
	{MT9D115_MCU_DATA_4, 0xC302},
	{MT9D115_MCU_DATA_5, 0x618F}, 
	{MT9D115_MCU_DATA_6, 0xEC14}, 
	{MT9D115_MCU_DATA_7, 0x939F},

	{MT9D115_MCU_ADDRESS, 0x565},
	{MT9D115_MCU_DATA_0, 0x30ED}, 
	{MT9D115_MCU_DATA_1, 0x8DC}, 
	{MT9D115_MCU_DATA_2, 0x7693}, 
	{MT9D115_MCU_DATA_3, 0x9D25}, 
	{MT9D115_MCU_DATA_4, 0x8F6},
	{MT9D115_MCU_DATA_5, 0x2BC}, 
	{MT9D115_MCU_DATA_6, 0x4F93}, 
	{MT9D115_MCU_DATA_7, 0x7F23},

	{MT9D115_MCU_ADDRESS, 0x575},
	{MT9D115_MCU_DATA_0, 0x3DF6}, 
	{MT9D115_MCU_DATA_1, 0x2BC}, 
	{MT9D115_MCU_DATA_2, 0x4F93}, 
	{MT9D115_MCU_DATA_3, 0x7F23}, 
	{MT9D115_MCU_DATA_4, 0x6F6},
	{MT9D115_MCU_DATA_5, 0x2BC}, 
	{MT9D115_MCU_DATA_6, 0x4FDD}, 
	{MT9D115_MCU_DATA_7, 0x7FDC},

	{MT9D115_MCU_ADDRESS, 0x585},
	{MT9D115_MCU_DATA_0, 0x9DDD}, 
	{MT9D115_MCU_DATA_1, 0x76F6}, 
	{MT9D115_MCU_DATA_2, 0x2BC}, 
	{MT9D115_MCU_DATA_3, 0x4F93}, 
	{MT9D115_MCU_DATA_4, 0x7F26},
	{MT9D115_MCU_DATA_5, 0xFE6}, 
	{MT9D115_MCU_DATA_6, 0xAC1}, 
	{MT9D115_MCU_DATA_7, 0x226},

	{MT9D115_MCU_ADDRESS, 0x595},
	{MT9D115_MCU_DATA_0, 0x9D6}, 
	{MT9D115_MCU_DATA_1, 0x85C1}, 
	{MT9D115_MCU_DATA_2, 0x8026}, 
	{MT9D115_MCU_DATA_3, 0x314}, 
	{MT9D115_MCU_DATA_4, 0x7401},
	{MT9D115_MCU_DATA_5, 0xF602}, 
	{MT9D115_MCU_DATA_6, 0xBC4F}, 
	{MT9D115_MCU_DATA_7, 0x937F},

	{MT9D115_MCU_ADDRESS, 0x5A5},
	{MT9D115_MCU_DATA_0, 0x2416}, 
	{MT9D115_MCU_DATA_1, 0xDE7F}, 
	{MT9D115_MCU_DATA_2, 0x9DF}, 
	{MT9D115_MCU_DATA_3, 0x7F30}, 
	{MT9D115_MCU_DATA_4, 0xEC08},
	{MT9D115_MCU_DATA_5, 0xDD76}, 
	{MT9D115_MCU_DATA_6, 0x200A}, 
	{MT9D115_MCU_DATA_7, 0xDC76},

	{MT9D115_MCU_ADDRESS, 0x5B5},
	{MT9D115_MCU_DATA_0, 0xA308}, 
	{MT9D115_MCU_DATA_1, 0x2304}, 
	{MT9D115_MCU_DATA_2, 0xEC08}, 
	{MT9D115_MCU_DATA_3, 0xDD76}, 
	{MT9D115_MCU_DATA_4, 0x1274},
	{MT9D115_MCU_DATA_5, 0x122}, 
	{MT9D115_MCU_DATA_6, 0xDE5D}, 
	{MT9D115_MCU_DATA_7, 0xEE14},

	{MT9D115_MCU_ADDRESS, 0x5C5},
	{MT9D115_MCU_DATA_0, 0xAD00}, 
	{MT9D115_MCU_DATA_1, 0x30ED}, 
	{MT9D115_MCU_DATA_2, 0x11EC}, 
	{MT9D115_MCU_DATA_3, 0x6ED}, 
	{MT9D115_MCU_DATA_4, 0x2CC},
	{MT9D115_MCU_DATA_5, 0x80}, 
	{MT9D115_MCU_DATA_6, 0xED00}, 
	{MT9D115_MCU_DATA_7, 0x8F30},

	{MT9D115_MCU_ADDRESS, 0x5D5},
	{MT9D115_MCU_DATA_0, 0xED04}, 
	{MT9D115_MCU_DATA_1, 0xEC11}, 
	{MT9D115_MCU_DATA_2, 0xEE04}, 
	{MT9D115_MCU_DATA_3, 0xBDA4}, 
	{MT9D115_MCU_DATA_4, 0x630},
	{MT9D115_MCU_DATA_5, 0xE603}, 
	{MT9D115_MCU_DATA_6, 0xD785}, 
	{MT9D115_MCU_DATA_7, 0x30C6},

	{MT9D115_MCU_ADDRESS, 0x5E5},
	{MT9D115_MCU_DATA_0, 0xB3A}, 
	{MT9D115_MCU_DATA_1, 0x3539}, 
	{MT9D115_MCU_DATA_2, 0x3C3C}, 
	{MT9D115_MCU_DATA_3, 0x3C34}, 
	{MT9D115_MCU_DATA_4, 0xCC32},
	{MT9D115_MCU_DATA_5, 0x3EBD}, 
	{MT9D115_MCU_DATA_6, 0xA558}, 
	{MT9D115_MCU_DATA_7, 0x30ED},

	{MT9D115_MCU_ADDRESS, 0x5F5},
	{MT9D115_MCU_DATA_0, 0x4BD}, 
	{MT9D115_MCU_DATA_1, 0xB2D7}, 
	{MT9D115_MCU_DATA_2, 0x30E7}, 
	{MT9D115_MCU_DATA_3, 0x6CC}, 
	{MT9D115_MCU_DATA_4, 0x323E},
	{MT9D115_MCU_DATA_5, 0xED00}, 
	{MT9D115_MCU_DATA_6, 0xEC04}, 
	{MT9D115_MCU_DATA_7, 0xBDA5},

	{MT9D115_MCU_ADDRESS, 0x605},
	{MT9D115_MCU_DATA_0, 0x44CC}, 
	{MT9D115_MCU_DATA_1, 0x3244}, 
	{MT9D115_MCU_DATA_2, 0xBDA5}, 
	{MT9D115_MCU_DATA_3, 0x585F}, 
	{MT9D115_MCU_DATA_4, 0x30ED},
	{MT9D115_MCU_DATA_5, 0x2CC}, 
	{MT9D115_MCU_DATA_6, 0x3244}, 
	{MT9D115_MCU_DATA_7, 0xED00},

	{MT9D115_MCU_ADDRESS, 0x615},
	{MT9D115_MCU_DATA_0, 0xF601}, 
	{MT9D115_MCU_DATA_1, 0xD54F}, 
	{MT9D115_MCU_DATA_2, 0xEA03}, 
	{MT9D115_MCU_DATA_3, 0xAA02}, 
	{MT9D115_MCU_DATA_4, 0xBDA5},
	{MT9D115_MCU_DATA_5, 0x4430}, 
	{MT9D115_MCU_DATA_6, 0xE606}, 
	{MT9D115_MCU_DATA_7, 0x3838},

	{MT9D115_MCU_ADDRESS, 0x625},
	{MT9D115_MCU_DATA_0, 0x3831}, 
	{MT9D115_MCU_DATA_1, 0x39BD}, 
	{MT9D115_MCU_DATA_2, 0xD661}, 
	{MT9D115_MCU_DATA_3, 0xF602}, 
	{MT9D115_MCU_DATA_4, 0xF4C1},
	{MT9D115_MCU_DATA_5, 0x126}, 
	{MT9D115_MCU_DATA_6, 0xBFE}, 
	{MT9D115_MCU_DATA_7, 0x2BD},

	{MT9D115_MCU_ADDRESS, 0x635},
	{MT9D115_MCU_DATA_0, 0xEE10}, 
	{MT9D115_MCU_DATA_1, 0xFC02}, 
	{MT9D115_MCU_DATA_2, 0xF5AD}, 
	{MT9D115_MCU_DATA_3, 0x39}, 
	{MT9D115_MCU_DATA_4, 0xF602},
	{MT9D115_MCU_DATA_5, 0xF4C1}, 
	{MT9D115_MCU_DATA_6, 0x226}, 
	{MT9D115_MCU_DATA_7, 0xAFE},

	{MT9D115_MCU_ADDRESS, 0x645},
	{MT9D115_MCU_DATA_0, 0x2BD}, 
	{MT9D115_MCU_DATA_1, 0xEE10}, 
	{MT9D115_MCU_DATA_2, 0xFC02}, 
	{MT9D115_MCU_DATA_3, 0xF7AD}, 
	{MT9D115_MCU_DATA_4, 0x39},
	{MT9D115_MCU_DATA_5, 0x3CBD}, 
	{MT9D115_MCU_DATA_6, 0xB059}, 
	{MT9D115_MCU_DATA_7, 0xCC00},

	{MT9D115_MCU_ADDRESS, 0x655},
	{MT9D115_MCU_DATA_0, 0x28BD}, 
	{MT9D115_MCU_DATA_1, 0xA558}, 
	{MT9D115_MCU_DATA_2, 0x8300}, 
	{MT9D115_MCU_DATA_3, 0x27}, 
	{MT9D115_MCU_DATA_4, 0xBCC},
	{MT9D115_MCU_DATA_5, 0x26}, 
	{MT9D115_MCU_DATA_6, 0x30ED}, 
	{MT9D115_MCU_DATA_7, 0xC6},

	{MT9D115_MCU_ADDRESS, 0x665},
	{MT9D115_MCU_DATA_0, 0x3BD}, 
	{MT9D115_MCU_DATA_1, 0xA544}, 
	{MT9D115_MCU_DATA_2, 0x3839}, 
	{MT9D115_MCU_DATA_3, 0xBDD9}, 
	{MT9D115_MCU_DATA_4, 0x42D6},
	{MT9D115_MCU_DATA_5, 0x9ACB}, 
	{MT9D115_MCU_DATA_6, 0x1D7}, 
	{MT9D115_MCU_DATA_7, 0x9B39}
};


/*!
 * Maintains the information on the current state of the sesor.
 */
static struct sensor_data mt9d115_data;
static struct fsl_mxc_camera_platform_data *camera_plat;

/**
 * mt9d115_reg_read - read resgiter value
 * @client: pointer to i2c client
 * @command: register address
 */
static inline int mt9d115_read_reg(u16 u16Reg, u16 *u16Val)
{
	u8  u8RegBuf[2] = {0, 0};
	u16 u16RdVal = 0;
#ifdef I2C_FUNCTION_DEBUG
	LOG_FUNCTION_NAME;
#endif

	u8RegBuf[0] = u16Reg >> 8;
	u8RegBuf[1] = u16Reg & 0xff;

	if (I2C_BYTE_ACCESS != i2c_master_send(mt9d115_data.i2c_client, u8RegBuf, I2C_BYTE_ACCESS)) {
		pr_err("%s:write reg error at %x\n", __func__, u16Reg);
		return -1;
	}

	if (I2C_BYTE_ACCESS != i2c_master_recv(mt9d115_data.i2c_client, (char *)&u16RdVal, I2C_BYTE_ACCESS)) {
		pr_err("%s:read reg error at %x : %x\n", __func__, u16Reg, u16RdVal);
		return -1;
	}
	u16RdVal = cpu_to_be16(u16RdVal);
	*u16Val = u16RdVal;

#ifdef I2C_FUNCTION_DEBUG
	LOG_FUNCTION_NAME_EXIT;
#endif
	return u16RdVal;
}


/**
 * mt9d115_reg_write - read resgiter value
 * @client: pointer to i2c client
 * @command: register address
 * @data: value to be written 
 */
static inline int mt9d115_write_reg(u16 u16Reg, u16 u16Val)
{
	u8 u8Buf[4] = {0, 0, 0, 0};

#ifdef I2C_FUNCTION_DEBUG
	LOG_FUNCTION_NAME;
#endif

	u8Buf[0] = u16Reg >> 8;
	u8Buf[1] = u16Reg & 0xff;
	u8Buf[2] = u16Val >> 8;
	u8Buf[3] = u16Val & 0xff;

	if (i2c_master_send(mt9d115_data.i2c_client, u8Buf, 4) < 0) {
		pr_err("%s:write reg error at %x : %x\n", __func__, u16Reg, u16Val);
		return -1;
	}

#ifdef I2C_FUNCTION_DEBUG
	LOG_FUNCTION_NAME_EXIT;
#endif
	return 0;
}


/**
 * mt9d115_detect - Detect if an mt9d115 is present, and if so which revision
 * @client: pointer to the i2c client driver structure
 *
 * Returns a negative error number if no device is detected
 */
static int mt9d115_detect(struct i2c_client *client)
{
	int retval = 0;
	u16 u16ChipVersion = 0x0;
	
	LOG_FUNCTION_NAME;

	retval = mt9d115_read_reg(MT9D115_CHIP_ID_REG, &u16ChipVersion);
	if (retval < 0 || u16ChipVersion != MT9D115_CHIP_ID) {
		pr_err("%s:cannot find camera u16ChipVersion : 0x%x\n", __func__, u16ChipVersion);
		retval = -ENODEV;
		return retval;
	}
	dev_info(&client->dev, "Aptina MT9D115 detected at address 0x%02x : 0x%x\n", client->addr, u16ChipVersion);
	mt9d115_dbg("Aptina MT9D115 detected at address 0x%02x : 0x%x\n", client->addr, u16ChipVersion);

	LOG_FUNCTION_NAME_EXIT;
    return retval;
}


static int mt9d115_reset(void) 
{
	int retVal = 0;  
	u16 readVal = 0;
	LOG_FUNCTION_NAME;
	
	retVal = mt9d115_read_reg(MT9D115_RST_CTRL_REG, &readVal);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_RST_CTRL_REG, readVal | 0x0001);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	mt9d115_dbg("Reset Reg 0x%x : 0x%x\n",MT9D115_RST_CTRL_REG, readVal);
	
	mdelay(10);
	
	retVal = mt9d115_read_reg(MT9D115_RST_CTRL_REG, &readVal);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_RST_CTRL_REG, readVal & 0xFFFE);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	mt9d115_dbg("Reset Reg 0x%x : 0x%x\n",MT9D115_RST_CTRL_REG, readVal);
	
	LOG_FUNCTION_NAME_EXIT;
	return retVal;
}


static int mt9d115_interfaceMode(INTERFACE_MODE_T mode) 
{
	int retVal = 0;  
	u16 readVal = 0;
	LOG_FUNCTION_NAME;
	
	retVal = mt9d115_read_reg(MT9D115_RST_CTRL_REG, &readVal);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	switch (mode) {
		case MT9D115_MIPI_MODE: // MIPI
			readVal = (readVal & 0xFDFF) | 0x0008;
			break;
		case MT9D115_PARALLEL_MODE: // PARALLEL
			readVal = (readVal & 0xFFF7) | 0x0200;
			break;
	}
	retVal = mt9d115_write_reg(MT9D115_RST_CTRL_REG, readVal);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	mt9d115_dbg("Reset Reg 0x%x : 0x%x\n",MT9D115_RST_CTRL_REG, readVal);
	
	LOG_FUNCTION_NAME_EXIT;
	return retVal;
}


static int mt9d115_pllSetting(PLL_SETTING_T type) 
{
	int retVal = 0;  
	
	LOG_FUNCTION_NAME;
	
	// program the on-chip PLL
	switch (type) {
		case MT9D115_PLL_MIPI_YUV422:
			//PLL Control: BYPASS PLL = 8697
			retVal = mt9d115_write_reg(MT9D115_PLL_CONTROL, 0x21F9);
			if (retVal < 0) { MT9D115_ERR; return -1; }
			
			// mipi timing for YUV422 (clk_txfifo_wr = 85/42.5Mhz; clk_txfifo_rd = 63.75Mhz)
			// PLL Dividers = 277
			retVal = mt9d115_write_reg(MT9D115_PLL_DIVIDERS, 0x0115);
			if (retVal < 0) { MT9D115_ERR; return -1; }
			// wcd = 8
			retVal = mt9d115_write_reg(MT9D115_PLL_P_DIVIDERS, 0x00F5);
			if (retVal < 0) { MT9D115_ERR; return -1; }
			
			// PLL Control: TEST_BYPASS on = 9541
			retVal = mt9d115_write_reg(MT9D115_PLL_CONTROL, 0x2545);
			if (retVal < 0) { MT9D115_ERR; return -1; }
			
			// PLL Control: PLL_ENABLE on = 9543
			retVal = mt9d115_write_reg(MT9D115_PLL_CONTROL, 0x2547);
			if (retVal < 0) { MT9D115_ERR; return -1; }
			
			// PLL Control: SEL_LOCK_DET on
			retVal = mt9d115_write_reg(MT9D115_PLL_CONTROL, 0x2447);
			if (retVal < 0) { MT9D115_ERR; return -1; }
			
			mdelay(10);
			
			// PLL Control: PLL_BYPASS off
			retVal = mt9d115_write_reg(MT9D115_PLL_CONTROL, 0x2047);
			if (retVal < 0) { MT9D115_ERR; return -1; }
			
			// PLL Control: TEST_BYPASS off
			retVal = mt9d115_write_reg(MT9D115_PLL_CONTROL, 0x2046);
			if (retVal < 0) { MT9D115_ERR; return -1; }
			break;
		case MT9D115_PLL_MIPI_BAYER10:
			break;
	}	
	LOG_FUNCTION_NAME_EXIT;
	return retVal;
}


static int mt9d115_Gamma(void) 
{
	int retVal = 0;
	int count = 0;

	// Set to automatic mode VAR(0x0B,0x0037)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0XAB37);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 3);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// HG_GAMMASTARTMORPH @ 100 lux VAR(0x0B,0x0038)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0X2B38);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 10600);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	// HG_GAMMASTOPMORPH @ 20 lux VAR(0x0B,0x003A)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0X2B3A);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 11600);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Fade-To-Black: disabled VAR(0x0B,0x0062)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0X2B62);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0xFFFE);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	// Disable FTB VAR(0x0B,0x0064)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0X2B64);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0xFFFF);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	for(count = 0; count < ARRAY_SIZE(mt9d115_gamma_data); count++) {
		//mt9d115_dbg("Reg: 0x%X :: Val: %d\n", mt9d115_gamma_data[count].stAddr.u16RegAddr, mt9d115_gamma_data[count].u16Val);
		retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, mt9d115_gamma_data[count].stAddr.u16RegAddr);
		if (retVal < 0) { MT9D115_ERR; return -1; }
		retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, mt9d115_gamma_data[count].u16Val);
		if (retVal < 0) { MT9D115_ERR; return -1; }
	}

	LOG_FUNCTION_NAME_EXIT;
	return retVal;
}


static int mt9d115_CCM(void) 
{
	int retVal = 0;
	int count = 0;
	
	LOG_FUNCTION_NAME;
	
	for(count = 0; count < ARRAY_SIZE(mt9d115_CCM_data); count++) {
		//mt9d115_dbg("Reg: 0x%X :: Val: 0x%X\n", mt9d115_CCM_data[count].stAddr.u16RegAddr, mt9d115_CCM_data[count].u16Val);
		retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, (unsigned short)mt9d115_CCM_data[count].stAddr.u16RegAddr);
		if (retVal < 0) { MT9D115_ERR; return -1; }
		retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, mt9d115_CCM_data[count].u16Val);
		if (retVal < 0) { MT9D115_ERR; return -1; }
	}

	LOG_FUNCTION_NAME_EXIT;
	return retVal;
}


static int mt9d115_patch(void) 
{
	int retVal = 0;
	int count = 0;
	u16 readVal = 0;
	
	LOG_FUNCTION_NAME;
	// SOC2031_patch
	for(count = 0; count < ARRAY_SIZE(mt9d115_patch_data); count++) {
		retVal = mt9d115_write_reg(mt9d115_patch_data[count].u16RegAddr, mt9d115_patch_data[count].u16Val);
		if (retVal < 0) { MT9D115_ERR; return -1; }
	}
	
	// hard coded start address of the patch at "patchSetup" 
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2006);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x415);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	// execute the patch
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA005);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x1);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	// wait for the patch to complete initialization
	// POLL_FIELD=MON_PATCH_ID_0,==0,DELAY=10,TIMEOUT=100
	count = 0;
	readVal = 0;
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA024);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	do
	{
		retVal = mt9d115_read_reg(MT9D115_MCU_DATA_0, &readVal);
		if (retVal < 0) { MT9D115_ERR; return -1; }
		mdelay(10);
		count++;
		//mt9d115_dbg("count : %d readVal = 0x%x\n", count, readVal);
	} while (count < 10 && readVal == 0x0000);

	LOG_FUNCTION_NAME_EXIT;
	return retVal;
}


static int mt9d115_basicInit(void) 
{
	int retVal = 0;
	int count = 0;
	u16 readVal = 0;
	
	LOG_FUNCTION_NAME;
	
	// MCU Powerup Stop Enable
	// set powerup stop bit
	// MCU Powerup Stop Enable
	retVal = mt9d115_read_reg(MT9D115_CTRL_STATUS, &readVal);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_CTRL_STATUS, readVal | 0x0004);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	 
	// start MCU, includes wait for standby_done to clear
	// release MCU from standby
	// GO
	retVal = mt9d115_read_reg(MT9D115_CTRL_STATUS, &readVal);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_CTRL_STATUS, readVal & 0xFFFE);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	// wait for R20B to come out of standby
	do
	{
		retVal = mt9d115_read_reg(MT9D115_CTRL_STATUS, &readVal);
		if (retVal < 0) { MT9D115_ERR; return -1; }
		readVal &= 0x4000;
		mdelay(10);
		count++;
		//mt9d115_dbg("count : %d\n", count);
	} while (count < 10 && readVal != 0x0000);
	
	
	// sensor core & flicker timings
	// MT9D115 (SOC2031) Register Wizard Defaults: Sensor Core Timing 4 MIPI
	// Preview : Output width VAR(7,0x03)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2703);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, MT9D115_PREVIEW_WIDTH);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	// Preview : Output height VAR(7,0x05)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2705);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, MT9D115_PREVIEW_HEIGHT);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Capture : Output width VAR(7,0x07)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2707);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, MT9D115_CAPTURE_WIDTH);
	
	// Capture : Output height VAR(7,0x09)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2709);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, MT9D115_CAPTURE_HEIGHT);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	// Row Start (A) = 0, VAR(7,0xD)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x270D);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0000);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Column Start (A) = 0, VAR(7,0xF)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x270F);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0000);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Row End (A) = 1213, VAR(7,0x11)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2711);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x4BD);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Column End (A) = 1613, VAR(7,0x13)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2713);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x64D);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Row Speed (A) = 273, VAR(7,0x15)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2715);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0111);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Read Mode (A) = 1132, VAR(7,0x17)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2717);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x046C);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Sensor_fine_correction (A) = 90, VAR(7,0x19)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2719);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x005A);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Sensor_fine_IT_min (A) = 446, VAR(7,0x1B)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x271B);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x01BE);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// sensor_fine_IT_max_margin (A) = 305, VAR(7,0x1D)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x271D);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0131);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// mipi timing
	// Frame Lines (A) = 699, VAR(7,0x1F)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x271F);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x02BB);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Line Length (A) = 2184, VAR(7,0x21)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2721);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0888);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Row Start (B) = 4, VAR(7,0x23)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2723);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x004);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Column Start (B) = 4, VAR(7,0x25)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2725);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x004);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Row End (B) = 1211, VAR(7,0x27)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2727);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x4BB);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Column End (B) = 1611, VAR(7,0x29)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2729);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x64B);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Row Speed (B) = 273, VAR(7,0x2B)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x272B);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0111);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Read Mode (B) = 36, VAR(7,0x2D)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x272D);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0024);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// sensor_fine_correction (B) = 58, VAR(7,0x2F)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x272F);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x003A);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// sensor_fine_IT_min (B) = 246, VAR(7,0x31)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2731);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x00F6);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// sensor_fine_IT_max_margin (B) = 139, VAR(7,0x33)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2733);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x008B);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// mipi timing
	// Frame Lines (B) = 1313, VAR(7,0x35)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2735);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0521);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Sensor Lines Length (B) = 0x0888, VAR(7,0x37)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2737);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0888);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Crop_X0 (A) = 0, VAR(7,0x39)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2739);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0000);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Crop_X1 (A) = 799, VAR(7,0x3B)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x273B);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x031F);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Crop_Y0 (A) = 0, VAR(7,0x3D)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x273D);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0000);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Crop_Y1 (A) = 599, VAR(7,0x3F)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x273F);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0257);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Crop_X0 (B) = 0, VAR(7,0x47)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2747);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0000);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Crop_X1 (B) = 1599, VAR(7,0x49)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2749);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x063F);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Crop_Y0 (B) = 0, VAR(7,0x4B)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x274B);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0000);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Crop_Y1 (B) = 1199, VAR(7,0x4D)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x274D);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x04AF);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// mipi timing
	// R9 Step = 160, VAR(2,0x2D)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x222D);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x00A0);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// search_f1_50 = 38, VAR(4,0x08)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA408);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x26);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// search_f2_50 = 41, VAR(4,0x09)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA409);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x29);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// search_f1_60 = 46, VAR(4,0x0A)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA40A);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x2E);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// search_f2_60 = 49, VAR(4,0x0B)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA40B);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x31);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// R9_Step_60 (A) = 160, VAR(4,0x11)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2411);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x00A0);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// R9_Step_50 (A) = 192, VAR(4,0x13)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2413);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x00C0);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// R9_Step_60 (B) = 160, VAR(4,0x15)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2415);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x00A0);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// R9_Step_50 (B) = 192, VAR(4,0x17)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2417);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x00C0);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// FD Mode = 16, VAR(4,0x04)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA404);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x10);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Stat_min = 2, VAR(4,0x0D)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA40D);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x02);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Stat_max = 3, VAR(4,0x0E)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA40E);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x03);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// Min_amplitude = 10, VAR(4,0x10)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA410);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0A);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	
	// AE settings
	// Do the rest of the basic initialization.
	// preview ENTER: average luma = 2, VAR(1,0x17)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA117);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x02);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	// preview: average luma = 2 VAR(1,0x001D)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA11D);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x02);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	// capture: average luma = 2, VAR(1,0x0029)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA129);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x02);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	// AE_BASETARGET = 50, VAR(2, 0x004F)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA24F);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x32);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	// AE_MAX_INDEX = 16, VAR(2, 0x000C)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA20C);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x10);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	// MCU_ADDRESS [AE_MAXGAIN23]
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA216);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	// ae_max_gain23 has to be less than or equal to ae_max_virtgain
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x0091);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	// ae.maxvirtgain = 255 is the max. analog green gain
	// ae.maxvirtgain * (B/G) <= 255 in order to prevent the analog blue gain from overflow
	// under A light WB tuning, the max. (B/G) is 1.750, hence, ae.maxvirtgain <= 145
	// figure out the max. analog green gain to get WB in virtual units (16 virtual units = 1 analog unit)
	// VAR(2,0x000E), 0x91
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA20E);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x91);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	//ae_max_dgain_ae1 = (494 -> 174 -> 164) in order to reduce CFPN
	// VAR(2,0x0012)
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0x2212);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x00A4);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	// Lens Correction
	// Note: this LSC is generated manually for this part - DO NOT USE the "FACTORY" label
	// LOAD_PROM=0xA8, PGA   // load the PGA from the Demo2 on-board EEPROM
	// PGA_ENABLE
	retVal = mt9d115_read_reg(0x3210, &readVal);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(0x3210, readVal | 0x00008);
	if (retVal < 0) { MT9D115_ERR; return -1; }

#if 0	
	if (mt9d115_Gamma() < 0) {
		MT9D115_ERR; return -1;
	}
	
	if (mt9d115_CCM() < 0) {
		MT9D115_ERR; return -1;
	}
#endif
	if (mt9d115_patch() < 0) {
		MT9D115_ERR; return -1;
	}
	
	// continue after powerup stop
	// clear powerup stop bit
	retVal = mt9d115_read_reg(0x0018, &readVal);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(0x0018, readVal & 0xFFFB);
	if (retVal < 0) { MT9D115_ERR; return -1; }

	// wait for sequencer to enter preview state
	// POLL_FIELD=SEQ_STATE,!=3,DELAY=10,TIMEOUT=100
	readVal = 0;
	count = 0;
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA104);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	do
	{
		retVal = mt9d115_read_reg(MT9D115_MCU_DATA_0, &readVal);
		if (retVal < 0) { MT9D115_ERR; return -1; }
		mdelay(10);
		count++;
		//mt9d115_dbg("count : %d\n", count);
	} while (count < 10 && readVal != 3);

	// syncronize the FW with the sensor
	retVal = mt9d115_write_reg(MT9D115_MCU_ADDRESS, 0xA103);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	retVal = mt9d115_write_reg(MT9D115_MCU_DATA_0, 0x06);
	if (retVal < 0) { MT9D115_ERR; return -1; }
	
	LOG_FUNCTION_NAME_EXIT;
	return retVal;
}


/************************************************************************
			v4l2_ioctls
************************************************************************/
/**
 * mt9d115_v4l2_int_enum_framesizes - V4L2 sensor if handler for vidioc_int_enum_framesizes
 * @s: pointer to standard V4L2 device structure
 * @frms: pointer to standard V4L2 framesizes enumeration structure
 *
 * Returns possible framesizes depending on choosen pixel format
 */
static int 
mt9d115_v4l2_int_enum_framesizes(struct v4l2_int_device *s, struct v4l2_frmsizeenum *frms)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * mt9d115_v4l2_int_enum_frameintervals - V4L2 sensor if handler for vidioc_int_enum_frameintervals
 * @s: pointer to standard V4L2 device structure
 * @frmi: pointer to standard V4L2 frameinterval enumeration structure
 *
 * Returns possible frameinterval numerator and denominator depending on choosen frame size
 */
static int 
mt9d115_v4l2_int_enum_frameintervals(struct v4l2_int_device *s, struct v4l2_frmivalenum *frmi)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * mt9d115_v4l2_int_s_power - V4L2 sensor interface handler for vidioc_int_s_power_num
 * @s: pointer to standard V4L2 device structure
 * @on: power state to which device is to be set
 *
 * Sets devices power state to requrested state, if possible.
 */
static int 
mt9d115_v4l2_int_s_power(struct v4l2_int_device *s, int on)
{	
	static int u32SensorIsReady = 0;
	struct sensor_data *sensor = s->priv;
	struct i2c_client *client = sensor->i2c_client;
	int ret;

	LOG_FUNCTION_NAME;

	if (on && !sensor->on) {
		mt9d115_dbg("Power on!\n");
		/* Make sure power on */
		if (camera_plat->pwdn)
			camera_plat->pwdn(0);

	} else if (!on && sensor->on) {
		mt9d115_dbg("Power off!\n");
		if (camera_plat->pwdn)
			camera_plat->pwdn(1);
	}
	sensor->on = on;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * mt9d115_v4l2_int_g_priv - V4L2 sensor interface handler for vidioc_int_g_priv_num
 * @s: pointer to standard V4L2 device structure
 * @p: void pointer to hold sensor's private data address
 *
 * Returns device's (sensor's) private data area address in p parameter
 */
static int 
mt9d115_v4l2_int_g_priv(struct v4l2_int_device *s, void *p)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * mt9d115_v4l2_int_g_ifparm - V4L2 sensor interface handler for vidioc_int_g_priv_num
 * @s: pointer to standard V4L2 device structure
 * @p: void pointer to hold sensor's ifparm
 *
 * Returns device's (sensor's) ifparm in p parameter
 */
static int mt9d115_v4l2_int_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	LOG_FUNCTION_NAME;

	if (s == NULL) {
		pr_err("   ERROR!! no slave device set!\n");
		return -1;
	}

	memset(p, 0, sizeof(*p));
	p->u.bt656.clock_curr = mt9d115_data.mclk;
	pr_debug("   clock_curr=mclk=%d\n", mt9d115_data.mclk);
	p->if_type = V4L2_IF_TYPE_BT656;
	p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
	p->u.bt656.clock_min = MT9D115_XCLK_MIN;
	p->u.bt656.clock_max = MT9D115_XCLK_MAX;
	p->u.bt656.bt_sync_correct = 1;  /* Indicate external vsync */


	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * mt9d115_v4l2_int_enum_fmt_cap - Implement the CAPTURE buffer VIDIOC_ENUM_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @fmt: standard V4L2 VIDIOC_ENUM_FMT ioctl structure
 *
 * Implement the VIDIOC_ENUM_FMT ioctl for the CAPTURE buffer type.
 */
static int 
mt9d115_v4l2_int_enum_fmt_cap(struct v4l2_int_device *s,struct v4l2_fmtdesc *fmt)
{
	LOG_FUNCTION_NAME;
	fmt->pixelformat = mt9d115_data.pix.pixelformat;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * mt9d115_v4l2_int_try_fmt_cap - Implement the CAPTURE buffer VIDIOC_TRY_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_TRY_FMT ioctl structure
 *
 * Implement the VIDIOC_TRY_FMT ioctl for the CAPTURE buffer type.  This
 * ioctl is used to negotiate the image capture size and pixel format
 * without actually making it take effect.
 */
static int 
mt9d115_v4l2_int_try_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * mt9d115_v4l2_int_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int 
mt9d115_v4l2_int_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	LOG_FUNCTION_NAME;
	f->fmt.pix = mt9d115_data.pix;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * mt9d115_v4l2_int_s_fmt_cap - V4L2 sensor interface handler for VIDIOC_S_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_S_FMT ioctl structure
 *
 * If the requested format is supported, configures the HW to use that
 * format, returns error code if format not supported or HW can't be
 * correctly configured.
 */
static int 
mt9d115_v4l2_int_s_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * mt9d115_v4l2_int_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int 
mt9d115_v4l2_int_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * mt9d115_v4l2_int_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 * ----->Note, this function is not active in this release.<------
 */
static int mt9d115_v4l2_int_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * mt9d115_v4l2_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 * ----->Note, this function is not active in this release.<------
 */
static int 
mt9d115_v4l2_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * mt9d115_v4l2_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 * ----->Note, this function is not active in this release.<------
 */
static int mt9d115_v4l2_s_ctrl(struct v4l2_int_device *s, struct v4l2_control  *ctrl)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * mt9d115_v4l2_queryctrl - V4L2 sensor interface handler for VIDIOC_QUERYCTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @qc: standard V4L2 VIDIOC_QUERYCTRL ioctl structure
 *
 * If the requested control is supported, returns the control information
 * from the video_control[] array.  Otherwise, returns -EINVAL if the
 * control is not supported.
 */
static int 
mt9d115_v4l2_queryctrl(struct v4l2_int_device *s, struct v4l2_queryctrl *qc)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/*!
 * mt9d115_v4l2_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.
 */
static int mt9d115_v4l2_dev_init(struct v4l2_int_device *s)
{
	int retval = 0;
	//struct i2c_client *client = mt9d115_data.i2c_client;
	//void *mipi_csi2_info;
	struct mipi_csi2_info *mipi_csi2_info = NULL;
	u32 mipi_reg;
	
	LOG_FUNCTION_NAME;

	mipi_csi2_info = mipi_csi2_get_info();

	/* enable mipi csi2 */
	if (mipi_csi2_info) {
		mipi_csi2_enable(mipi_csi2_info);
		printk(KERN_ERR "Get mipi_csi2_info!\n");
	}
	else {
		printk(KERN_ERR "Fail to get mipi_csi2_info!\n");
		return -EPERM;
	}

    mdelay(10);

	mipi_csi2_info = mipi_csi2_get_info();

	/* initial mipi dphy */
	if (mipi_csi2_info) {
		if (!mipi_csi2_get_status(mipi_csi2_info))
			mipi_csi2_enable(mipi_csi2_info);

		if (mipi_csi2_get_status(mipi_csi2_info)) {
			mipi_csi2_set_lanes(mipi_csi2_info);
			mipi_csi2_reset(mipi_csi2_info);

			if (mt9d115_data.pix.pixelformat == V4L2_PIX_FMT_YUYV)
				mipi_csi2_set_datatype(mipi_csi2_info, MIPI_DT_YUV422);
			else
				pr_err("currently this sensor format can not be supported!\n");
		} else {
			pr_err("Can not enable mipi csi2 driver!\n");
			return -1;
		}
	} else {
		printk(KERN_ERR "Fail to get mipi_csi2_info!\n");
		return -1;
	}
	
	// sensor configuration
	if (mt9d115_reset() < 0) {
		MT9D115_ERR; return -1;
	}  
	
	if (mt9d115_interfaceMode(MT9D115_MIPI_MODE) < 0) {
		MT9D115_ERR; return -1;
	}
	
	if (mt9d115_pllSetting(MT9D115_PLL_MIPI_YUV422) < 0) {
		MT9D115_ERR; return -1;
	}
	if (mt9d115_basicInit() < 0) {
		MT9D115_ERR; return -1;
	}
	if (mipi_csi2_info) {
		unsigned int i = 0;
		/* wait for mipi sensor ready */
		mipi_reg = mipi_csi2_dphy_status(mipi_csi2_info);
		while ((mipi_reg == 0x200) && (i < 10)) {
			mipi_reg = mipi_csi2_dphy_status(mipi_csi2_info);
			i++;
			msleep(10);
		}

		if (i >= 10) {
			pr_err("mipi csi2 can not receive sensor clk!\n");
			return -1;
		}

		i = 0;

		/* wait for mipi stable */
		mipi_reg = mipi_csi2_get_error1(mipi_csi2_info);
		while ((mipi_reg != 0x0) && (i < 10)) {
			mipi_reg = mipi_csi2_get_error1(mipi_csi2_info);
			i++;
			msleep(10);
		}

		if (i >= 10) {
			pr_err("mipi csi2 can not reveive data correctly!\n");
			return -1;
		}
	}
	
	LOG_FUNCTION_NAME_EXIT;
	return retval;
}


/*!
 * mt9d115_v4l2_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the device when slave detaches to the master.
 */
static int mt9d115_v4l2_dev_exit(struct v4l2_int_device *s)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/*!
 * mt9d115_v4l2_init_num - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 */
static int mt9d115_v4l2_init_num(struct v4l2_int_device *s)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/*!
 * mt9d115_v4l2_g_chip_ident - V4L2 sensor interface handler for
 *			VIDIOC_DBG_G_CHIP_IDENT ioctl
 * @s: pointer to standard V4L2 device structure
 * @id: pointer to int
 *
 * Return 0.
 */
static int mt9d115_v4l2_g_chip_ident(struct v4l2_int_device *s, int *id)
{
	LOG_FUNCTION_NAME;
	
	((struct v4l2_dbg_chip_ident *)id)->match.type = V4L2_CHIP_MATCH_I2C_DRIVER;
	((struct v4l2_dbg_chip_ident *)id)->ident = V4L2_IDENT_MT9D115;
	((struct v4l2_dbg_chip_ident *)id)->revision = 1;					
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name, "mt9d115_mipi_camera");

	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


static struct v4l2_int_ioctl_desc mt9d115_ioctl_desc[] = {
	{ .num = vidioc_int_enum_framesizes_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_int_enum_framesizes },
	{ .num = vidioc_int_enum_frameintervals_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_int_enum_frameintervals },
	{ .num = vidioc_int_s_power_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_int_s_power },
	{ .num = vidioc_int_g_priv_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_int_g_priv },
	{ .num = vidioc_int_g_ifparm_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_int_g_ifparm },
	{ .num = vidioc_int_enum_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_int_enum_fmt_cap },
	{ .num = vidioc_int_try_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_int_try_fmt_cap },
	{ .num = vidioc_int_g_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_int_g_fmt_cap }, 
	{ .num = vidioc_int_s_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_int_s_fmt_cap },
	{ .num = vidioc_int_g_parm_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_int_g_parm },
	{ .num = vidioc_int_s_parm_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_int_s_parm },
	{ .num = vidioc_int_g_ctrl_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_g_ctrl },
	{ .num = vidioc_int_s_ctrl_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_s_ctrl },
	{ .num = vidioc_int_queryctrl_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_queryctrl },
	{ .num = vidioc_int_dev_init_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_dev_init },
	{ .num = vidioc_int_dev_exit_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_dev_exit },
	{ .num = vidioc_int_init_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_init_num },
	{ .num = vidioc_int_g_chip_ident_num,
	  .func = (v4l2_int_ioctl_func *)mt9d115_v4l2_g_chip_ident },
};


static struct v4l2_int_slave mt9d115_slave = {
	.ioctls = mt9d115_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(mt9d115_ioctl_desc),
};


static struct v4l2_int_device mt9d115_int_device = {
	.module = THIS_MODULE,
	.name = "mt9d115",
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &mt9d115_slave,
	},
};


/**
 * mt9d115_probe - probing the mt9d115 soc sensor
 * @client: i2c client driver structure
 * @did:    device id of i2c_device_id structure
 *
 * Upon the given i2c client, the sensor's module id is to be retrieved.
 */
static int mt9d115_probe(struct i2c_client *client, const struct i2c_device_id *did)
{
	int retval;
	struct fsl_mxc_camera_platform_data *plat_data = client->dev.platform_data;

	LOG_FUNCTION_NAME;

	if (!client->dev.platform_data) {
		dev_err(&client->dev, "no platform data?\n");
		return -ENODEV;
	}

	/* Set initial values for the sensor struct. */
	memset(&mt9d115_data, 0, sizeof(mt9d115_data));
	mt9d115_data.mclk = MT9D115_XCLK_MAX; /* 6 - 54 MHz, typical 24MHz */
	mt9d115_data.mclk = plat_data->mclk;
	mt9d115_data.mclk_source = plat_data->mclk_source;
	mt9d115_data.csi = plat_data->csi;
	mt9d115_data.io_init = plat_data->io_init;

	mt9d115_data.i2c_client = client;
	mt9d115_data.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	mt9d115_data.pix.width = MT9D115_PREVIEW_WIDTH;
	mt9d115_data.pix.height = MT9D115_PREVIEW_HEIGHT;
	mt9d115_data.streamcap.capability = V4L2_MODE_HIGHQUALITY |
					   V4L2_CAP_TIMEPERFRAME;
	mt9d115_data.streamcap.capturemode = 0;
	mt9d115_data.streamcap.timeperframe.denominator = DEFAULT_FPS;
	mt9d115_data.streamcap.timeperframe.numerator = 1;

	if (plat_data->io_init)
		plat_data->io_init();

	if (plat_data->pwdn)
		plat_data->pwdn(0);
	
	// detecting I2C sensor
	retval = mt9d115_detect(mt9d115_data.i2c_client);
	if (retval < 0 ) {
		pr_err("Cannot detect camera!\n");
		retval = -ENODEV;
		return retval;
	}

	if (plat_data->pwdn)
		plat_data->pwdn(1);

	// Keeping copy of plat_data
	camera_plat = plat_data;
	mt9d115_int_device.priv = &mt9d115_data;

	// registering with v4l2 int device
	retval = v4l2_int_device_register(&mt9d115_int_device);

	LOG_FUNCTION_NAME_EXIT;
	return retval;
}


/**
 * mt9d115_remove - remove the mt9d115 soc sensor driver module
 * @client: i2c client driver structure
 *
 * Upon the given i2c client, the sensor driver module is removed.
 */
static int mt9d115_remove(struct i2c_client *client)
{
	LOG_FUNCTION_NAME;
	v4l2_int_device_unregister(&mt9d115_int_device);
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


static const struct i2c_device_id mt9d115_id[] = {
	{ "mt9d115_mipi", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9d115_id);


static struct i2c_driver mt9d115_i2c_driver = {
	.driver = {
		.name = "mt9d115",
	},
	.probe    = mt9d115_probe,
	.remove   = mt9d115_remove,
	.id_table = mt9d115_id,
};


/************************************************************************
			module function
************************************************************************/
static int __init mt9d115_module_init(void)
{
	LOG_FUNCTION_NAME;
	return i2c_add_driver(&mt9d115_i2c_driver);
	LOG_FUNCTION_NAME_EXIT;
}


static void __exit mt9d115_module_exit(void)
{
	LOG_FUNCTION_NAME;
	i2c_del_driver(&mt9d115_i2c_driver);
	LOG_FUNCTION_NAME_EXIT;
}


module_init(mt9d115_module_init);
module_exit(mt9d115_module_exit);


MODULE_AUTHOR("Aptina Imaging");
MODULE_DESCRIPTION("Aptina Imaging, MT9D115 Sensor Driver");
MODULE_LICENSE("GPL v2");
