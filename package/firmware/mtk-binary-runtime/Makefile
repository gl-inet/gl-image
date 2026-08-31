include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mtk-binary-runtime
PKG_VERSION:=8.3.1.3
PKG_RELEASE:=1
PKG_LICENSE:=Proprietary
PKGARCH:=aarch64_cortex-a53

include $(INCLUDE_DIR)/package.mk

define Package/mtk-wifi-runtime
  SECTION:=firmware
  CATEGORY:=Firmware
  TITLE:=MTK MT7990/MT7991 Wi-Fi binary runtime for GL-BE10000
  DEPENDS:=@TARGET_mediatek_mt7987 +kmod-cfg80211 +kmod-mediatek_hnat +blkid +nand-utils +lua +liblua +libuci +libuci-lua +libnl-tiny +libopenssl +libubox +libubus +luci-lib-nixio
  PROVIDES:=hostapd wpa-supplicant wpad
endef

define Package/mtk-wifi-runtime/description
 Binary-only MT7990/MT7991 Wi-Fi kernel modules, firmware, control programs and
 configuration scripts, including the optional mt7990_dbg diagnostic module.
 The payload is ABI-locked to Linux 5.4.281.
endef

define Package/mtk-phy-runtime
  SECTION:=firmware
  CATEGORY:=Firmware
  TITLE:=MT7987 and AN8811HB 2.5G PHY binary runtime
  DEPENDS:=@TARGET_mediatek_mt7987
endef

define Package/mtk-phy-runtime/description
 Binary-only MT7987 internal and Airoha AN8811HB 2.5G PHY drivers and firmware.
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

define Build/Prepare
	$(if $(filter 5.4.281,$(LINUX_VERSION)),,$(error MTK binary runtime requires Linux 5.4.281, got $(LINUX_VERSION)))
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)/wifi $(PKG_BUILD_DIR)/phy $(PKG_BUILD_DIR)/network
	for ipk in $(CURDIR)/blobs/wifi/*.ipk; do \
		tar -xOzf $$$$ipk ./data.tar.gz | tar -xzf - -C $(PKG_BUILD_DIR)/wifi; \
	done
	for ipk in $(CURDIR)/blobs/phy/*.ipk; do \
		tar -xOzf $$$$ipk ./data.tar.gz | tar -xzf - -C $(PKG_BUILD_DIR)/phy; \
	done
	for ipk in $(CURDIR)/blobs/network/*.ipk; do \
		tar -xOzf $$$$ipk ./data.tar.gz | tar -xzf - -C $(PKG_BUILD_DIR)/network; \
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
endef

define Package/mtk-phy-runtime/install
	$(CP) $(PKG_BUILD_DIR)/phy/. $(1)/
endef

define Package/mtk-network-runtime/install
	$(CP) $(PKG_BUILD_DIR)/network/. $(1)/
endef

$(eval $(call BuildPackage,mtk-wifi-runtime))
$(eval $(call BuildPackage,mtk-phy-runtime))
$(eval $(call BuildPackage,mtk-network-runtime))
