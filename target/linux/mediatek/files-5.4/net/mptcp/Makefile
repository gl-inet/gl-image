# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_MPTCP) += mptcp.o

mptcp-y := compat.o protocol.o subflow.o options.o token.o crypto.o ctrl.o pm.o diag.o \
	   mib.o pm_netlink.o sockopt.o
obj-$(CONFIG_SYN_COOKIES) += syncookies.o
#obj-$(CONFIG_INET_MPTCP_DIAG) += mptcp_diag.o

#mptcp-objs := compat.o protocol.o subflow.o options.o token.o crypto.o ctrl.o pm.o diag.o \
	   mib.o pm_netlink.o sockopt.o

#obj-m := mptcp.o
#obj-m += mptcp_diag.o
# -DCONFIG_MPTCP_IPV6=1 -DCONFIG_IPV6=1
#KBUILD_CFLAGS += -Wno-unused-variable -Wno-unused-function -Wno-uninitialized -DCONFIG_MPTCP=1 -DCONFIG_SKB_EXTENSIONS -DCONFIG_COMPAT
#PWD := $(shell pwd)
#KDIR := /home/glinet/work/tmp/gl-infra-builder/mt7981/build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_mt7981/linux-5.4.211
#TOOS_CHAIN=/home/glinet/work/tmp/gl-infra-builder/mt7981/staging_dir/toolchain-aarch64_cortex-a53_gcc-8.4.0_musl/bin/aarch64-openwrt-linux-musl-

#all:
#	clear
#	make -C $(KDIR) M=$(PWD) modules CROSS_COMPILE=$(TOOS_CHAIN) ARCH=arm64
#clean:
#	rm -f *.ko *.o *.mod.o *.mod.c *.symvers *.bak *.order *.cmd *.mod .*.cmd .*.d
