#
# Copyright (C) 2007-2010 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=iperf
PKG_VERSION:=3.10
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/pabeni/iperf.git
PKG_MIRROR_HASH:=c6cad20d88d38a85b257a83e5fe946467c2b102b19aaf4b44650306958c5c777
PKG_SOURCE_VERSION := 26b066b9d4e92442d55950689dbd9fd101b429a7
PKG_HASH:=skip

PKG_MAINTAINER:=Felix Fietkau <nbd@nbd.name>
PKG_LICENSE:=BSD-3-Clause

PKG_BUILD_PARALLEL:=1
PKG_INSTALL:=1

PKG_FIXUP:=autoreconf

include $(INCLUDE_DIR)/package.mk

DISABLE_NLS:=

define Package/iperf3/default
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Internet Protocol bandwidth measuring tool
  URL:=https://github.com/esnet/iperf
endef

define Package/iperf3
$(call Package/iperf3/default)
  VARIANT:=nossl
  DEPENDS:=+libiperf3
endef

define Package/iperf3-ssl
$(call Package/iperf3/default)
  TITLE+= with iperf_auth support
  VARIANT:=ssl
  DEPENDS:=+libopenssl
endef

define Package/libiperf3
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=Internet Protocol bandwidth measuring library
  URL:=https://github.com/esnet/iperf
endef

TARGET_CFLAGS += -D_GNU_SOURCE

ifeq ($(BUILD_VARIANT),ssl)
	CONFIGURE_ARGS += --with-openssl="$(STAGING_DIR)/usr" --disable-shared
else
	CONFIGURE_ARGS += --without-openssl
endif

MAKE_FLAGS += noinst_PROGRAMS=

define Package/iperf3/description
 Iperf is a modern alternative for measuring TCP and UDP bandwidth
 performance, allowing the tuning of various parameters and
 characteristics.
endef

define Package/libiperf3/description
 Libiperf is a library providing an API for iperf3 functionality.
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/include
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libiperf.* $(1)/usr/lib/
	$(CP) $(PKG_INSTALL_DIR)/usr/include/* $(1)/usr/include/
endef

# autoreconf fails if the README file isn't present
define Build/Prepare
	$(call Build/Prepare/Default)
	touch $(PKG_BUILD_DIR)/README
endef

define Package/iperf3/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/iperf3 $(1)/usr/bin/
endef

define Package/iperf3-ssl/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/iperf3 $(1)/usr/bin/
endef

define Package/libiperf3/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libiperf.so.* $(1)/usr/lib
endef

$(eval $(call BuildPackage,iperf3))
$(eval $(call BuildPackage,iperf3-ssl))
$(eval $(call BuildPackage,libiperf3))
