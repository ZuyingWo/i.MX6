Support:
--------
1. VGA/NTSC/PAL resolutions
2. 640*480 Resolution 
3. YUV422 format


Code changes:
-------------
Files needs to replace/merge with existing freescale kernel(changes can be find with mkajal@aptina.com)
•
drivers/media/video/mxc/capture/mt9v129.c
Sensor driver code.
•
drivers/media/video/mxc/capture/Kconfig
Kconfig file changes for configuring mt9v129 sensor
•
drivers/media/video/mxc/capture/Makefile
Added sensor in makefile.
•
drivers/media/video/mxc/capture/mxc_v4l2_capture.c
Added support for NTSC/PAL/VGA resolution selection support.
•
arch/arm/mach-mx6/board-mx6q_sabrelite.c
Added sensor I2C address.
•
arch/arm/configs/imx6_android_defconfig
Added MT9V129 in default configuration file.



Configuration:
--------------
make ARCH=arm CROSS_COMPILE=arm-eabi- imx6_android_defconfig

Compile:
--------
make ARCH=arm CROSS_COMPILE=arm-eabi- uImage

cp uImage /BOOT/uImage

Boot your device and see Aptina MT9V129 is detectet or not.

Run:
----
Use V4L2 app to verify preview. See JIRA : DRV-8 for v4L2 app
Compilation Step
----------------------
Copy Makefile and capture_mmap.c to a directory and then give below command from that directory
make CROSS_COMPILE=<cross compiler path>
Above step will create 'capture_mmap' binary, copy this to iMX6 filesystem.

Running Test Application
--------------------------------
Run below command from the directory containing 'capture_mmap'
./capturer_mmap -w 640*480 >test.yuv
VGA resolution Video will be saved to test.yuv file.

Test:
-----
This driver is tested on boundary devicel-imx_jb4.3_1.0.0-beta with kernel 3.0.35
