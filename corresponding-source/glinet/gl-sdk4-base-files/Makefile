#
# Copyright (C) 2011-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include version.mk

PKG_NAME:=gl-sdk4-base-files
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/gl-sdk4-base-files
  SECTION:=base
  CATEGORY:=gl-sdk4
  TITLE:=Base filesystem for gl-inet
  DEPENDS:=+kmod-gl-sdk4-hw-info +conntrack
endef

define Package/gl-sdk4-base-files/config
        select BUSYBOX_CUSTOM
        select BUSYBOX_CONFIG_USLEEP
endef

Build/Compile=

ifneq ($(CONFIG_TARGET_ipq806x)$(CONFIG_TARGET_ipq_ipq40xx)$(CONFIG_TARGET_ipq_ipq60xx_64),)
  INSTALL_VARIANT:=install
else
  INSTALL_VARIANT:=install-overlay
endif

define Package/gl-sdk4-base-files/$(INSTALL_VARIANT)
	$(CP) ./files/etc $(1)/
	$(CP) ./files/lib $(1)/
	$(CP) ./files/sbin $(1)/
	$(CP) ./files/usr $(1)/
ifneq ($(CONFIG_TARGET_ath79_nand_DEVICE_glinet_gl-e750)$(CONFIG_TARGET_ath79_nand_DEVICE_glinet_gl-x300b-nor),)
	$(CP) ./files/inittab $(1)/etc/inittab
endif
endef

$(eval $(call BuildPackage,gl-sdk4-base-files))
