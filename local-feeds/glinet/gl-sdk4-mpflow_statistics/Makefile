
include $(TOPDIR)/rules.mk
include ./version.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-mpflow_statistics
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  SECTION:=base
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=glinet mpflow statistics
  FILES:=$(PKG_BUILD_DIR)/gl-mpflow.ko 
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

