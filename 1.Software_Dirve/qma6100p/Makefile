SDK_DIR = /home/alen/VisonFive2_SDK/VisionFive2
KERN_DIR = $(SDK_DIR)/work/linux
export ARCH=riscv
# 指定交叉编译工具链前缀
export CROSS_COMPILE = /home/alen/VisonFive2_SDK/VisionFive2/work/buildroot_initramfs/host/bin/riscv64-buildroot-linux-gnu-

obj-m += qma6100p.o  # 假设你的源文件是 motor.c

all:
	$(MAKE) -C $(KERN_DIR) M=$(PWD) ARCH=riscv CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KERN_DIR) M=$(PWD) ARCH=riscv CROSS_COMPILE=$(CROSS_COMPILE) modules clean
	rm -rf modules.order
#/home/alen/VisonFive2_SDK/VisionFive2/work/buildroot_rootfs/host/bin/riscv64-buildroot-linux-gnu-gcc