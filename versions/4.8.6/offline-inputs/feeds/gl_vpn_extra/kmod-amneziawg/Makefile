include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
PKG_NAME:=kmod-amneziawg

# Detect if WireGuard exists in kernel tree
$(info LINUX_DIR: $(LINUX_DIR))
WG_IN_TREE_DIR := $(strip $(wildcard $(LINUX_DIR)/drivers/net/wireguard))
$(info WG_IN_TREE_DIR: $(WG_IN_TREE_DIR))
$(info Checking path: $(WG_IN_TREE_DIR))

# This sets PKG_SOURCE, PKG_VERSION, PKG_BUILD_DIR etc.
ifeq ($(WG_IN_TREE_DIR),)
  # Out-of-tree: include download and build configuration
  include $(CURDIR)/build-oot.mk
else
  # In-tree: no download needed
  include $(CURDIR)/build-intree.mk
endif

define KernelPackage/amneziawg
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  TITLE:=AmneziaWG Kernel Module
  URL:=https://amnezia.org/
  MAINTAINER:=Amnezia Admin <admin@amnezia.org>
  FILES:=$(AWG_BUILD_DIR)/amneziawg.$(LINUX_KMOD_SUFFIX)
  DEPENDS:= \
		+kmod-udptunnel4 \
		+kmod-udptunnel6 \
		+kmod-crypto-lib-chacha20poly1305 \
		+kmod-crypto-lib-curve25519
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(KERNEL_MAKE_FLAGS) \
		M="$(AWG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(BUILDFLAGS) $(WG_EXTRA_CFLAGS)" \
		modules
endef

$(eval $(call KernelPackage,amneziawg))

