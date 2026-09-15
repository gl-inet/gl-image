
include $(TOPDIR)/rules.mk
include ./version.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-parental-control
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

PKG_BUILD_DEPENDS:=lua/host

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  SECTION:=base
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=glinet parental control
  FILES:=$(PKG_BUILD_DIR)/parental_control.ko 
  DEPENDS:=+kmod-ipt-conntrack
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

define KernelPackage/$(PKG_NAME)/conffiles
/etc/parental_control_v2/
endef

 
define KernelPackage/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin  $(1)/etc/init.d $(1)/lib/functions $(1)/usr/share/gl-validator.d $(1)/etc
	$(INSTALL_BIN) ./files/pc_schedule.sh $(1)/usr/bin/pc_schedule
	$(INSTALL_BIN) ./files/parental_control.init $(1)/etc/init.d/parental_control
	$(INSTALL_BIN) ./files/parental_control.sh $(1)/lib/functions

	$(INSTALL_DIR) $(1)/usr/lib/oui-httpd/rpc $(1)/etc/parental_control_v2  $(1)/etc/config
	$(CP) ./files/parental_control.config $(1)/etc/config/parental_control_v2
	$(CP) ./files/parental_control_apps.config $(1)/etc/config/parental_control_apps
	$(CP) ./apps/app-metadata.json $(1)/etc/app-metadata.json
	$(CP) ./apps/apps.conf $(1)/etc/apps.conf
	$(CP) ./apps/categories.json $(1)/etc/categories.json
	$(STAGING_DIR_HOSTPKG)/bin/luac -o $(1)/usr/lib/oui-httpd/rpc/parental-control ./files/parental_control.lua
	$(STAGING_DIR_HOSTPKG)/bin/luac -o $(1)/usr/bin/apps_domain_list.lua ./files/apps_domain_list.lua
	$(STAGING_DIR_HOSTPKG)/bin/luac -o $(1)/usr/share/gl-validator.d/parental-control.lua ./files/validator.lua
endef

$(eval $(call KernelPackage,$(PKG_NAME)))

