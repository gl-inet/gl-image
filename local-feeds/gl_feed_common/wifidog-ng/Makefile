
# Copyright (C) 2018 Jianhui Zhao
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=wifidog-ng
PKG_VERSION:=2.0.0
PKG_RELEASE:=1

PKG_LICENSE:=LGPL-2.1
PKG_LICENSE_FILES:=LICENSE

PKG_MAINTAINER:=Jianhui Zhao <zhaojh329@gmail.com>

include $(INCLUDE_DIR)/package.mk




include $(INCLUDE_DIR)/kernel.mk

define KernelPackage/wifidog-ng
  SUBMENU:=Other modules
  TITLE:=Kernel module for wifidog-ng
  DEPENDS:=+kmod-nf-nat +kmod-ipt-ipset
  FILES:=$(PKG_BUILD_DIR)/wifidog-ng.ko
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/. $(PKG_BUILD_DIR)
endef

KERNEL_MAKE_FLAGS?= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)"

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	M="$(PKG_BUILD_DIR)"

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,wifidog-ng))
