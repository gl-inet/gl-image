include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mtk-be14000-binary-runtime
PKG_VERSION:=8.3.1.3
PKG_RELEASE:=1
PKG_LICENSE:=Proprietary
PKGARCH:=aarch64_cortex-a53

include $(INCLUDE_DIR)/package.mk

define KernelPackage/be14000-mtk-wifi-abi
  SUBMENU:=Wireless Drivers
  TITLE:=Kernel ABI support for MTK Wi-Fi binary runtime
  DEPENDS:=@TARGET_mediatek_mt7988
  KCONFIG:= \
    CONFIG_INET_ESP=y \
    CONFIG_INET_ESP_OFFLOAD=y \
    CONFIG_INET6_ESP=y \
    CONFIG_INET6_ESP_OFFLOAD=y
endef

define Package/be14000-mtk-wifi-runtime
  SECTION:=firmware
  CATEGORY:=Firmware
  TITLE:=MTK MT7990/MT7991 Wi-Fi binary runtime for GL-BE14000
  DEPENDS:=@TARGET_mediatek_mt7988 +kmod-be14000-mtk-wifi-abi +kmod-cfg80211 +kmod-mediatek_hnat +blkid +nand-utils +lua +liblua +libuci +libuci-lua +libnl-tiny +libopenssl +libubox +libubus +luci-lib-nixio
  PROVIDES:=hostapd wpa-supplicant wpad
endef

define Package/be14000-mtk-wifi-runtime/description
 Binary-only MT7990/MT7991 Wi-Fi kernel modules, firmware, control programs and
 configuration scripts, including the optional mt7990_dbg diagnostic module.
 The payload is ABI-locked to Linux 5.4.281.
endef

define Package/be14000-mtk-phy-runtime
  SECTION:=firmware
  CATEGORY:=Firmware
  TITLE:=RTL8261 and YT92xx binary network PHY runtime
  DEPENDS:=@TARGET_mediatek_mt7988 +kmod-libphy +kmod-swconfig
endef

define Package/be14000-mtk-phy-runtime/description
 Binary-only RTL8261 and YT92xx PHY and switch drivers.
 The driver payload is ABI-locked to Linux 5.4.281.
endef

define Package/be14000-mtk-network-runtime
  SECTION:=firmware
  CATEGORY:=Firmware
  TITLE:=MTK binary network extension modules for GL-BE14000
  DEPENDS:=@TARGET_mediatek_mt7988
endef

define Package/be14000-mtk-network-runtime/description
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

define Package/be14000-mtk-wifi-runtime/install
	$(CP) $(PKG_BUILD_DIR)/wifi/. $(1)/
	$(INSTALL_DIR) $(1)/usr/share/licenses/mtk-wifi-runtime
	$(INSTALL_DATA) ./files/LICENSE.BSD-3-Clause \
		./files/LICENSE.GPL-2.0 ./files/LICENSE.MTK ./files/NOTICE \
		$(1)/usr/share/licenses/mtk-wifi-runtime/
endef

define Package/be14000-mtk-phy-runtime/install
	$(CP) $(PKG_BUILD_DIR)/phy/. $(1)/
endef

define Package/be14000-mtk-network-runtime/install
	$(CP) $(PKG_BUILD_DIR)/network/. $(1)/
endef

$(eval $(call KernelPackage,be14000-mtk-wifi-abi))
$(eval $(call BuildPackage,be14000-mtk-wifi-runtime))
$(eval $(call BuildPackage,be14000-mtk-phy-runtime))
$(eval $(call BuildPackage,be14000-mtk-network-runtime))
