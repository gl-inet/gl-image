# Copyright (C) 2019 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-fan
PKG_VERSION:=763868c2bb9d
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  CATEGORY:=Kernel modules
  SUBMENU:=GL.iNet modules
  DEPENDS:=+kmod-hwmon-pwmfan
  TITLE:=GL.iNet fan kernel module
  FILES:=$(PKG_BUILD_DIR)/driver/gl_fan_driver.ko
  AUTOLOAD:=$(call AutoLoad,81,gl_fan_driver,1)
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/driver
	$(CP) ./src/driver/. $(PKG_BUILD_DIR)/driver/
endef

KERNEL_MAKE_FLAGS?= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)"

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	M="$(PKG_BUILD_DIR)/driver"

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,$(PKG_NAME)))
