#
# Copyright (C) 2019 Jianhui Zhao <zhaojh329@gmail.com>
#
# This is free software, licensed under the GNU General Public License v2.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-hw-info
PKG_VERSION:=763868c2bb9d
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define KernelPackage/gl-sdk4-hw-info
  CATEGORY:=Kernel modules
  SUBMENU:=GL.iNet modules
  TITLE:=GL.iNet hardware information
  FILES:=$(PKG_BUILD_DIR)/gl-sdk4-hw-info.ko
  DEPENDS:=+uboot-envtools
  AUTOLOAD:=$(call AutoLoad,01,gl-sdk4-hw-info,1)
  KCONFIG:=
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


$(eval $(call KernelPackage,gl-sdk4-hw-info))
