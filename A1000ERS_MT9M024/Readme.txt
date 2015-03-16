This release support kernel version 3.0.35 for iMX6 processor and LTIB version 4.0.0._130424

linux-3.0.35.tar.gz (Download Fresh kernel source code)
L3.0.35_4.0.0_130424_source.tar.gz (LTIB from Freescale)
IMX_MMCODEC_3.0.35_4.0.0_BUNDLE_CODA.tar.gz (Multimedia support from Freescale)
IMX_AACPD_3.0.7.tar.gz (AAC support from Freescacle)
i.MX6/A1000ERS_MT9M024/LTIB/mx6-ltib-patchset-2.0.0.tar.gz(Kernel patches and Gstreamer support for MT9M024 color/monochrome)
i.MX6/A1000ERS_MT9M024/test_app/mxc_v4l2_still.c (snapshot capture test application)
i.MX6/A1000ERS_MT9M024/script/6x_bootscript (boot script for SD booting)
i.MX6/A1000ERS_MT9M024/patch/2509-mt9m024.patch (kernel change patch)
fsl-linaro-toolchain.tar.gz ( Download Cross compiler for compiling kernel alone)


1. Kernel code modifications
Following are the files modified/added for adding support for MT9M024 sensor driver is present under kernel folder.
drivers/media/video/mxc/capture/mt9m024.c
drivers/media/video/mxc/capture/Kconfig
drivers/media/video/mxc/capture/mt9m024.h
drivers/media/video/mxc/capture/Makefile
drivers/media/video/mxc/capture/mt9m024_pll.h
arch/arm/plat-mxc/include/mach/ipu-v3.h
arch/arm/plat-mxc/include/mach/ipu.h
drivers/media/video/mxc/capture/ipu_csi_enc.c
drivers/media/video/mxc/capture/ipu_prp_enc.c
drivers/media/video/mxc/capture/ipu_still.c
drivers/media/video/mxc/capture/mxc_v4l2_capture.c
drivers/mxc/ipu3/ipu_common.c
drivers/mxc/ipu3/ipu_param_mem.h
include/linux/ipu.h
drivers/mxc/ipu3/ipu_capture.c
arch/arm/configs/imx6_defconfig
arch/arm/mach-mx6/board-mx6q_sabrelite.c
drivers/media/video/mxc/capture/mt9m024_seque

2.Compiling Linux kernel
Kernel source used is linux-3.0.35
Apply the kernel patch provided
• patch -p1 <~i.MX6/A1000ERS_MT9M024/patch/2509-mt9m024.patch
• cp arch/arm/configs/imx6_defconfig .config
Get the appropriate cross compiler
• cd <cross_compiler_folder>
• tar -xzf fsl-linaro-toolchain.tar.gz
• sudo cp -rf fsl-linaro-toolchain /opt
• cd linux-3.0.35
Compile the kernel
• export PATH=/opt/fsl-linaro-toolchain/bin/:$PATH
• make ARCH=arm CROSS_COMPILE=/opt/fsl-linaro-toolchain/bin/arm-none-linux-gnueabi-uImage
uImage is formed under arch/arm/boot/ directory


3. SD card preparation steps ( from prebuilt filesystem )

Take a fresh SD card (format the SD card if it is not a fresh one) and create a single partition in it. Steps for
creating a single partition are given below.
Insert the SD card into your PC . Execute the below command from terminal

sudo umount /dev/sdx (assuming that your SD card shows up as /dev/sdx, if it uses different
location then change command accordingly)

sudo fdisk /dev/sdx 

After creating a single partition execute the following commands
sudo mkfs.ext3 -L ltib /dev/sdx1
sudo tar -xf rootfs.tar.bz2 -C /media/ltib/
sudo udisks --mount /dev/sdx1

After writing filesystem to SD card, copy the 6x_bootscript file provided in the release folder to /media/ltib
sudo cp i.MX6/A1000ERS_MT9M024/script/6x_bootscript /media/ltib
 uImage is formed under ~/CR/src/kernel/linux-3.0.35/arch/arm/boot/uImage. Copy that uImage to 'boot' directory of SD card.
sudo cp i.MX6/A1000ERS_MT9M024/script/linux-3.0.35/arch/arm/boot/uImage /media/ltib/boot/
sync && sudo umount /media/ltib

4.Test Application compilation
	Go to test application directory and compile it
cd i.MX6/A1000ERS_MT9M024/test_app
/opt/fsl-linaro-toolchain/bin/arm-none-linux-gnueabi-gcc mxc_v4l2_still.c -o mxc_v4l2_still
Mount the SD card if it is not mounted.
sudo udisks –mount /dev/sdx1
sudo cp mxc_v4l2_still /media/ltib/bin
Copy 'mxc_v4l2_still' binary created in i.MX6/A1000ERS_MT9M024/test_app to SD card 'bin' directory


5.Still Capture Test (monochrome/color)
After SD card preparation,
Insert the SD card into the board and switch on the power supply (Assuming SPI flash of Saberlite
board contains valid u-boot). Once the system is booted up,then execute the commands given below
from Linux console, it will create 'still.yuv' file in '/root/' directory.

rmmod mt9m024-camera
modprobe mt9m024-camera testpattern=0 datawidth=12 autoexposure=1
/bin/mxc_v4l2_still -w 1280 -h 960 -fr 45 -f Y16
Copy the image still.yuv to a windows PC and rename it with .raw extension.
Open the Aptina Devware application/Use any RAW file viewer
	File->open Image or v ideo file->select the .raw extension file
Select 'monochrome' if still file is monochrome, else no change.
Change Image format to Bayer 16










