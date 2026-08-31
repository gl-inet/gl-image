include $(TOPDIR)/rules.mk
include ./version.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-dns-mark
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  SUBMENU:=Network Support
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
