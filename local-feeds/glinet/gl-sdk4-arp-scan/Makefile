# Simplified Makefile for local main.go compilation
include $(TOPDIR)/rules.mk

PKG_NAME:=gl-sdk4-arp-scan
PKG_VERSION:=1.0.1
PKG_RELEASE:=1

PKG_MAINTAINER:=Clan Xie <yongping.xie@gl-inet.com>
PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=LICENSE

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_BUILD_DEPENDS:=golang/host
PKG_BUILD_PARALLEL:=1
PKG_USE_MIPS16:=0

GO_PKG:=\
	gl-arp-scan

include $(INCLUDE_DIR)/package.mk
include $(TOPDIR)/feeds/gl_feed*_common/golang/golang-package.mk

define Package/gl-sdk4-arp-scan
  SECTION:=base
  CATEGORY:=gl-sdk4
  TITLE:=ARP scanning for AstroWrap.
  URL:=https://www.gl-inet.com
  DEPENDS:=$(GO_ARCH_DEPENDS)
endef

define Package/gl-sdk4-arp-scan/description
  This is a simple Go program for ARP scanning.
endef

define Package/gl-sdk4-arp-scan/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(GO_PKG_BUILD_BIN_DIR)/gl-arp-scan $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,gl-sdk4-arp-scan))
