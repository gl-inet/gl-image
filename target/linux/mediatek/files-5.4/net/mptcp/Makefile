# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_MPTCP) += mptcp.o

mptcp-y := compat.o protocol.o subflow.o options.o token.o crypto.o ctrl.o pm.o diag.o \
		   mib.o pm_netlink.o sockopt.o
obj-$(CONFIG_SYN_COOKIES) += syncookies.o
