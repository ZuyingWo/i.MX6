/*
 * Copyright 2005-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*
 * Copyright 2011-2013 Bluetechnix GmbH. All Rights Reserved.
 *
 * Device driver for Bluetechnix ISM-MT9M025 and ISM-AR0132 camera modules
 * based on Freescale's OV3640 driver
 *
 * Author: Harald Krapfenbauer
 */


/*
 * drivers/media/video/mt9m024.c
 *
 * Aptina MT9M024 sensor driver
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


//#define DEBUG

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/fsl_devices.h>
#include <media/v4l2-int-device.h>
#include "mxc_v4l2_capture.h"
#include "mt9m024.h"
#include "mt9m024_pll.h"
#include "mt9m024_sequencer.h"

#define pr_Dbg pr_err

/* module parameters */
static int testpattern = 0;
static int autoexposure = 0;
static int hdrmode = 0;
static int sensorwidth = APT_MT9M024_MAX_X_RES;
static int sensorheight = APT_MT9M024_MAX_Y_RES;
static int datawidth = 12;
static int rotate = 0;
static int binning = 1;

/*!
 * Maintains the information on the current state of the sensor.
 */
struct sensor {
	struct v4l2_int_device *v4l2_int_device;
	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	struct v4l2_captureparm streamcap;
	bool on;
	int framerate;
	int height;
	int width;

	/* control settings */
	int brightness;
	int hue;
	int contrast;
	int saturation;
	int red;
	int green;
	int blue;
	int ae_mode;

	int csi;
} mt9m024_data;

const struct fsl_mxc_camera_platform_data *camera_plat;

static int mt9m024_probe(struct i2c_client *adapter,
				const struct i2c_device_id *device_id);
static int mt9m024_remove(struct i2c_client *client);
static s32 mt9m024_read_reg(u16 reg, u16 *val);
static s32 mt9m024_write_reg(u16 reg, u16 val);

static const struct i2c_device_id mt9m024_id[] = {
	{"mt9m024", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, mt9m024_id);

static struct i2c_driver mt9m024_i2c_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = "mt9m024",
		  },
	.probe  = mt9m024_probe,
	.remove = mt9m024_remove,
	.id_table = mt9m024_id,
};

static s32 mt9m024_write_reg(u16 reg, u16 val)
{
	u8 au8Buf[4] = {0};

	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xff;
	au8Buf[2] = val >> 8;
	au8Buf[3] = val & 0xff;

	if (i2c_master_send(mt9m024_data.i2c_client, au8Buf, 4) < 0) {
		pr_err("%s:write reg error: reg=%04x, val=%04x\n",
		       __func__, reg, val);
		return -1;
	}

	return 0;
}

static s32 mt9m024_read_reg(u16 reg, u16 *val)
{
	u8 regbuf[2] = {0,0};
	u8 readval[2] = {0,0};

	regbuf[0] = reg >> 8;
	regbuf[1] = reg & 0xff;

	if (2 != i2c_master_send(mt9m024_data.i2c_client, regbuf, 2)) {
		pr_err("%s:write reg error: reg=%x\n",
		       __func__, reg);
		return -1;
	}

	if (2 != i2c_master_recv(mt9m024_data.i2c_client, (char*)&readval, 2)) {
		pr_err("%s:read reg error: reg=%x\n",
		       __func__, reg);
		return -1;
	}
	*val = (readval[0] << 8) | readval[1];

	return 0;
}

static int APTsetPLL(unsigned short pa_ucM, unsigned short pa_ucN,
		     unsigned short pa_ucP1, unsigned short pa_ucP2)
{
	unsigned short usDevReg = 0;
	unsigned short usData = 0;

	// assuming software standby

	// set N = Pre_PLL_Clk_Div
	usDevReg = APT_MT9M024_PRE_PLL_CLK_DIV;
	usData = pa_ucN;
	if (mt9m024_write_reg(usDevReg, usData))
		return -1;
	// set P1 = Vt_Sys_Clk_Div
	usDevReg = APT_MT9M024_VT_SYS_CLK_DIV;
	usData = pa_ucP1;
	if (mt9m024_write_reg(usDevReg, usData))
		return -1;
	// set P2 = Vt_PIX_Clk_Div
	usDevReg = APT_MT9M024_VT_PIX_CLK_DIV;
	usData = pa_ucP2;
	if (mt9m024_write_reg(usDevReg, usData))
		return -1;
	// set M = PLL Multiplier
	usDevReg = APT_MT9M024_PLL_MULTIPLIER;
	usData = pa_ucM;
	if (mt9m024_write_reg(usDevReg, usData))
		return -1;

	// wait for 1 ms until VCO locked
	msleep(100);
	return 0;
}

static int mt9m024_init_mode(int frame_rate, int width, int height)
{
	u16 regaddr = 0;
	u16 regval = 0;
	u16 uc_M, uc_N, uc_P1, uc_P2;
	int tgtPixClk, realPixClk;
	int offset_left, offset_top;
	int sensorWidth, sensorHeight;

	if (mt9m024_data.framerate == frame_rate &&
	    mt9m024_data.width == width &&
	    mt9m024_data.height == height) {
		/* values already set, no need to repeat that */
		return 0;
	}

	/* streaming off */
	regaddr = APT_MT9M024_RESET_REGISTER;
	if (mt9m024_read_reg(regaddr, &regval))
		return -1;
	regval &= ~(1<<2);
	if (mt9m024_write_reg(regaddr, regval))
		return -1;
	pr_Dbg("%s: streaming off\n",__func__);

	/* set resolution */
	if (width <= 640 && height <= 480 && binning) {
		sensorWidth = width * 2;
		sensorHeight = height * 2;
	} else {
		sensorWidth = width;
		sensorHeight = height;
	}
	offset_left = (APT_MT9M024_MAX_X_RES - sensorWidth) / 2
		+ APT_MT9M024_X_ADDR_START_DEFAULT;
	offset_top = (APT_MT9M024_MAX_Y_RES - sensorHeight) / 2
		+ APT_MT9M024_Y_ADDR_START_DEFAULT;
	if (mt9m024_write_reg(APT_MT9M024_X_ADDR_START_, offset_left))
		return -1;
	if (mt9m024_write_reg(APT_MT9M024_X_ADDR_END_,
				offset_left + sensorWidth - 1))
		return -1;
	if (mt9m024_write_reg(APT_MT9M024_Y_ADDR_START_, offset_top))
		return -1;
	if (mt9m024_write_reg(APT_MT9M024_Y_ADDR_END_,
			      offset_top + sensorHeight - 1))
		return -1;
	pr_Dbg("%s: set resolution\n",__func__);

	/* enable digital binning? */
	if (width <= 640 && height <= 480 && binning) {
		if (mt9m024_read_reg(APT_MT9M024_DIGITAL_BINNING, &regval))
			return -1;
		regval &= ~(0x3 << 0);
		regval |= (0x2 << 0);
		if (mt9m024_write_reg(APT_MT9M024_DIGITAL_BINNING, regval))
			return -1;
		pr_Dbg("%s: digital binning on\n",__func__);
	} else {
		pr_Dbg("%s: digital binning off\n",__func__);
	}

	/* streaming on */
	if (mt9m024_read_reg(APT_MT9M024_RESET_REGISTER, &regval))
		return -1;
	regval |= (1<<2);
	if (mt9m024_write_reg(APT_MT9M024_RESET_REGISTER, regval))
		return -1;
	pr_Dbg("%s: streaming on\n",__func__);
	pr_info("%s: Mode changed %dx%d at %d fps\n", __func__, width, height,
		frame_rate);

	mt9m024_data.framerate = frame_rate;
	mt9m024_data.width = width;
	mt9m024_data.height = height;

	return 0;
}

int mt9m024_config(void)
{
	unsigned short regval;
	int i;

	pr_Dbg("%s entry\n",__FUNCTION__);
	/* Reset HW and SW */
	if (camera_plat->io_init)
		camera_plat->io_init();
	msleep(200);
	regval = 0x1;
	if (mt9m024_write_reg(APT_MT9M024_RESET_REGISTER, regval))
		return -1;
	msleep(200);
	regval = 0x10D8;
	if (mt9m024_write_reg(APT_MT9M024_RESET_REGISTER, regval))
		return -1;

	/* A-1000 Hidy and linear sequencer load August 2 2011 */
	msleep(200);
	/* enable sequencer ram */
	regval = 0x8000;
	if (mt9m024_write_reg(APT_MT9M024_SEQ_CTRL_PORT, regval))
		return -1;
	/* load sequencer ram */
	for (i=0; i<sizeof(MT9M024sequencerReg)/sizeof(unsigned short); i++) {
		if (mt9m024_write_reg(APT_MT9M024_SEQ_DATA_PORT, MT9M024sequencerReg[i]))
			return -1;
	}
	/* execute sequence */
	if (mt9m024_write_reg(0x309e, 0x0186))
		return -1;
	msleep(200);

	/* configuration presets */
	if (mt9m024_write_reg(APT_MT9M024_RESET_REGISTER, 0x10D8))
		return -1;
	if (mt9m024_write_reg(APT_MT9M024_OPERATION_MODE_CTRL, 0x29))
		return -1;
	/* A-1000ERS Optimized settings August 2 2011 */
	if (mt9m024_write_reg(APT_MT9M024_DATA_PEDESTAL_, 0x00C8)) // set datapedestal to 200 to avoid clipping near saturation
		return -1;
	if (mt9m024_write_reg(0x3EDA, 0x0F03)) //Set vln_dac to 0x3 as recommended by Sergey
		return -1;
	if (mt9m024_write_reg(0x3EDE, 0xC005))
		return -1;
	if (mt9m024_write_reg(0x3ED8, 0x09EF)) // Vrst_low = +1
		return -1;
	if (mt9m024_write_reg(0x3EE2, 0xA46B))
		return -1;
	if (mt9m024_write_reg(0x3EE0, 0x067D)) // enable anti eclipse and adjust setting for high conversion gain (changed to help with high temp noise)
		return -1;
	if (mt9m024_write_reg(0x3EDC, 0x0070)) // adjust anti eclipse setting for low conversion gain
		return -1;
	if (mt9m024_write_reg(APT_MT9M024_DARK_CONTROL, 0x0404)) // enable digital row noise correction and cancels TX during column correction
		return -1;
	if (mt9m024_write_reg(0x3EE6, 0x8303)) // Helps with column noise at low light
		return -1;
	if (mt9m024_write_reg(APT_MT9M024_DAC_LD_24_25, 0xD208)) // enable analog row noise correction and 1.25x gain
		return -1;
	if (mt9m024_write_reg(0x3ED6, 0x00BD))
		return -1;
	if (mt9m024_write_reg(0x3EE6, 0x8303)) // improves low light FPN
		return -1;
	if (mt9m024_write_reg(0x30E4, 0x6372)) // ADC settings to improve noise performance
		return -1;
	if (mt9m024_write_reg(0x30E2, 0x7253))
		return -1;
	if (mt9m024_write_reg(0x30E0, 0x5470))
		return -1;
	if (mt9m024_write_reg(0x30E6, 0xC4CC))
		return -1;
	if (mt9m024_write_reg(0x30E8, 0x8050))
		return -1;

	/* Column Retriggering at start up */
	if (mt9m024_write_reg(0x30B0, 0x1300))
		return -1;
	if (mt9m024_write_reg(0x30D4, 0xE007))
		return -1;
	if (mt9m024_write_reg(0x30BA, 0x0008))
		return -1;
	/* streaming on */
	if (mt9m024_read_reg(APT_MT9M024_RESET_REGISTER, &regval))
		return -1;
	regval |= (1<<2);
	if (mt9m024_write_reg(APT_MT9M024_RESET_REGISTER, regval))
		return -1;
	msleep(200);
	/* streaming off */
	if (mt9m024_read_reg(APT_MT9M024_RESET_REGISTER, &regval))
		return -1;
	regval &= ~(1<<2);
	if (mt9m024_write_reg(APT_MT9M024_RESET_REGISTER, regval))
		return -1;
	pr_Dbg("Reset Register #1 = %x\n", regval);
	if (mt9m024_write_reg(0x3058, 0x003F))
		return -1;
	if (mt9m024_write_reg(0x3012, 0x02A0))
		return -1;

#if 0
	/* Full Resolution 45FPS Setup */
	if (mt9m024_write_reg(APT_MT9M024_DIGITAL_BINNING, 0x0))
		return -1;
	if (mt9m024_write_reg(APT_MT9M024_Y_ADDR_START_, 0x2))
		return -1;
	if (mt9m024_write_reg(APT_MT9M024_X_ADDR_START_, 0x0))
		return -1;
	if (mt9m024_write_reg(APT_MT9M024_Y_ADDR_END_, 0x3C1))
		return -1;
	if (mt9m024_write_reg(APT_MT9M024_X_ADDR_END_, 0x4FF))
		return -1;
	if (mt9m024_write_reg(APT_MT9M024_FRAME_LENGTH_LINES_, 0x3DE))
		return -1;
	if (mt9m024_write_reg(APT_MT9M024_LINE_LENGTH_PCK_, 0x672))
		return -1;
#endif

	/* Enable Parallel Mode */
	/* Disable streaming and setup parallel */
	if (mt9m024_write_reg(0x301A, 0xD018))
		return -1;
	/* Set to 12 bits */
	if (mt9m024_write_reg(0x31D0, 0x1))
		return -1;
	if (mt9m024_write_reg(0x30B0, 0x1300))
		return -1;
	/* PLL Enabled 27Mhz to 74.25Mhz */
	if (APTsetPLL(0x2c, 0x2, 0x2, 0x4))
		return -1;
	/* streaming on */
	if (mt9m024_read_reg(APT_MT9M024_RESET_REGISTER, &regval))
		return -1;
	regval |= (1<<2);
	pr_Dbg("Reset Register #2 = %x\n", regval);
	regval = 0x10DC;
	if (mt9m024_write_reg(APT_MT9M024_RESET_REGISTER, regval))
		return -1;
	pr_Dbg("Reset Register #3 = %x\n", regval);

	/* Misc Setup Auto Exposure */
	if (mt9m024_write_reg(0x3100, 0x1B))
		return -1;
	if (mt9m024_write_reg(0x3112, 0x029F))
		return -1;
	if (mt9m024_write_reg(0x3114, 0x008C))
		return -1;
	if (mt9m024_write_reg(0x3116, 0x02C0))
		return -1;
	if (mt9m024_write_reg(0x3118, 0x005B))
		return -1;
	if (mt9m024_write_reg(0x3102, 0x0384))
		return -1;
	if (mt9m024_write_reg(0x3104, 0x1000))
		return -1;
	if (mt9m024_write_reg(0x3126, 0x0080))
		return -1;
	if (mt9m024_write_reg(0x311C, 0x03DD))
		return -1;
	if (mt9m024_write_reg(0x311E, 0x0003))
		return -1;
	return 0;
}

/* --------------- IOCTL functions from v4l2_int_ioctl_desc --------------- */

static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	pr_Dbg("%s entry\n",__FUNCTION__);

	if (s == NULL) {
		pr_err("   ERROR!! no slave device set!\n");
		return -1;
	}

	memset(p, 0, sizeof(*p));
	p->u.bt656.clock_curr = APT_MT9M024_CLOCK;
	p->if_type = V4L2_IF_TYPE_BT656;
	if (datawidth == 12)
		p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_12BIT;
	else
		p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
	p->u.bt656.clock_min = us_PllLutMin*1000000;
	p->u.bt656.clock_max = us_PllLutMax*1000000;
	p->u.bt656.latch_clk_inv = 0;
	p->u.bt656.nobt_vs_inv = 0;
	p->u.bt656.nobt_hs_inv = 0;
	p->u.bt656.bt_sync_correct = 0;

	pr_Dbg("%s exit\n",__FUNCTION__);

	return 0;
}

/*!
 * ioctl_s_power - V4L2 sensor interface handler for VIDIOC_S_POWER ioctl
 * @s: pointer to standard V4L2 device structure
 * @on: indicates power mode (on or off)
 *
 * Turns the power on or off, depending on the value of on and returns the
 * appropriate error code.
 */
static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
	struct sensor *sensor = s->priv;

	pr_Dbg("%s entry %d\n",__FUNCTION__, on);

#if 0
	if (on && !sensor->on) {
		/* Make sure power on */
		if (camera_plat->pwdn)
			camera_plat->pwdn(0);
	} else if (!on && sensor->on) {
		/* Power down */
		if (camera_plat->pwdn)
			camera_plat->pwdn(1);

	}
#endif

	sensor->on = on;

	pr_Dbg("%s exit\n",__FUNCTION__);

	return 0;
}

/*!
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct sensor *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;
	int ret = 0;

	pr_Dbg("%s entry\n",__FUNCTION__);

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
		pr_Dbg("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	pr_Dbg("%s exit\n",__FUNCTION__);

	return ret;
}

/*!
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct sensor *sensor = s->priv;
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	u32 tgt_fps;	/* target frames per secound */
	int ret = 0;

	pr_Dbg("%s entry\n",__FUNCTION__);

	/* Make sure power on */
	if (camera_plat->pwdn)
		camera_plat->pwdn(0);

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		/* Check that the new frame rate is allowed. */
		if ((timeperframe->numerator == 0) ||
		    (timeperframe->denominator == 0)) {
			timeperframe->denominator = APT_MT9M024_DEFAULT_FPS;
			timeperframe->numerator = 1;
		}

		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps > APT_MT9M024_MAX_FPS) {
			timeperframe->denominator = APT_MT9M024_MAX_FPS;
			timeperframe->numerator = 1;
		} else if (tgt_fps < APT_MT9M024_MIN_FPS) {
			timeperframe->denominator = APT_MT9M024_MIN_FPS;
			timeperframe->numerator = 1;
		}

		/* Actual frame rate we use */
		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		sensor->streamcap.timeperframe = *timeperframe;
		sensor->streamcap.capturemode =
				(u32)a->parm.capture.capturemode;

		ret = mt9m024_init_mode(tgt_fps,
					sensor->pix.width,
					sensor->pix.height);
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		pr_Dbg("   type is not " \
			"V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
			a->type);
		ret = -EINVAL;
		break;

	default:
		pr_Dbg("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	pr_Dbg("%s exit\n",__FUNCTION__);

	return ret;
}

/*!
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct sensor *sensor = s->priv;

	pr_Dbg("%s entry\n",__FUNCTION__);

	f->fmt.pix = sensor->pix;

	pr_Dbg("%s exit\n",__FUNCTION__);

	return 0;
}

/*!
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int ret = 0;

	pr_Dbg("%s entry\n",__FUNCTION__);

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		vc->value = mt9m024_data.brightness;
		break;
	case V4L2_CID_HUE:
		vc->value = mt9m024_data.hue;
		break;
	case V4L2_CID_CONTRAST:
		vc->value = mt9m024_data.contrast;
		break;
	case V4L2_CID_SATURATION:
		vc->value = mt9m024_data.saturation;
		break;
	case V4L2_CID_RED_BALANCE:
		vc->value = mt9m024_data.red;
		break;
	case V4L2_CID_BLUE_BALANCE:
		vc->value = mt9m024_data.blue;
		break;
	case V4L2_CID_EXPOSURE:
		vc->value = mt9m024_data.ae_mode;
		break;
	default:
		ret = -EINVAL;
	}

	pr_Dbg("%s exit\n",__FUNCTION__);

	return ret;
}

/*!
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int retval = 0;

	pr_Dbg("%s entry\n",__FUNCTION__);

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		break;
	case V4L2_CID_CONTRAST:
		break;
	case V4L2_CID_SATURATION:
		break;
	case V4L2_CID_HUE:
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		break;
	case V4L2_CID_RED_BALANCE:
		break;
	case V4L2_CID_BLUE_BALANCE:
		break;
	case V4L2_CID_GAMMA:
		break;
	case V4L2_CID_EXPOSURE:
		break;
	case V4L2_CID_AUTOGAIN:
		break;
	case V4L2_CID_GAIN:
		break;
	case V4L2_CID_HFLIP:
		break;
	case V4L2_CID_VFLIP:
		break;
	default:
		retval = -EPERM;
		break;
	}

	pr_Dbg("%s exit\n",__FUNCTION__);

	return retval;
}

/*!
 * ioctl_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
				 struct v4l2_frmsizeenum *fsize)
{
	if (fsize->index > 0)
		return -EINVAL;

	pr_Dbg("%s entry\n",__FUNCTION__);
	fsize->pixel_format = mt9m024_data.pix.pixelformat;
	fsize->discrete.width = sensorwidth;
	fsize->discrete.height = sensorheight;
	pr_Dbg("%s exit\n",__FUNCTION__);
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
	((struct v4l2_dbg_chip_ident *)id)->match.type =
		V4L2_CHIP_MATCH_I2C_DRIVER;
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name,
		"ism-mt9m024_camera");

	return 0;
}

/*!
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	return 0;
}

/*!
 * ioctl_enum_fmt_cap - V4L2 sensor interface handler for VIDIOC_ENUM_FMT
 * @s: pointer to standard V4L2 device structure
 * @fmt: pointer to standard V4L2 fmt description structure
 *
 * Return 0.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
			      struct v4l2_fmtdesc *fmt)
{
	if (fmt->index > 0)
		return -EINVAL;

	fmt->pixelformat = mt9m024_data.pix.pixelformat;

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
	struct sensor *sensor = s->priv;
	u32 tgt_fps;	/* target frames per second */
	int retval;

	pr_Dbg("%s entry\n",__FUNCTION__);

	mt9m024_data.on = true;
	mt9m024_data.framerate = -1;
	mt9m024_data.width = -1;
	mt9m024_data.height = -1;

	/* Default camera frame rate is set in probe */
	tgt_fps = sensor->streamcap.timeperframe.denominator /
		  sensor->streamcap.timeperframe.numerator;

	retval = mt9m024_init_mode(tgt_fps,
				   sensor->pix.width,
				   sensor->pix.height);

	pr_Dbg("%s exit\n",__FUNCTION__);

	return retval;
}

/*!
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the device when slave detaches to the master.
 */
static int ioctl_dev_exit(struct v4l2_int_device *s)
{
	return 0;
}

/*!
 * This structure defines all the ioctls for this module and links them to the
 * enumeration.
 */
static struct v4l2_int_ioctl_desc mt9m024_ioctl_desc[] = {
	{vidioc_int_dev_init_num, (v4l2_int_ioctl_func *)ioctl_dev_init},
	{vidioc_int_dev_exit_num, ioctl_dev_exit},
	{vidioc_int_s_power_num, (v4l2_int_ioctl_func *)ioctl_s_power},
	{vidioc_int_g_ifparm_num, (v4l2_int_ioctl_func *)ioctl_g_ifparm},
/*	{vidioc_int_g_needs_reset_num,
				(v4l2_int_ioctl_func *)ioctl_g_needs_reset}, */
/*	{vidioc_int_reset_num, (v4l2_int_ioctl_func *)ioctl_reset}, */
	{vidioc_int_init_num, (v4l2_int_ioctl_func *)ioctl_init},
	{vidioc_int_enum_fmt_cap_num,
				(v4l2_int_ioctl_func *)ioctl_enum_fmt_cap},
/*	{vidioc_int_try_fmt_cap_num,
				(v4l2_int_ioctl_func *)ioctl_try_fmt_cap}, */
	{vidioc_int_g_fmt_cap_num, (v4l2_int_ioctl_func *)ioctl_g_fmt_cap},
/*	{vidioc_int_s_fmt_cap_num, (v4l2_int_ioctl_func *)ioctl_s_fmt_cap}, */
	{vidioc_int_g_parm_num, (v4l2_int_ioctl_func *)ioctl_g_parm},
	{vidioc_int_s_parm_num, (v4l2_int_ioctl_func *)ioctl_s_parm},
/*	{vidioc_int_queryctrl_num, (v4l2_int_ioctl_func *)ioctl_queryctrl}, */
	{vidioc_int_g_ctrl_num, (v4l2_int_ioctl_func *)ioctl_g_ctrl},
	{vidioc_int_s_ctrl_num, (v4l2_int_ioctl_func *)ioctl_s_ctrl},
	{vidioc_int_enum_framesizes_num,
				(v4l2_int_ioctl_func *)ioctl_enum_framesizes},
	{vidioc_int_g_chip_ident_num,
				(v4l2_int_ioctl_func *)ioctl_g_chip_ident},
};

static struct v4l2_int_slave mt9m024_slave = {
	.ioctls = mt9m024_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(mt9m024_ioctl_desc),
};

static struct v4l2_int_device mt9m024_int_device = {
	.module = THIS_MODULE,
	.name = "mt9m024",
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &mt9m024_slave,
	},
};

/*!
 * mt9m024 I2C probe function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
static int mt9m024_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int retval;
	struct fsl_mxc_camera_platform_data *plat_data = client->dev.platform_data;
	u16 regaddr;
	u16 regval;

	/* Set initial values for the sensor struct. */
	memset(&mt9m024_data, 0, sizeof(mt9m024_data));
	mt9m024_data.csi = plat_data->csi;

	mt9m024_data.i2c_client = client;
	if (datawidth == 12)
		/* because of the CSI's color extension to 16 bits */
		mt9m024_data.pix.pixelformat = IPU_PIX_FMT_GENERIC_16;
	else
		mt9m024_data.pix.pixelformat = IPU_PIX_FMT_GENERIC;

	mt9m024_data.pix.width = sensorwidth;
	mt9m024_data.pix.height = sensorheight;
	mt9m024_data.streamcap.capability = V4L2_CAP_TIMEPERFRAME;
	mt9m024_data.streamcap.capturemode = 0;
	mt9m024_data.streamcap.timeperframe.denominator =
		APT_MT9M024_DEFAULT_FPS;
	mt9m024_data.streamcap.timeperframe.numerator = 1;


#if 0
	if (plat_data->pwdn)
		plat_data->pwdn(0);
#endif
	camera_plat = plat_data;

	/* read model id */
	regaddr = APT_MT9M024_MODEL_ID_;
	if (mt9m024_read_reg(regaddr, &regval))
		return -1;
	if (regval != 0x2400) {
		pr_err("%s: Camera not found\n", __func__);
		return -1;
	} else {
		pr_Dbg("%s: Camera found!\n", __func__);
	}

#if 0
	/* sw reset */
	regaddr = APT_MT9M024_RESET_REGISTER;
	if (mt9m024_read_reg(regaddr, &regval))
		return -1;
	// Bit 0 is used to reset the digital logic of the sensor
	regval |= 0x1;
#endif
#if 1
	/* sequencer ram and configuration presets */
	if (mt9m024_config()) {
		pr_err("%s: Loading sequencer failed\n",__func__);
		return -1;
	}
#endif

#if 1
	/* disable hispi i/f */
	regaddr = APT_MT9M024_RESET_REGISTER;
	if (mt9m024_read_reg(regaddr, &regval))
		return -1;
	regval |= (1<<12);
	if (mt9m024_write_reg(regaddr, regval))
		return -1;

	/* enable parallel i/f */
	regaddr = APT_MT9M024_RESET_REGISTER;
	if (mt9m024_read_reg(regaddr, &regval))
		return -1;
	regval |= ((1<<7)|(1<<6));
	if (mt9m024_write_reg(regaddr, regval))
		return -1;
#endif

	/* enable test pattern? */
	regaddr = APT_MT9M024_TEST_PATTERN_MODE_;
	switch (testpattern) {
	case 1:
	case 2:
	case 3:
		regval = testpattern;
		pr_info("%s: Enabling test pattern %d\n",__func__,testpattern);
		break;
	case 4:
		regval = 256;
		pr_info("%s: Enabling test pattern %d\n",__func__,testpattern);
		break;
	default:
		regval = 0;
		break;
	}
	if (mt9m024_write_reg(regaddr, regval))
		return -1;

#if 0
	/* auto exposure and HDR mode */
	if (autoexposure) {
		pr_info("%s: Enabling auto exposure\n",__func__);
		if (mt9m024_write_reg(APT_MT9M024_AE_CTRL_REG, 0x1b))
			return -1;
		if (hdrmode) {
			pr_info("%s: Enabling HDR mode\n",__func__);
			if (mt9m024_write_reg(APT_MT9M024_OPERATION_MODE_CTRL,
						0x28))
				return -1;
		}
	} else {
		regaddr = APT_MT9M024_AE_CTRL_REG;
		pr_info("%s: Auto exposure disabled\n",__func__);
		regval = 0x1a;
		if (mt9m024_write_reg(regaddr, regval))
			return -1;
	}
#endif

	/* Rotate */
	if (rotate) {
		pr_info("%s: Enabling 180° rotation\n",__func__);
		regaddr = APT_MT9M024_READ_MODE;
		if (mt9m024_read_reg(regaddr, &regval))
			return -1;
		regval |= (1<<15 | 1<<14);
		if (mt9m024_write_reg(regaddr, regval))
			return -1;
	}

	mt9m024_int_device.priv = &mt9m024_data;
	retval = v4l2_int_device_register(&mt9m024_int_device);

	if (!retval)
		pr_info("%s: Successfully probed\n",__func__);
	else
		pr_info("%s: Error\n",__func__);


	return retval;
}

/*!
 * mt9m024 I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int mt9m024_remove(struct i2c_client *client)
{
	v4l2_int_device_unregister(&mt9m024_int_device);
	return 0;
}

/*!
 * mt9m024 init function
 * Called by insmod mt9m024_camera.ko.
 *
 * @return  Error code indicating success or failure
 */
static __init int mt9m024_init(void)
{
	u8 err;

	pr_Dbg("%s:driver registration\n", __func__);
	err = i2c_add_driver(&mt9m024_i2c_driver);
	if (err != 0)
		pr_err("%s:driver registration failed, error=%d \n",
			__func__, err);

	return err;
}

/*!
 * MT9M024 cleanup function
 * Called on rmmod mt9m024_camera.ko
 *
 * @return  Error code indicating success or failure
 */
static void __exit mt9m024_clean(void)
{
	i2c_del_driver(&mt9m024_i2c_driver);
}

module_init(mt9m024_init);
module_exit(mt9m024_clean);

MODULE_AUTHOR("Harald Krapfenbauer, Bluetechnix");
MODULE_DESCRIPTION("ISM-MT9M024 Camera Module Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("CSI");

module_param(testpattern, int, 0);
module_param(autoexposure, int, 0);
module_param(hdrmode, int, 0);
module_param(sensorwidth, int, 0);
module_param(sensorheight, int, 0);
module_param(datawidth, int, 0);
module_param(rotate, int, 0);
module_param(binning, int, 0);

MODULE_PARM_DESC(testpattern, "Test pattern: 0=disable (default), 1=solid color, 2=color bar, 3=fade-to-gray color bar, 4=walking 1s");
MODULE_PARM_DESC(autoexposure, "Auto exposure: 0=disable, 1=enable (default)");
MODULE_PARM_DESC(hdrmode, "High dynamic range mode: 0=disable (default), 1=enable");
MODULE_PARM_DESC(sensorwidth, "Sensor width, default 1280");
MODULE_PARM_DESC(sensorheight, "Sensor height, default 960");
MODULE_PARM_DESC(datawidth, "Data width: 8 (default), 12 bits");
MODULE_PARM_DESC(rotate, "Rotate by 180°, 0=no (default), 1=yes");
MODULE_PARM_DESC(binning, "Enable digital binning for resolutions of VGA or smaller, 0=no, 1=yes (default)");
