include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-dns-mark
PKG_VERSION:=c80883038a1f
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  CATEGORY:=Kernel modules
  SUBMENU:=GL.iNet modules
  TITLE:=DNS marking module based on MAC and domain
  FILES:=$(PKG_BUILD_DIR)/dns_mark.ko
  AUTOLOAD:=$(call AutoLoad,30,dns_mark)
endef

define KernelPackage/$(PKG_NAME)/description
  Kernel module for marking DNS packets based on source MAC and query domain
endef

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	M="$(PKG_BUILD_DIR)"

define Build/Compile
	# Compile kernel module
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,$(PKG_NAME)))
