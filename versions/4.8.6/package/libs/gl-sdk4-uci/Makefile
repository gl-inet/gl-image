include $(TOPDIR)/rules.mk

PKG_NAME:=gl-sdk4-uci
PKG_VERSION:=git-2025.042.27505-7e5f319
PKG_RELEASE:=1
CMAKE_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/gl-sdk4-uci
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=GL.iNet UCI C wrapper library
  DEPENDS:=+libuci
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/. $(PKG_BUILD_DIR)/
endef

define Package/gl-sdk4-uci/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/libguci.so $(1)/usr/lib/
endef

$(eval $(call BuildPackage,gl-sdk4-uci))
