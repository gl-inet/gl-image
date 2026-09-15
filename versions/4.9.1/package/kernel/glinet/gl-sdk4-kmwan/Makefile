
include $(TOPDIR)/rules.mk
include ./version.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-kmwan
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

PKG_BUILD_DEPENDS:=lua/host

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  SECTION:=base
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=glinet kmwan
  FILES:=$(PKG_BUILD_DIR)/kmwan.ko 
  DEPENDS:=+kmod-nf-conntrack +kmod-nf-nat
  AUTOLOAD:=$(call AutoLoad,40,kmwan,1)
endef

EXTRA_CFLAGS += -Wno-frame-larger-than

KERNEL_MAKE_FLAGS?= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)"

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	M="$(PKG_BUILD_DIR)"

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		modules
endef

define KernelPackage/$(PKG_NAME)/conffiles
/etc/config/kmwan
endef

 
define KernelPackage/$(PKG_NAME)/install
	echo "$(EXTRA_CFLAGS)"
	$(INSTALL_DIR) $(1)/usr/bin  $(1)/etc/init.d $(1)/lib/functions $(1)/etc/hotplug.d/iface $(1)/etc/config  $(1)/etc/hotplug.d/kmwan  $(1)/etc/udhcpc.user.d

	$(INSTALL_BIN) ./files/gen_weight_route $(1)/usr/bin/gen_weight_route
	$(INSTALL_BIN) ./files/kmwan.init $(1)/etc/init.d/kmwan
	$(INSTALL_BIN) ./files/kmwan.sh $(1)/lib/functions
	$(INSTALL_BIN) ./files/01-kmwan $(1)/etc/hotplug.d/iface
	$(INSTALL_BIN) ./files/kmwan_sync_route.sh $(1)/etc/udhcpc.user.d
	$(INSTALL_DATA) ./files/etc/hotplug.d/kmwan/*  $(1)/etc/hotplug.d/kmwan
	$(CP) ./files/config $(1)/etc/config/kmwan
	$(INSTALL_DIR) $(1)/usr/lib/oui-httpd/rpc
	$(INSTALL_DIR) $(1)/usr/lib/lua/gl
	$(CP) ./files/libkmwan.lua $(1)/usr/lib/lua/gl/kmwan.lua
	$(STAGING_DIR_HOSTPKG)/bin/luac -o $(1)/usr/lib/oui-httpd/rpc/kmwan ./files/kmwan.lua
endef

$(eval $(call KernelPackage,$(PKG_NAME)))

