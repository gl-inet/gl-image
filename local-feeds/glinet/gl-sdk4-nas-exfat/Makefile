#
# Copyright (C) 2013 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=gl-sdk4-nas-exfat
PKG_VERSION:=1.6
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
	SECTION:=base
	CATEGORY:=gl-sdk4
	TITLE:=lib exfat
	#DEPENDS:=+libuci +libjson-c +libiwinfo +libcurl +libubox +libubus +libblobmsg-json
	DEPENDS:=+libpthread
endef

define Package/$(PKG_NAME)/description
lib exfat
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/gl
	$(CP) $(PKG_BUILD_DIR)/include/* $(1)/usr/include/gl
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libexfat.so $(1)/usr/lib/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/usr/lib/gl
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libexfat.so $(1)/usr/lib/gl
	$(LN) /usr/lib/gl/libexfat.so $(1)/usr/lib/

	$(INSTALL_BIN) $(PKG_BUILD_DIR)/fsck.exfat $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mkfs.exfat $(1)/usr/bin
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
