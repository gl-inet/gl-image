
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include ./version.mk

PKG_NAME:=gl-sdk4-black_white_list
# Source snapshot: b9ed2946568527b4be0e956818eede618b6f3406
PKG_VERSION:=git-2026.162.24246-b9ed294
PKG_RELEASE:=1

PKG_BUILD_DEPENDS:=lua/host

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
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

define KernelPackage/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/etc/config $(1)/usr/lib/oui-httpd/rpc $(1)/www/js $(1)/usr/share/gl-upload.d
	$(INSTALL_CONF) ./files/gl-black_white_list.config $(1)/etc/config/gl-black_white_list
	$(INSTALL_BIN) ./files/gl-black_white_list.init $(1)/etc/init.d/gl-black_white_list
	$(STAGING_DIR_HOSTPKG)/bin/luac -o $(1)/usr/lib/oui-httpd/rpc/black_white_list ./files/black_white_list.lua
	$(CP) ./files/mac-template.csv $(1)/www/js
	$(INSTALL_BIN) ./files/black_white_list.upload	$(1)/usr/share/gl-upload.d/black_white_list
endef

$(eval $(call KernelPackage,$(PKG_NAME)))
