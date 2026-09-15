include $(TOPDIR)/rules.mk

PKG_NAME:=gl-oopslog
PKG_VERSION:=90760d874d8b
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only
PKG_LICENSE_FILES:=COPYING

include $(INCLUDE_DIR)/package.mk

define Package/gl-oopslog
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=MTD crash log extraction utility
  DEPENDS:=+gl-sdk4-uci
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/. $(PKG_BUILD_DIR)/
	$(CP) ./COPYING $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) $(TARGET_CONFIGURE_OPTS) CFLAGS="$(TARGET_CFLAGS) $(TARGET_CPPFLAGS)" -j1
endef

define Package/gl-oopslog/install
	$(INSTALL_DIR) $(1)/usr/bin $(1)/usr/share/licenses/gl-oopslog
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/oopslog $(1)/usr/bin/
	$(INSTALL_DATA) ./COPYING $(1)/usr/share/licenses/gl-oopslog/
endef

$(eval $(call BuildPackage,gl-oopslog))
