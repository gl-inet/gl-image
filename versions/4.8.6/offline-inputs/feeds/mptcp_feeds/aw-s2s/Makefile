include $(TOPDIR)/rules.mk
include ./version.mk

PKG_NAME:=aw-s2s
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1
PKG_BUILD_DEPENDS:=lua/host

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=base
  CATEGORY:=gl-sdk4
  TITLE:= AstroWarp s2s API
endef

Build/Compile=

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin/
	$(INSTALL_BIN) $(CURDIR)/files/mptcp-proxy.nft $(1)/usr/bin/
	$(INSTALL_BIN) $(CURDIR)/files/proxy-route.sh $(1)/usr/bin/
	$(INSTALL_BIN) $(CURDIR)/files/get_mplinkip.sh $(1)/usr/bin/
	$(INSTALL_BIN) $(CURDIR)/files/get_mpsrv_delay.sh $(1)/usr/bin/

	$(INSTALL_DIR) $(1)/etc/mptun/
	$(INSTALL_DATA) $(CURDIR)/files/dnsmasq.conf.mptun $(1)/etc/

	$(INSTALL_DIR) $(1)/etc/config/
	$(INSTALL_CONF) $(CURDIR)/files/mptun.conf $(1)/etc/config/mptun
	$(INSTALL_DIR) $(1)/etc/init.d/
	$(INSTALL_BIN) $(CURDIR)/files/mptun.init $(1)/etc/init.d/mptun

	$(INSTALL_DIR) $(1)/lib/functions/
	$(INSTALL_BIN) $(CURDIR)/files/mptun.sh $(1)/lib/functions/

	$(INSTALL_DIR) $(1)/usr/lib/oui-httpd/rpc
	$(CP) $(CURDIR)/files/mptun.lua $(1)/usr/lib/oui-httpd/rpc/mptun
	$(STAGING_DIR_HOSTPKG)/bin/luac -o $(1)/usr/lib/oui-httpd/rpc/mptun $(1)/usr/lib/oui-httpd/rpc/mptun

	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) $(CURDIR)/files/51-aw_upgrade_compat.sh  $(1)/etc/uci-defaults

	$(INSTALL_DIR) $(1)/etc/gl_crontabs/crontabs.d/
	$(INSTALL_BIN) $(CURDIR)/files/aw_arp_scan $(1)/etc/gl_crontabs/crontabs.d/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
