# SPDX-License-Identifier: GPL-3.0-only
#
# Copyright (C) 2021-2022 ImmortalWrt.org

include $(TOPDIR)/rules.mk

PKG_NAME:=mptun-core

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/luochongjun/mptcptun.git
PKG_SOURCE_DATE:=2024-01-04
PKG_SOURCE_VERSION:=52c8e7080742dc9b422f583b9335dca7e95854e3
PKG_MIRROR_HASH:=3b92bfdabbd1c989439e626f709eead86a5f2ba973e2d81b8424f8bd5a2b04d2

PKG_LICENSE:=MIT
PKG_LICENSE_FILES:=LICENSE
PKG_MAINTAINER:=Tianling Shen <cnsztl@immortalwrt.org>

PKG_BUILD_DEPENDS:=golang/host
PKG_BUILD_PARALLEL:=1
PKG_USE_MIPS16:=0

GO_PKG:=github.com/v2fly/v2ray-core/v5
GO_PKG_BUILD_PKG:=$(GO_PKG)/main
#GO_PKG_TAGS:=gc
GO_PKG_LDFLAGS_X:= \
	$(GO_PKG).build=OpenWrt \
	$(GO_PKG).version=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include ../lang/golang/golang-package.mk

define Package/mptun/template
  TITLE:=A platform for building proxies to bypass network restrictions
  SECTION:=net
  CATEGORY:=Network
endef

define Package/$(PKG_NAME)
  $(call Package/mptun/template)
  DEPENDS:=$(GO_ARCH_DEPENDS) +ca-bundle +kmod-ipt-tproxy  +bind-dig
endef

define Package/$(PKG_NAME)/install
	$(call GoPackage/Package/Install/Bin,$(PKG_INSTALL_DIR))

	$(INSTALL_DIR) $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/main $(1)/usr/bin/mptun

	$(INSTALL_DIR) $(1)/etc/mptun/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/release/config/config.json $(1)/etc/mptun/
	$(INSTALL_DATA) $(CURDIR)/files/basic.json $(1)/etc/mptun/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
