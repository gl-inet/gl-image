include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel-defaults.mk

PKG_NAME:=gl-sdk4-port-forward
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only

define KernelPackage/$(PKG_NAME)
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=GL SDK4 port forward kernel module
  DEPENDS:=+kmod-ipt-conntrack +kmod-nf-nat +kmod-nf-nathelper
  FILES:=$(PKG_BUILD_DIR)/port_forward.ko
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" $(KERNEL_MAKE_FLAGS) M="$(PKG_BUILD_DIR)" modules
endef

$(eval $(call KernelPackage,$(PKG_NAME)))
