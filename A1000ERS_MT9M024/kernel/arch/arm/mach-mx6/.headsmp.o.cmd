cmd_arch/arm/mach-mx6/headsmp.o := /opt/fsl-linaro-toolchain/bin/arm-none-linux-gnueabi-gcc -Wp,-MD,arch/arm/mach-mx6/.headsmp.o.d  -nostdinc -isystem /opt/fsl-linaro-toolchain/bin/../lib/gcc/arm-fsl-linux-gnueabi/4.6.2/include -I/home/mkajal/Desktop/CR/CR_2.1.2.8/src/kernel/linux-3.0.35/arch/arm/include -Iarch/arm/include/generated -Iinclude  -include include/generated/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-mx6/include -Iarch/arm/plat-mxc/include -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables  -D__LINUX_ARM_ARCH__=7 -march=armv7-a  -include asm/unified.h -msoft-float        -c -o arch/arm/mach-mx6/headsmp.o arch/arm/mach-mx6/headsmp.S

source_arch/arm/mach-mx6/headsmp.o := arch/arm/mach-mx6/headsmp.S

deps_arch/arm/mach-mx6/headsmp.o := \
  /home/mkajal/Desktop/CR/CR_2.1.2.8/src/kernel/linux-3.0.35/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
    $(wildcard include/config/thumb2/kernel.h) \
  include/linux/linkage.h \
  include/linux/compiler.h \
    $(wildcard include/config/sparse/rcu/pointer.h) \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  /home/mkajal/Desktop/CR/CR_2.1.2.8/src/kernel/linux-3.0.35/arch/arm/include/asm/linkage.h \
  include/linux/init.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/hotplug.h) \

arch/arm/mach-mx6/headsmp.o: $(deps_arch/arm/mach-mx6/headsmp.o)

$(deps_arch/arm/mach-mx6/headsmp.o):
