
include $(TOPDIR)/rules.mk
include ./version.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-kmwan
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

PKG_BUILD_DEPENDS:=lua/host

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  SECTION:=base
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=glinet kmwan
  FILES:=$(PKG_BUILD_DIR)/kmwan.ko 
  DEPENDS:=+kmod-nf-conntrack +kmod-nf-nat
  AUTOLOAD:=$(call AutoLoad,40,kmwan,1)
endef

EXTRA_CFLAGS += -Wno-frame-larger-than

KERNEL_MAKE_FLAGS?= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)"

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	M="$(PKG_BUILD_DIR)"

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		modules
endef


 

$(eval $(call KernelPackage,$(PKG_NAME)))

