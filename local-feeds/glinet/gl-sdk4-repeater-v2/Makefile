include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-repeater-v2
PKG_VERSION:=1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/gl-sdk4-repeater
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=GL.iNet repeater kernel module
  FILES:=$(PKG_BUILD_DIR)/gl-repeater.ko
  AUTOLOAD:=$(call AutoLoad,30,gl-repeater,1)
endef

KERNEL_MAKE_FLAGS?= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)"

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	M="$(PKG_BUILD_DIR)"

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/. $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,gl-sdk4-repeater))
