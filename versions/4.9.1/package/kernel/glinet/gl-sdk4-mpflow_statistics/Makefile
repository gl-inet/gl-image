
include $(TOPDIR)/rules.mk
include ./version.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-mpflow_statistics
# Source snapshot: ddc056ac1470d9c403e13d495ced14a07ff9c7fc
PKG_VERSION:=git-2024.352.34343-ddc056a
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  SECTION:=base
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=glinet mpflow statistics
  FILES:=$(PKG_BUILD_DIR)/gl-mpflow.ko 
endef

KERNEL_MAKE_FLAGS?= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)"

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	M="$(PKG_BUILD_DIR)"

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

define KernelPackage/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/etc/hotplug.d/iface $(1)/usr/bin
	$(INSTALL_BIN) ./files/99-mpflow $(1)/etc/hotplug.d/iface/99-mpflow
	$(INSTALL_BIN) ./files/mpflow.init $(1)/etc/init.d/mpflow
	$(INSTALL_BIN) ./files/get_mpflow.sh $(1)/usr/bin/get_mpflow.sh
endef
$(eval $(call KernelPackage,$(PKG_NAME)))

