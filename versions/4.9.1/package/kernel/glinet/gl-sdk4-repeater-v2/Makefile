include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include version.mk

PKG_NAME:=gl-sdk4-repeater-v2
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

PKG_BUILD_DEPENDS:=lua/host lua5.4/host

PKG_CONFIG_DEPENDS := \
	CONFIG_GL_REPEATER_VENDOR_NL80211 \
	CONFIG_GL_REPEATER_VENDOR_MTK

include $(INCLUDE_DIR)/package.mk

define Package/gl-sdk4-repeater-v2
  SECTION:=base
  CATEGORY:=gl-sdk4
  TITLE:=Repeater management for Gl.inet
  DEPENDS:=+libuci-lua +libuci-lua54 +gl-oui-rpc +gl-sdk4-wifi +libiwinfo-lua54 \
           +lua-eco-ubus +lua-eco-log +lua-eco-nl80211 +lua-eco-ip +lua-eco-http \
		   +lua-cjson-lua5.4 +lua-htmlparser \
		   +kmod-gl-sdk4-repeater
endef

define Package/gl-sdk4-repeater-v2/config
if PACKAGE_gl-sdk4-repeater-v2
choice
  prompt "WiFi vendor"
  default GL_REPEATER_VENDOR_NL80211

    config GL_REPEATER_VENDOR_NL80211
	  bool "nl80211"

	config GL_REPEATER_VENDOR_MTK
	  bool "MTK Property"

endchoice
endif
endef

define KernelPackage/gl-sdk4-repeater
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=GL repeater
  FILES:=$(PKG_BUILD_DIR)/gl-repeater.ko
  AUTOLOAD:=$(call AutoLoad,30,gl-repeater,1)
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

ifneq ($(CONFIG_GL_REPEATER_VENDOR_NL80211),)
vendor:=nl80211
endif

ifneq ($(CONFIG_GL_REPEATER_VENDOR_MTK),)
vendor:=mtk
endif

ifeq ($(CONFIG_MTK_WIFI7_DRIVER),y)
	vendor:=mtkwifi7
endif

define Package/gl-sdk4-repeater-v2/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/config $(1)/etc/init.d \
		$(1)/usr/lib/oui-httpd/rpc $(1)/etc/gl-reset-network.d \
		$(1)/usr/local/lib/lua/5.4/gl $(1)/etc/uci-defaults \
		$(1)/usr/share/gl-ngx/websocket
	$(INSTALL_BIN) ./files/repeater.init $(1)/etc/init.d/repeater
	$(INSTALL_CONF) ./files/repeater.config $(1)/etc/config/repeater
	$(CP) ./files/reset-repeater.sh $(1)/etc/gl-reset-network.d/reset-repeater
	$(CP) ./files/repeater.default $(1)/etc/uci-defaults/gl-repeater
ifneq ($(CONFIG_PACKAGE_qca-cfg80211)$(CONFIG_PACKAGE_kmod-cfg80211),)
ifneq ($(CONFIG_TARGET_sdx75_glinet_gl-e5800),y)
	$(INSTALL_DIR) $(1)/etc/hotplug.d/ieee80211
	$(CP) ./files/etc/hotplug.d/99_fix-repeater-phy $(1)/etc/hotplug.d/ieee80211/99_fix-repeater-phy
endif
endif
	$(INSTALL_DATA) ./files/status-ws.lua $(1)/usr/share/gl-ngx/websocket/repeater.lua
	$(STAGING_DIR_HOSTPKG)/bin/luac5.4 -o $(1)/usr/sbin/gl-repeater ./files/gl-repeater.lua
	$(STAGING_DIR_HOSTPKG)/bin/luac -o $(1)/usr/lib/oui-httpd/rpc/repeater ./files/rpc/repeater.lua
	$(STAGING_DIR_HOSTPKG)/bin/luac5.4 -o $(1)/usr/local/lib/lua/5.4/gl/repeater.lua ./files/lib/repeater.lua
	$(STAGING_DIR_HOSTPKG)/bin/luac5.4 -o $(1)/usr/local/lib/lua/5.4/gl/repeater-portal.lua ./files/lib/repeater-portal.lua
	$(STAGING_DIR_HOSTPKG)/bin/luac5.4 -o $(1)/usr/local/lib/lua/5.4/gl/repeater-vendor.lua ./files/lib/repeater-$(vendor).lua

	$(INSTALL_DIR) $(1)/usr/share/gl-validator.d
	$(STAGING_DIR_HOSTPKG)/bin/luac -o $(1)/usr/share/gl-validator.d/repeater.lua ./files/rpc/validator.lua

	$(INSTALL_DIR) $(1)/etc/gl-switch.d
	$(INSTALL_BIN) ./files/switch-repeater.sh $(1)/etc/gl-switch.d/repeater.sh

	$(INSTALL_BIN) ./files/repeater-dbg.sh $(1)/usr/sbin
endef

$(eval $(call BuildPackage,gl-sdk4-repeater-v2))
$(eval $(call KernelPackage,gl-sdk4-repeater))
