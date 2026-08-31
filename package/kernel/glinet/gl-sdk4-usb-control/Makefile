#
# Copyright (C) 2025 Chengyang.li <chengyang.li@gl-inet.com>
#
# This is free software, licensed under the MIT.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include ./version.mk

PKG_NAME:=gl-sdk4-usb-control
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/gl-sdk4-usb-control
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=GL.iNet USB Power Control Driver
  FILES:=$(PKG_BUILD_DIR)/gl-sdk4-usb-control.ko
  AUTOLOAD:=$(call AutoLoad,99,gl-sdk4-usb-control,99)
endef

define KernelPackage/gl-sdk4-usb-control/description
  This driver controls USB power based on GPIO edge detection.
  When GPIO24 has rising edge, set GPIO23 high.
  When GPIO24 has falling edge, set GPIO23 to low level.
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


$(eval $(call KernelPackage,gl-sdk4-usb-control))
