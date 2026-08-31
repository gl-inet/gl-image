include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-firewall
PKG_VERSION:=1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel-defaults.mk

define Package/gl-sdk4-firewall
  SECTION:=base
  CATEGORY:=gl-sdk4
  TITLE:=GL.iNet port-forward kernel module
  DEPENDS:=@TARGET_mediatek_mt7987 +kmod-ipt-conntrack +kmod-nf-nat
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

define Package/gl-sdk4-firewall/install
	$(INSTALL_DIR) $(1)/lib/modules/$(LINUX_VERSION)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/port_forward.ko \
		$(1)/lib/modules/$(LINUX_VERSION)/port_forward.ko
endef

$(eval $(call BuildPackage,gl-sdk4-firewall))
