
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-black_white_list
PKG_VERSION:=1feaf4afd4b8
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only

PKG_BUILD_DEPENDS:=lua/host

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  CATEGORY:=Kernel modules
  SUBMENU:=GL.iNet modules
  TITLE:=glinet black_white_list moudle
  FILES:=$(PKG_BUILD_DIR)/gl-sdk4-black_white_list.ko
  AUTOLOAD:=$(call AutoLoad,30,gl-sdk4-black_white_list,1)
endef

define KernelPackage/gl-sdk4-black_white_list/description
	Kernel module for gl-sdk4-black_white_list
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


$(eval $(call KernelPackage,$(PKG_NAME)))
