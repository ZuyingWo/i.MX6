/* mt9d115_mipi Camera
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MT9D115_MIPI_H__
#define __MT9D115_MIPI_H__

#define MT9D115_I2C_ADDR				(0x78>>1)

#define MT9D115_CHIP_ID_REG				0x0000
#define MT9D115_CHIP_ID					0x2580
#define MT9D115_RST_CTRL_REG			0x001A
#define MT9D115_PLL_DIVIDERS			0x0010
#define MT9D115_PLL_P_DIVIDERS			0x0012
#define MT9D115_PLL_CONTROL				0x0014
#define MT9D115_CTRL_STATUS				0x0018
#define MT9D115_MCU_ADDRESS				0x098C
#define MT9D115_MCU_DATA_0				0x0990
#define MT9D115_MCU_DATA_1				0x0992
#define MT9D115_MCU_DATA_2				0x0994
#define MT9D115_MCU_DATA_3				0x0996
#define MT9D115_MCU_DATA_4				0x0998
#define MT9D115_MCU_DATA_5				0x099A
#define MT9D115_MCU_DATA_6				0x099C
#define MT9D115_MCU_DATA_7				0x099E

#define V4L2_CID_TEST_PATTERN           (V4L2_CID_USER_BASE | 0x1001)
#define V4L2_CID_GAIN_RED				(V4L2_CID_USER_BASE | 0x1002)
#define V4L2_CID_GAIN_GREEN1			(V4L2_CID_USER_BASE | 0x1003)
#define V4L2_CID_GAIN_GREEN2			(V4L2_CID_USER_BASE | 0x1004)
#define V4L2_CID_GAIN_BLUE				(V4L2_CID_USER_BASE | 0x1005)
#define V4L2_CID_ANALOG_GAIN			(V4L2_CID_USER_BASE | 0x1006)

#endif
