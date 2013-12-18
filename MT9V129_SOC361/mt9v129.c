/*
 * drivers/media/video/mt9v129.c
 *
 * Aptina MT9V129 sensor driver
 *
 * Copyright (C) 2013 Aptina Imaging
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "debug level (0-2)");

//#define FUNCTION_DEBUG
#ifdef FUNCTION_DEBUG
#define LOG_FUNCTION_NAME printk("[%d] : %s : %s() ENTER\n", __LINE__, __FILE__, __FUNCTION__)
#define LOG_FUNCTION_NAME_EXIT printk("[%d] : %s : %s() EXIT\n", __LINE__, __FILE__,  __FUNCTION__)
#else
#define LOG_FUNCTION_NAME (0)
#define LOG_FUNCTION_NAME_EXIT (0)
#endif

#define MT9V129_DEBUG
#ifdef MT9V129_DEBUG
#define mt9v129_dbg printk
#else
#define mt9v129_dbg //
#endif

#define NTSC
#define MT9V129_XCLK_MIN 						6000000
#define MT9V129_XCLK_MAX 						24000000

#define MT9V129_PIXEL_ARRAY_WIDTH				640
#define MT9V129_PIXEL_ARRAY_HEIGHT				480

// Result Status codes
#define MT9V129_ENOERR		0x00 // No error - command was successful
#define MT9V129_ENOENT		0x01 // No such entity
#define MT9V129_EINTR		0x02 // Operation interrupted
#define MT9V129_EIO			0x03 // I/O failure
#define MT9V129_E2BIG		0x04 // Too big
#define MT9V129_EBADF		0x05 // Bad file/handle
#define MT9V129_EAGAIN		0x06 // Would-block, try again
#define MT9V129_ENOMEM		0x07 // Not enough memory/resource
#define MT9V129_EACCES		0x08 // Permission denied
#define MT9V129_EBUSY		0x09 // Entity busy, cannot support operation
#define MT9V129_EEXIST		0x0A // Entity exists
#define MT9V129_ENODEV		0x0B // Device not found
#define MT9V129_EINVAL		0x0C // Invalid argument
#define MT9V129_ENOSPC		0x0D // No space/resource to complete
#define MT9V129_ERANGE		0x0E // Parameter out of range
#define MT9V129_ENOSYS		0x0F // Operation not supported
#define MT9V129_EALREADY	0x10 // Already requested/exists

/* Sysctl registers */
#define MT9V129_CHIP_ID						0x0000
#define MT9V129_COMMAND_REGISTER			0x40
#define MT9V129_COMMAND_REGISTER_SET_STATE	0x8100
#define MT9V129_COMMAND_REGISTER_GET_STATE	0x8101
#define MT9V129_PAD_SLEW					0x1E
#define MT9V129_SOFT_RESET					0x001a

/* Mask */
#define MT9V129_COMMAND_REGISTER_DOORBELL_MASK	(1 << 15)

/* Command parameters */
#define MT9V129_COMMAND_PARAMS_0	0xFC00
#define MT9V129_COMMAND_PARAMS_1	0xFC02
#define MT9V129_COMMAND_PARAMS_2	0xFC04
#define MT9V129_COMMAND_PARAMS_3	0xFC06
#define MT9V129_COMMAND_PARAMS_4	0xFC08
#define MT9V129_COMMAND_PARAMS_5	0xFC0A
#define MT9V129_COMMAND_PARAMS_6	0xFC0C
#define MT9V129_COMMAND_PARAMS_7	0xFC0E

/* NTSC Variables */
#define MT9V129_NTSC_OUTPUT_FORMAT_YUV		0x9420
#define MT9V129_NTSC_PORT_PARALLEL_CONTROL	0x9426
#define MT9V129_NTSC_PORT_COMPOSITE_CONTROL	0x9427

/* PAL Variables */
#define MT9V129_PAL_OUTPUT_FORMAT_YUV		0x9820
#define MT9V129_PAL_PORT_PARALLEL_CONTROL	0x9826
#define MT9V129_PAL_PORT_COMPOSITE_CONTROL	0x9827

/* Camera Control registers */
#define MT9V129_CAM_FRAME_SCAN_CONTROL		0xC858
#define MT9V129_CAM_PORT_PARALLEL_CONTROL	0xC972
#define MT9V129_CAM_OUTPUT_FORMAT_YUV		0xC96E

/* System Manager registers */
#define MT9V129_SYSMGR_NEXT_STATE MT9V129_COMMAND_PARAMS_0

/* SYS_STATE values (for SYSMGR_NEXT_STATE and SYSMGR_CURRENT_STATE) */
#define MT9V129_SYS_STATE_ENTER_CONFIG_CHANGE   0x28
#define MT9V129_SYS_STATE_STREAMING				0x31
#define MT9V129_SYS_STATE_START_STREAMING       0x34
#define MT9V129_SYS_STATE_ENTER_SUSPEND         0x40
#define MT9V129_SYS_STATE_SUSPENDED				0x41
#define MT9V129_SYS_STATE_ENTER_STANDBY         0x50
#define MT9V129_SYS_STATE_STANDBY				0x52
#define MT9V129_SYS_STATE_LEAVE_STANDBY         0x54

enum mt9v129_mode {
	MT9V129_MODE_MIN = 0,
	MT9V129_MODE_VGA = 0,
	MT9V129_MODE_NTSC = 2,
	MT9V129_MODE_PAL = 3,
	MT9V129_MODE_MAX = 3
};

struct mt9v129_resolution {
	unsigned int raw_width;
	unsigned int raw_height;
	unsigned int act_width;
	unsigned int act_height;
};

static const struct mt9v129_resolution mt9v129_resolutions[] = {
	[MT9V129_MODE_VGA] = {
		.raw_width  = 640,
		.raw_height = 480,
		.act_width  = 640,
		.act_height = 480,
	},
	[MT9V129_MODE_NTSC] = {
		.raw_width  = 720,
		.raw_height = 525,
		.act_width  = 720,
		.act_height = 480,
	},
	[MT9V129_MODE_PAL] = {
		.raw_width  = 720,
		.raw_height = 625,
		.act_width  = 720,
		.act_height = 576,
	},
};

#define MIN_FPS 25
#define MAX_FPS 60
#define DEFAULT_MODE 0

enum mt9v129_frame_rate {
	MT9V129_VGA_FPS = 0,
	MT9V129_NTSC_FPS,
	MT9V129_PAL_FPS
};

static int mt9v129_framerates[] = {
	[MT9V129_VGA_FPS] = 60,
	[MT9V129_NTSC_FPS] = 30,
	[MT9V129_PAL_FPS] = 25,
};

/*!
 * Maintains the information on the current state of the sesor.
 */
static struct sensor_data mt9v129_data;
static struct fsl_mxc_camera_platform_data *camera_plat;

struct mt9v129_reg {
	u16 reg;
	u32 val;
	int width;
};

/* Default Settings from Devware */
static const struct mt9v129_reg mt9v129_init[] = {
	/*AE Rule Variable List*/
	{0xA406, 0x06, 1},
	/*AE Track Variable List*/
	{0xA800, 0x0408, 2},
	{0xA806, 0x00, 1},
	{0xA807, 0x37, 1},
	{0xA809, 0x37, 1},
	{0xA80B, 0x10, 1},
	{0xA80E, 0x0106, 2},
	{0xA818, 0x0002, 2},
	{0xA81B, 0x01, 1},
	{0xA826, 0x013A, 2},
	{0xA828, 0x14EB, 2},
	{0xA82A, 0x075B, 2},
	{0xA82C, 0x11C7, 2},
	{0xA82E, 0x1B3A, 2},
	{0xA83C, 0x00000234, 4},
	{0xA84E, 0x0084, 2},
	/*AWB Variable List*/
	{0xAC00, 0x0021, 2},
	{0xAC0E, 0x48, 1},
	{0xAC0F, 0x34, 1},
	{0xAC10, 0x63, 1},
	{0xAC12, 0x00B1, 2},
	{0xAC14, 0x00F6, 2},
	/*CCM Variable List*/
	{0xB406, 0x01FB, 2},
	{0xB408, 0xFF32, 2},
	{0xB40A, 0xFFD8, 2},
	{0xB40C, 0xFF89, 2},
	{0xB40E, 0x018B, 2},
	{0xB410, 0xFFF0, 2},
	{0xB412, 0x000C, 2},
	{0xB414, 0xFF15, 2},
	{0xB416, 0x01E3, 2},
	/*Low Light Variable List*/
	{0xBC0B, 0x20, 1},
	{0xBC0C, 0x2E, 1},
	{0xBC0D, 0x45, 1},
	{0xBC0E, 0x69, 1},
	{0xBC0F, 0x83, 1},
	{0xBC10, 0x96, 1},
	{0xBC11, 0xA7, 1},
	{0xBC12, 0xB4, 1},
	{0xBC13, 0xC0, 1},
	{0xBC14, 0xCB, 1},
	{0xBC15, 0xD5, 1},
	{0xBC16, 0xDD, 1},
	{0xBC17, 0xE5, 1},
	{0xBC18, 0xEB, 1},
	{0xBC19, 0xF2, 1},
	{0xBC1A, 0xF7, 1},
	{0xBC1B, 0xFC, 1},
	{0xBC1C, 0xFF, 1},
	{0xBC1E, 0x1F, 1},
	{0xBC1F, 0x2F, 1},
	{0xBC20, 0x49, 1},
	{0xBC21, 0x6E, 1},
	{0xBC22, 0x87, 1},
	{0xBC23, 0x99, 1},
	{0xBC24, 0xAA, 1},
	{0xBC25, 0xB8, 1},
	{0xBC26, 0xC4, 1},
	{0xBC27, 0xCE, 1},
	{0xBC28, 0xD7, 1},
	{0xBC29, 0xE0, 1},
	{0xBC2A, 0xE7, 1},
	{0xBC2B, 0xED, 1},
	{0xBC2C, 0xF3, 1},
	{0xBC2D, 0xF7, 1},
	{0xBC2E, 0xFC, 1},
	{0xBC2F, 0xFF, 1},
	{0xBC32, 0x7D, 1},
	{0xBC38, 0x02D1, 2},
	{0xBC3A, 0x047F, 2},
	/*Flicker Detect Variables*/
	{0xC07B, 0x03, 1},
	{0xC07D, 0x05, 1},
	/*CamControl Variable List*/
	{0xC80E, 0x00A4, 2},
	{0xC83A, 0x0021, 2},
	{0xC83D, 0x01, 1},
	{0xC83E, 0x020D, 2},
	{0xC840, 0x020B, 2},
	{0xC842, 0x035A, 2},
	{0xC844, 0x00BB, 2},
	{0xC84A, 0x0103, 2},
	{0xC84C, 0x0081, 2},
	{0xC84F, 0x15, 1},
	{0xC882, 0x3BF0, 2},
	{0xC884, 0x3BF0, 2},
	{0xC888, 0x01F2, 2},
	{0xC88A, 0xFF6E, 2},
	{0xC88C, 0xFFA0, 2},
	{0xC88E, 0xFF76, 2},
	{0xC890, 0x0188, 2},
	{0xC892, 0x0002, 2},
	{0xC894, 0xFFC2, 2},
	{0xC896, 0xFF30, 2},
	{0xC898, 0x020E, 2},
	{0xC8AC, 0x01AF, 2},
	{0xC8AE, 0xFF93, 2},
	{0xC8B0, 0xFFBE, 2},
	{0xC8B2, 0xFF9E, 2},
	{0xC8B4, 0x0176, 2},
	{0xC8B6, 0xFFED, 2},
	{0xC8B8, 0x0001, 2},
	{0xC8BA, 0xFF78, 2},
	{0xC8BC, 0x0187, 2},
	{0xC8E6, 0x13E2, 2},
	{0xC8EA, 0x5D54, 2},
	{0xC8EC, 0xDCCE, 2},
	{0xC8F0, 0x812A, 2},
	{0xC8F2, 0x9C08, 2},
	{0xC8F4, 0x989D, 2},
	{0xC8F6, 0x3D23, 2},
	{0xC8F8, 0x0624, 2},
	{0xC8FA, 0x0040, 2},
	{0xC8FC, 0x0035, 2},
	{0xC928, 0x0018, 2},
	{0xC92A, 0x0090, 2},
	{0xC931, 0x07, 1},
	{0xC932, 0x02, 1},
	{0xC934, 0x02, 1},
	{0xC935, 0x1F, 1},
	{0xC946, 0x28, 1},
	{0xC947, 0x1E, 1},
	{0xC948, 0x0093, 2},
	{0xC94A, 0x0366, 2},
	{0xC958, 0x0020, 2},
	{0xC95A, 0x0095, 2},
	{0xC960, 0x0001, 2},
	{0xC96E, 0x001C, 2},
	{0xC970, 0x10, 1},
	{0xC972, 0x0025, 2},
	{0xC97E, 0x0071, 2},
	{0xC980, 0x0471, 2},
	{0xC985, 0x3B, 1},
	{0xC986, 0x34, 1},
	{0xC987, 0x3E, 1},
};

static int mt9v129_read8(struct i2c_client *client, u16 reg, u8 *val)
{
	int ret;
	u8 rval;
	struct i2c_msg msg[] = {
		{
			.addr   = client->addr,
			.flags  = 0,
			.len    = 2,
			.buf    = (u8 *)&reg,
		},
		{
			.addr   = client->addr,
			.flags  = I2C_M_RD,
			.len    = 2,
			.buf    = (u8 *)&rval,
		},
	};

	reg = swab16(reg);

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		v4l_err(client, "Failed to read register 0x%04x!\n", reg);
		return ret;
	}
	*val = rval;

	return 0;
}

static int mt9v129_write8(struct i2c_client *client, u16 reg, u8 val)
{
	int ret;
	struct {
		u16 reg;
		u8 val;
	} __packed buf;
	struct i2c_msg msg = {
		.addr   = client->addr,
		.flags  = 0,
		.len    = 3,
		.buf    = (u8 *)&buf,
	};
	buf.reg = swab16(reg);
	buf.val = val;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		v4l_err(client, "Failed to write register 0x%04x!\n", reg);
		return ret;
	}

	return 0;
}

static int mt9v129_read16(struct i2c_client *client, u16 reg, u16 *val)
{
	int ret;
	u16 rval;
	struct i2c_msg msg[] = {
		{
			.addr   = client->addr,
			.flags  = 0,
			.len    = 2,
			.buf    = (u8 *)&reg,
		},
		{
			.addr   = client->addr,
			.flags  = I2C_M_RD,
			.len    = 2,
			.buf    = (u8 *)&rval,
		},
	};

	reg = swab16(reg);

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		v4l_err(client, "Failed to read register 0x%04x!\n", reg);
		return ret;
	}
	*val = swab16(rval);

	return 0;
}

static int mt9v129_write16(struct i2c_client *client, u16 reg, u16 val)
{
	int ret;
	struct {
		u16 reg;
		u16 val;
	} __packed buf;
	struct i2c_msg msg = {
		.addr   = client->addr,
		.flags  = 0,
		.len    = 4,
		.buf    = (u8 *)&buf,
	};
	buf.reg = swab16(reg);
	buf.val = swab16(val);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		v4l_err(client, "Failed to write register 0x%04x!\n", reg);
		return ret;
	}

	return 0;
}

static int mt9v129_write32(struct i2c_client *client, u16 reg, u32 val)
{
	int ret;
	struct {
		u16 reg;
		u32 val;
	} __packed buf;
	struct i2c_msg msg = {
		.addr   = client->addr,
		.flags  = 0,
		.len    = 6,
		.buf    = (u8 *)&buf,
	};
	buf.reg = swab16(reg);
	buf.val = swab32(val);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		v4l_err(client, "Failed to write register 0x%04x!\n", reg);
		return ret;
	}

	return 0;
}

static int mt9v129_read32(struct i2c_client *client, u16 reg, u32 *val)
{
	int ret;
	u32 rval;
	struct i2c_msg msg[] = {
		{
			.addr   = client->addr,
			.flags  = 0,
			.len    = 2,
			.buf    = (u8 *)&reg,
		},
		{
			.addr   = client->addr,
			.flags  = I2C_M_RD,
			.len    = 4,
			.buf    = (u8 *)&rval,
		},
	};

	reg = swab16(reg);

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		v4l_err(client, "Failed to read register 0x%04x!\n", reg);
		return ret;
	}
	*val = swab32(rval);

	return 0;
}

static int mt9v129_writeregs(struct i2c_client *client,
		const struct mt9v129_reg *regs, int len)
{
	int i, ret;

	for (i = 0; i < len; i++) {
		switch (regs[i].width) {
			case 1:
				ret = mt9v129_write8(client,
						regs[i].reg, regs[i].val);
				break;
			case 2:
				ret = mt9v129_write16(client,
						regs[i].reg, regs[i].val);
				break;
			case 4:
				ret = mt9v129_write32(client,
						regs[i].reg, regs[i].val);
				break;
			default:
				ret = -EINVAL;
				break;
		}
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int mt9v129_set_state(struct i2c_client *client, u8 next_state)
{
	int timeout = 100, ret;
	u16 command;

	/* set the next desired state */
	ret = mt9v129_write8(client, MT9V129_SYSMGR_NEXT_STATE, next_state);
	if (ret < 0)
		return ret;

	/* check door bell status */
	ret = mt9v129_read16(client,
			MT9V129_COMMAND_REGISTER, &command);
	if (ret < 0)
		return ret;
	if (command & MT9V129_COMMAND_REGISTER_DOORBELL_MASK) {
		v4l_err(client, "Firmware busy. Cant Set State!!\n");
		return -EAGAIN;
	}

	/* start state transition */
	ret = mt9v129_write16(client, MT9V129_COMMAND_REGISTER,
			MT9V129_COMMAND_REGISTER_SET_STATE);
	if (ret < 0)
		return ret;

	/* wait for the state transition to complete */
	while (timeout) {
		ret = mt9v129_read16(client,
				MT9V129_COMMAND_REGISTER, &command);
		if (ret < 0)
			return ret;
		if (!(command & MT9V129_COMMAND_REGISTER_DOORBELL_MASK))
			break;
		msleep(10);
		timeout--;
	}
	if (!timeout) {
		v4l_err(client, "Failed to poll command register\n");
		return -ETIMEDOUT;
	}

	/* check if the command is successful */
	ret = mt9v129_read16(client,
			MT9V129_COMMAND_REGISTER, &command);
	if (ret < 0)
		return ret;
	if (!(command & ~(MT9V129_COMMAND_REGISTER_DOORBELL_MASK))) {
		return 0;
	} else {
		v4l_err(client, "Failed to set state, Response = %x\n", command);
		return -EFAULT;
	}
}

static int mt9v129_get_state(struct i2c_client *client)
{
	int timeout = 1000, ret;
	u16 command;
	u8 state;

	/* check door bell status */
	ret = mt9v129_read16(client,
			MT9V129_COMMAND_REGISTER, &command);
	if (ret < 0)
		return ret;
	if ((command & MT9V129_COMMAND_REGISTER_DOORBELL_MASK)) {
		v4l_err(client, "Firmware busy. Cant get state!!\n");
		return -EAGAIN;
	}

	/* start state transition */
	ret = mt9v129_write16(client, MT9V129_COMMAND_REGISTER,
			MT9V129_COMMAND_REGISTER_GET_STATE);
	if (ret < 0)
		return ret;

	/* wait for the state transition to complete */
	while (timeout) {
		ret = mt9v129_read16(client,
				MT9V129_COMMAND_REGISTER, &command);
		if (ret < 0)
			return ret;
		if (!(command & MT9V129_COMMAND_REGISTER_DOORBELL_MASK))
			break;
		msleep(10);
		timeout--;
	}
	if (!timeout) {
		v4l_err(client, "Failed to poll command register\n");
		return -ETIMEDOUT;
	}

	/* check if the command is successful */
	ret = mt9v129_read16(client,
			MT9V129_COMMAND_REGISTER, &command);
	if (ret < 0)
		return ret;

	/* get the state */
	ret = mt9v129_read8(client, MT9V129_SYSMGR_NEXT_STATE, &state);
	if (ret < 0)
		return ret;
	if (!(command & ~(MT9V129_COMMAND_REGISTER_DOORBELL_MASK))) {
		return state;
	} else {
		v4l_err(client, "Failed to get state, Response = %x\n", command);
		return -EFAULT;
	}
}

static int mt9v129_change_mode(struct i2c_client *client,
		u32 mode)
{
	int ret = 0;
	switch (mode) {
		case MT9V129_MODE_VGA:
			/* set the sensor in parallel data output mode */
			ret = mt9v129_write16(client, MT9V129_CAM_PORT_PARALLEL_CONTROL, 0x5);
			ret = mt9v129_write16(client, MT9V129_CAM_FRAME_SCAN_CONTROL, 0x1);
			mt9v129_data.pix.width = 640;
			mt9v129_data.pix.height = 480;
			mt9v129_data.mclk_source = 0;
			break;
		case MT9V129_MODE_NTSC:
			/* set the sensor in NTSC data output mode */
			ret = mt9v129_write8(client, MT9V129_NTSC_PORT_PARALLEL_CONTROL, 0x23);
			ret = mt9v129_write16(client, MT9V129_CAM_FRAME_SCAN_CONTROL, 0x0);
			mt9v129_data.pix.width = 720;
			mt9v129_data.pix.height = 480;
			mt9v129_data.mclk_source = 1;
			break;
		case MT9V129_MODE_PAL:
			/* set the sensor in PAL data output mode */
			ret = mt9v129_write8(client, MT9V129_PAL_PORT_PARALLEL_CONTROL, 0x23);
			ret = mt9v129_write16(client, MT9V129_CAM_FRAME_SCAN_CONTROL, 0x2);
			mt9v129_data.pix.width = 720;
			mt9v129_data.pix.height = 576;
			mt9v129_data.mclk_source = 1;
			break;
	}
	/* start state transition */
	mt9v129_set_state(client, MT9V129_SYS_STATE_ENTER_CONFIG_CHANGE);
	return ret;
}

/************************************************************************
  v4l2_ioctls
 ************************************************************************/
/**
 * ioctl_enum_framesizes - V4L2 sensor if handler for vidioc_int_enum_framesizes
 * @s: pointer to standard V4L2 device structure
 * @frms: pointer to standard V4L2 framesizes enumeration structure
 *
 * Returns possible framesizes depending on choosen pixel format
 */
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
		struct v4l2_frmsizeenum *fsize)
{
	LOG_FUNCTION_NAME;
	if (fsize->index > MT9V129_MODE_MAX)
		return -EINVAL;
	fsize->pixel_format = mt9v129_data.pix.pixelformat;
	fsize->discrete.width = mt9v129_resolutions[fsize->index].act_width;
	fsize->discrete.height = mt9v129_resolutions[fsize->index].act_height;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * ioctl_enum_frameintervals - V4L2 sensor if handler for vidioc_int_enum_frameintervals
 * @s: pointer to standard V4L2 device structure
 * @frmi: pointer to standard V4L2 frameinterval enumeration structure
 *
 * Returns possible frameinterval numerator and denominator depending on choosen frame size
 */
static int ioctl_enum_frameintervals(struct v4l2_int_device *s,
		struct v4l2_frmivalenum *fival)
{
	int i;

	LOG_FUNCTION_NAME;
	if (fival->index < 0 || fival->index > MT9V129_MODE_MAX)
		return -EINVAL;
	if (fival->pixel_format == 0 || fival->width == 0 || fival->height == 0) {
		pr_warning("Please assign pixelformat, width and height.\n");
		return -EINVAL;
	}
	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator = 1;
	for (i = 0; i < ARRAY_SIZE(mt9v129_resolutions); i++) {
		if (fival->pixel_format == mt9v129_data.pix.pixelformat
				&& fival->width == mt9v129_resolutions[i].act_width
				&& fival->height == mt9v129_resolutions[i].act_height) {
			fival->discrete.denominator =
				mt9v129_framerates[i];
			return 0;
		}
	}
	LOG_FUNCTION_NAME_EXIT;
	return -EINVAL;
}


/**
 * ioctl_s_power - V4L2 sensor interface handler for vidioc_int_s_power_num
 * @s: pointer to standard V4L2 device structure
 * @on: power state to which device is to be set
 *
 * Sets devices power state to requrested state, if possible.
 */
static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
	struct sensor_data *sensor = s->priv;
	int ret, state;

	LOG_FUNCTION_NAME;

	if (on && !sensor->on) {
		mt9v129_dbg("Power on!\n");
		// Make sure power on
		if (camera_plat->pwdn)
			camera_plat->pwdn(0);
		ret = mt9v129_set_state(mt9v129_data.i2c_client,
				MT9V129_SYS_STATE_START_STREAMING);
		state = mt9v129_get_state(mt9v129_data.i2c_client);

	} else if (!on && sensor->on) {
		mt9v129_dbg("Power off!\n");
		if (camera_plat->pwdn)
			camera_plat->pwdn(1);
		ret = mt9v129_set_state(mt9v129_data.i2c_client,
				MT9V129_SYS_STATE_ENTER_SUSPEND);
		state = mt9v129_get_state(mt9v129_data.i2c_client);
	}
	v4l_dbg(1, debug, mt9v129_data.i2c_client,
			"Firmware State = %d\n", state);
	sensor->on = on;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}

/**
 * ioctl_g_priv - V4L2 sensor interface handler for vidioc_int_g_priv_num
 * @s: pointer to standard V4L2 device structure
 * @p: void pointer to hold sensor's private data address
 *
 * Returns device's (sensor's) private data area address in p parameter
 */
static int ioctl_g_priv(struct v4l2_int_device *s, void *p)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * ioctl_g_ifparm - V4L2 sensor interface handler for vidioc_int_g_priv_num
 * @s: pointer to standard V4L2 device structure
 * @p: void pointer to hold sensor's ifparm
 *
 * Returns device's (sensor's) ifparm in p parameter
 */
static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	LOG_FUNCTION_NAME;

	if (s == NULL) {
		v4l_err(mt9v129_data.i2c_client, "   ERROR!! no slave device set!\n");
		return -1;
	}
	memset(p, 0, sizeof(*p));
	p->if_type = V4L2_IF_TYPE_BT656;//V4L2_IF_TYPE_RAW;//V4L2_IF_TYPE_BT656;
	p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
	/* mclk_source shall be used to determine if Interlaced / not */
	if (!mt9v129_data.mclk_source)
		p->u.bt656.clock_curr = mt9v129_data.mclk;
	else
		p->u.bt656.clock_curr = 0;
	pr_debug("clock_curr = %d\n", p->u.bt656.clock_curr);
	p->u.bt656.clock_min = MT9V129_XCLK_MIN;
	p->u.bt656.clock_max = MT9V129_XCLK_MAX;
	p->u.bt656.bt_sync_correct = 1;  /* Indicate external vsync */
	p->u.bt656.nobt_hs_inv = 0;
	p->u.bt656.nobt_vs_inv = 0;
	p->u.bt656.latch_clk_inv = 0;

	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * ioctl_enum_fmt_cap - Implement the CAPTURE buffer VIDIOC_ENUM_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @fmt: standard V4L2 VIDIOC_ENUM_FMT ioctl structure
 *
 * Implement the VIDIOC_ENUM_FMT ioctl for the CAPTURE buffer type.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
		struct v4l2_fmtdesc *fmt)
{
	LOG_FUNCTION_NAME;
	fmt->pixelformat = mt9v129_data.pix.pixelformat;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * ioctl_try_fmt_cap - Implement the CAPTURE buffer VIDIOC_TRY_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_TRY_FMT ioctl structure
 *
 * Implement the VIDIOC_TRY_FMT ioctl for the CAPTURE buffer type.  This
 * ioctl is used to negotiate the image capture size and pixel format
 * without actually making it take effect.
 */
static int ioctl_try_fmt_cap(struct v4l2_int_device *s,
		struct v4l2_format *f)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s,
		struct v4l2_format *f)
{
	LOG_FUNCTION_NAME;
	f->fmt.pix = mt9v129_data.pix;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * ioctl_s_fmt_cap - V4L2 sensor interface handler for VIDIOC_S_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_S_FMT ioctl structure
 *
 * If the requested format is supported, configures the HW to use that
 * format, returns error code if format not supported or HW can't be
 * correctly configured.
 */
static int ioctl_s_fmt_cap(struct v4l2_int_device *s,
		struct v4l2_format *f)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s,
		struct v4l2_streamparm *a)
{
	struct sensor_data *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;
	int ret = 0;

	LOG_FUNCTION_NAME;
	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->streamcap.capability;
		cparm->timeperframe = sensor->streamcap.timeperframe;
		cparm->capturemode = sensor->streamcap.capturemode;
		ret = 0;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}
	LOG_FUNCTION_NAME_EXIT;
	return ret;
}

/**
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 * ----->Note, this function is not active in this release.<------
 */
static int ioctl_s_parm(struct v4l2_int_device *s,
		struct v4l2_streamparm *a)
{
	struct sensor_data *sensor = s->priv;
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	u32 new_mode = a->parm.capture.capturemode;
	u32 tgt_fps;	/* target frames per secound */
	enum mt9v129_frame_rate new_frame_rate;
	int ret = 0;

	/* Make sure power on */
	if (camera_plat->pwdn)
		camera_plat->pwdn(0);

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		/* Check that the new frame rate is allowed. */
		if ((timeperframe->numerator == 0) ||
		    (timeperframe->denominator == 0)) {
			timeperframe->denominator = mt9v129_framerates[DEFAULT_MODE];
			timeperframe->numerator = 1;
		}
		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;
		if (tgt_fps > MAX_FPS) {
			timeperframe->denominator = MAX_FPS;
			timeperframe->numerator = 1;
		} else if (tgt_fps < MIN_FPS) {
			timeperframe->denominator = MIN_FPS;
			timeperframe->numerator = 1;
		}
		/* Actual frame rate we use */
		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;
		if (tgt_fps == 60)
			new_frame_rate = MT9V129_VGA_FPS;
		else if (tgt_fps == 30)
			new_frame_rate = MT9V129_NTSC_FPS;
		else if (tgt_fps == 25)
			new_frame_rate = MT9V129_PAL_FPS;
		else {
			v4l_err(mt9v129_data.i2c_client,
					" The camera frame rate is not supported!\n");
			return -EINVAL;
		}
		/* Change the sensor mode */
		ret = mt9v129_change_mode(mt9v129_data.i2c_client, new_mode);
		if (ret < 0)
			return ret;
		sensor->streamcap.timeperframe = *timeperframe;
		sensor->streamcap.capturemode =
				(u32)a->parm.capture.capturemode;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		v4l_dbg(1, debug, mt9v129_data.i2c_client,
			"V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n", a->type);
		ret = -EINVAL;
		break;
	default:
		v4l_dbg(1, debug, mt9v129_data.i2c_client,
				"type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/**
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 * ----->Note, this function is not active in this release.<------
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s,
		struct v4l2_control *vc)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 * ----->Note, this function is not active in this release.<------
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s,
		struct v4l2_control  *ctrl)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/**
 * ioctl_queryctrl - V4L2 sensor interface handler for VIDIOC_QUERYCTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @qc: standard V4L2 VIDIOC_QUERYCTRL ioctl structure
 *
 * If the requested control is supported, returns the control information
 * from the video_control[] array.  Otherwise, returns -EINVAL if the
 * control is not supported.
 */
static int ioctl_queryctrl(struct v4l2_int_device *s,
		struct v4l2_queryctrl *qc)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/*!
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
	int retVal = 0;

	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return retVal;
}


/*!
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the device when slave detaches to the master.
 */
static int ioctl_dev_exit(struct v4l2_int_device *s)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/*!
 * ioctl_init_num - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 */
static int ioctl_init_num(struct v4l2_int_device *s)
{
	LOG_FUNCTION_NAME;
	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


/*!
 * ioctl_g_chip_ident - V4L2 sensor interface handler for
 *			VIDIOC_DBG_G_CHIP_IDENT ioctl
 * @s: pointer to standard V4L2 device structure
 * @id: pointer to int
 *
 * Return 0.
 */
static int ioctl_g_chip_ident(struct v4l2_int_device *s, int *id)
{
	LOG_FUNCTION_NAME;
	((struct v4l2_dbg_chip_ident *)id)->match.type =
		V4L2_CHIP_MATCH_I2C_DRIVER;
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name, "mt9v129_camera");

	LOG_FUNCTION_NAME_EXIT;
	return 0;
}


static struct v4l2_int_ioctl_desc mt9v129_ioctl_desc[] = {
	{ .num = vidioc_int_enum_framesizes_num,
		.func = (v4l2_int_ioctl_func *)ioctl_enum_framesizes },
	{ .num = vidioc_int_enum_frameintervals_num,
		.func = (v4l2_int_ioctl_func *)ioctl_enum_frameintervals },
	{ .num = vidioc_int_s_power_num,
		.func = (v4l2_int_ioctl_func *)ioctl_s_power },
	{ .num = vidioc_int_g_priv_num,
		.func = (v4l2_int_ioctl_func *)ioctl_g_priv },
	{ .num = vidioc_int_g_ifparm_num,
		.func = (v4l2_int_ioctl_func *)ioctl_g_ifparm },
	{ .num = vidioc_int_enum_fmt_cap_num,
		.func = (v4l2_int_ioctl_func *)ioctl_enum_fmt_cap },
	{ .num = vidioc_int_try_fmt_cap_num,
		.func = (v4l2_int_ioctl_func *)ioctl_try_fmt_cap },
	{ .num = vidioc_int_g_fmt_cap_num,
		.func = (v4l2_int_ioctl_func *)ioctl_g_fmt_cap },
	{ .num = vidioc_int_s_fmt_cap_num,
		.func = (v4l2_int_ioctl_func *)ioctl_s_fmt_cap },
	{ .num = vidioc_int_g_parm_num,
		.func = (v4l2_int_ioctl_func *)ioctl_g_parm },
	{ .num = vidioc_int_s_parm_num,
		.func = (v4l2_int_ioctl_func *)ioctl_s_parm },
	{ .num = vidioc_int_g_ctrl_num,
		.func = (v4l2_int_ioctl_func *)ioctl_g_ctrl },
	{ .num = vidioc_int_s_ctrl_num,
		.func = (v4l2_int_ioctl_func *)ioctl_s_ctrl },
	{ .num = vidioc_int_queryctrl_num,
		.func = (v4l2_int_ioctl_func *)ioctl_queryctrl },
	{ .num = vidioc_int_dev_init_num,
		.func = (v4l2_int_ioctl_func *)ioctl_dev_init },
	{ .num = vidioc_int_dev_exit_num,
		.func = (v4l2_int_ioctl_func *)ioctl_dev_exit },
	{ .num = vidioc_int_init_num,
		.func = (v4l2_int_ioctl_func *)ioctl_init_num },
	{ .num = vidioc_int_g_chip_ident_num,
		.func = (v4l2_int_ioctl_func *)ioctl_g_chip_ident },
};


static struct v4l2_int_slave mt9v129_slave = {
	.ioctls = mt9v129_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(mt9v129_ioctl_desc),
};


static struct v4l2_int_device mt9v129_int_device = {
	.module = THIS_MODULE,
	.name = "mt9v129",
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &mt9v129_slave,
	},
};

static int mt9v129_probe(struct i2c_client *client,
		const struct i2c_device_id *did)
{
	int retval;
	struct fsl_mxc_camera_platform_data *plat_data = client->dev.platform_data;
	int ret;
	u16 chip_id;
	int state;

	LOG_FUNCTION_NAME;

	if (!client->dev.platform_data) {
		dev_err(&client->dev, "no platform data?\n");
		return -ENODEV;
	}

	/* Set initial values for the sensor struct. */
	memset(&mt9v129_data, 0, sizeof(mt9v129_data));
	mt9v129_data.mclk = plat_data->mclk; /* 6 - 54 MHz, typical 24MHz */
	mt9v129_data.mclk_source = plat_data->mclk_source;
	mt9v129_data.csi = plat_data->csi;
	mt9v129_data.io_init = plat_data->io_init;

	mt9v129_data.i2c_client = client;
	mt9v129_data.pix.pixelformat = V4L2_PIX_FMT_UYVY;
	mt9v129_data.pix.width = MT9V129_PIXEL_ARRAY_WIDTH;
	mt9v129_data.pix.height = MT9V129_PIXEL_ARRAY_HEIGHT;
	mt9v129_data.pix.priv = 0;
	mt9v129_data.on = true;
	mt9v129_data.streamcap.capability = V4L2_MODE_HIGHQUALITY |
		V4L2_CAP_TIMEPERFRAME;
	mt9v129_data.streamcap.capturemode = 0;
	mt9v129_data.streamcap.timeperframe.denominator =
		mt9v129_framerates[DEFAULT_MODE];
	mt9v129_data.streamcap.timeperframe.numerator = 1;

	if (plat_data->io_init)
		plat_data->io_init();

	if (plat_data->pwdn)
		plat_data->pwdn(0);

	/* Verify Chip ID */
	ret = mt9v129_read16(client, MT9V129_CHIP_ID, &chip_id);
	if (ret < 0) {
		v4l_err(client, "Failed to get chip id\n");
		return -ENODEV;
	}
	if (chip_id != 0x2285) {
		v4l_err(client, "chip id 0x%04x mismatch\n", chip_id);
		return -ENODEV;
	}

	/* reset the sensor */
	ret = mt9v129_write16(client, MT9V129_SOFT_RESET, 0x0001);
	if (ret < 0) {
		v4l_err(client, "Failed to reset the sensor\n");
		return ret;
	}
	mt9v129_write16(client, MT9V129_SOFT_RESET, 0x0000);
	mdelay(500);
	/* TODO - Check Software Reset is Done!! */
	state = mt9v129_get_state(client);
	v4l_dbg(1, debug, client, "state = %x\n", state);
	/* Write Initial Values to sensor AE, AWB, LDC etc. from devware */
	ret = mt9v129_writeregs(client, mt9v129_init,
			ARRAY_SIZE(mt9v129_init));
	/* Write the PAD Slew register in SOC with value from Devware */
	ret = mt9v129_write16(client, MT9V129_PAD_SLEW, 0x302);
	/* Set Default Streaming Mode */
	mt9v129_change_mode(client, DEFAULT_MODE);
	if (plat_data->pwdn)
		plat_data->pwdn(1);

	// Keeping copy of plat_data
	camera_plat = plat_data;
	mt9v129_int_device.priv = &mt9v129_data;

	// registering with v4l2 int device
	retval = v4l2_int_device_register(&mt9v129_int_device);

	LOG_FUNCTION_NAME_EXIT;
	return retval;
}

/**
 * mt9v129_remove - remove the mt9v129 soc sensor driver module
 * @client: i2c client driver structure
 *
 * Upon the given i2c client, the sensor driver module is removed.
 */
static int mt9v129_remove(struct i2c_client *client)
{
	v4l2_int_device_unregister(&mt9v129_int_device);
	return 0;
}

static const struct i2c_device_id mt9v129_id[] = {
	{ "mt9v129", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9v129_id);

static struct i2c_driver mt9v129_i2c_driver = {
	.driver = {
		.name = "mt9v129",
	},
	.probe    = mt9v129_probe,
	.remove   = mt9v129_remove,
	.id_table = mt9v129_id,
};

/************************************************************************
  module function
 ************************************************************************/
static int __init mt9v129_module_init(void)
{
	return i2c_add_driver(&mt9v129_i2c_driver);
}

static void __exit mt9v129_module_exit(void)
{
	i2c_del_driver(&mt9v129_i2c_driver);
}


module_init(mt9v129_module_init);
module_exit(mt9v129_module_exit);


MODULE_AUTHOR("Aptina Imaging");
MODULE_DESCRIPTION("Aptina Imaging, MT9V129 sensor driver");
MODULE_LICENSE("GPL v2");
