#
# Copyright (C) 2017 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(TOPDIR)/include/version.mk
include ./version.mk

PKG_NAME:=gl-sdk4-s2s
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

PKG_BUILD_DEPENDS:=lua/host

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/gl-sdk4-s2s
  SECTION:=base
  CATEGORY:=gl-sdk4
  TITLE:= Oui Support for s2s
  DEPENDS += +jansson  +gl-sdk4-uci +gl-sdk4-utils +wireguard-tools
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/gl-sdk4-s2s/install
	$(INSTALL_DIR) $(1)/usr/lib/oui-httpd/rpc
	$(CP) $(PKG_BUILD_DIR)/s2s.so $(1)/usr/lib/oui-httpd/rpc

	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/echo_server $(1)/usr/bin/echo_server

	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/gl_s2s.defaults $(1)/etc/uci-defaults/gl-s2s.defaults
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/gl_s2s.init $(1)/etc/init.d/gl_s2s
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_BIN) ./files/gl_s2s.config $(1)/etc/config/gl_s2s

	$(INSTALL_DIR) $(1)/usr/share/gl-validator.d
	$(STAGING_DIR_HOSTPKG)/bin/luac -o $(1)/usr/share/gl-validator.d/s2s.lua ./files/validator.lua
endef

$(eval $(call BuildPackage,gl-sdk4-s2s))
