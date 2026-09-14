include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-port-forward
PKG_VERSION:=331301466735
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define KernelPackage/gl-sdk4-port-forward
  SUBMENU:=GL.iNet modules
  TITLE:=GL.iNet port forwarding kernel module
  DEPENDS:=+kmod-nf-conntrack +kmod-nf-nat
  FILES:=$(PKG_BUILD_DIR)/port_forward.ko
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/. $(PKG_BUILD_DIR)/
endef

define Build/Compile
	+$(MAKE) -C "$(LINUX_DIR)" $(KERNEL_MAKE_FLAGS) \
		M="$(PKG_BUILD_DIR)" modules
endef

$(eval $(call KernelPackage,gl-sdk4-port-forward))
