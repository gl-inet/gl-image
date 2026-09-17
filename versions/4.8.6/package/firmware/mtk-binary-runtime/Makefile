include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mtk-binary-runtime
PKG_VERSION:=8.3.1.3
PKG_RELEASE:=1
PKG_LICENSE:=Proprietary

include $(INCLUDE_DIR)/package.mk

# User-space payloads are already taken from the distributed image.
STRIP:=true
# Preserve the released bytes of the prebuilt kernel modules as well.
RSTRIP := $(subst $(SCRIPT_DIR)/strip-kmod.sh,$(CURDIR)/files/strip-runtime-kmod.sh,$(RSTRIP))

define Package/mtk-wifi-runtime
  SECTION:=firmware
  CATEGORY:=Firmware
  TITLE:=MTK MT7990/MT7991 Wi-Fi binary runtime for GL-BE10000
  DEPENDS:=@TARGET_mediatek_mt7987 +@DRIVER_WEXT_SUPPORT +kmod-cfg80211 +kmod-mediatek_hnat +blkid +nand-utils +lua +liblua +libuci +libuci-lua +libnl-tiny +libopenssl +libubox +libubus +libgcc +wpad-openssl
endef

define Package/mtk-wifi-runtime/description
 Binary-only MT7990/MT7991 Wi-Fi kernel modules, firmware, control programs and
 configuration scripts, including the optional mt7990_dbg diagnostic module.
 The payload is ABI-locked to Linux 5.4.281.
endef

define Package/mtk-phy-runtime
  SECTION:=firmware
  CATEGORY:=Firmware
  TITLE:=MT7987 2.5G PHY binary runtime
  DEPENDS:=@TARGET_mediatek_mt7987
endef

define Package/mtk-phy-runtime/description
 Binary-only MT7987 internal PHY driver and firmware. The AN8811HB driver is
 built from public source in the kernel.
 The driver payload is ABI-locked to Linux 5.4.281.
endef

define Package/mtk-network-runtime
  SECTION:=firmware
  CATEGORY:=Firmware
  TITLE:=MTK binary network extension modules for GL-BE10000
  DEPENDS:=@TARGET_mediatek_mt7987
endef

define Package/mtk-network-runtime/description
 Binary-only mapfilter, forwarding and QoS kernel modules.
 The payload is ABI-locked to Linux 5.4.281.
endef

define Package/mtk-tools-runtime
  SECTION:=firmware
  CATEGORY:=Firmware
  TITLE:=MTK shipping mesh and diagnostic tools for GL-BE10000
  DEPENDS:=@TARGET_mediatek_mt7987 +mtk-wifi-runtime +libcurl +ip-bridge +libunwind +libjson-c +libpthread +librt +libgcc +luci-lib-nixio
endef

define Package/mtk-tools-runtime/description
 Prebuilt MediaTek mesh, diagnostic and configuration tools from the
 corresponding shipping firmware. No GL proprietary application is included.
endef

define Build/Prepare
	$(if $(filter 5.4.281,$(LINUX_VERSION)),,$(error MTK binary runtime requires Linux 5.4.281, got $(LINUX_VERSION)))
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)/wifi $(PKG_BUILD_DIR)/phy $(PKG_BUILD_DIR)/network $(PKG_BUILD_DIR)/tools
	for ipk in $(CURDIR)/blobs/wifi/*.ipk; do \
		case $$$${ipk##*/} in hostapd-*|wpad-*|wpa-cli_*) continue;; esac; \
		tar -xOzf $$$$ipk ./data.tar.gz | tar -xzf - -C $(PKG_BUILD_DIR)/wifi; \
	done
	# wpa-cli is rebuilt from the release hostapd source package.
	rm -f $(PKG_BUILD_DIR)/wifi/usr/sbin/wpa_cli
	for ipk in $(CURDIR)/blobs/phy/*.ipk; do \
		tar -xOzf $$$$ipk ./data.tar.gz | tar -xzf - -C $(PKG_BUILD_DIR)/phy; \
	done
	for ipk in $(CURDIR)/blobs/network/*.ipk; do \
		tar -xOzf $$$$ipk ./data.tar.gz | tar -xzf - -C $(PKG_BUILD_DIR)/network; \
	done
	for ipk in $(CURDIR)/blobs/tools/*.ipk; do \
		tar -xOzf $$$$ipk ./data.tar.gz | tar -xzf - -C $(PKG_BUILD_DIR)/tools; \
	done
endef

define Build/Compile
endef

define Package/mtk-wifi-runtime/install
	$(CP) $(PKG_BUILD_DIR)/wifi/. $(1)/
	$(INSTALL_DIR) $(1)/usr/share/licenses/mtk-wifi-runtime
	$(INSTALL_DATA) ./files/LICENSE.BSD-3-Clause \
		./files/LICENSE.GPL-2.0 ./files/LICENSE.MTK ./files/NOTICE \
		$(1)/usr/share/licenses/mtk-wifi-runtime/
	$(INSTALL_DATA) ./files/tool-licenses/mwctl/LICENSE $(1)/usr/share/licenses/mtk-wifi-runtime/LICENSE.mwctl
endef

define Package/mtk-phy-runtime/install
	$(CP) $(PKG_BUILD_DIR)/phy/. $(1)/
endef

define Package/mtk-network-runtime/install
	$(CP) $(PKG_BUILD_DIR)/network/. $(1)/
endef

define Package/mtk-tools-runtime/install
	$(CP) $(PKG_BUILD_DIR)/tools/. $(1)/
	$(INSTALL_DIR) $(1)/usr/share/licenses/mtk-tools-runtime
	$(CP) ./files/tool-licenses/. $(1)/usr/share/licenses/mtk-tools-runtime/
endef

$(eval $(call BuildPackage,mtk-wifi-runtime))
$(eval $(call BuildPackage,mtk-phy-runtime))
$(eval $(call BuildPackage,mtk-network-runtime))
$(eval $(call BuildPackage,mtk-tools-runtime))
