include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
PKG_NAME:=kmod-amneziawg

include $(INCLUDE_DIR)/kernel-defaults.mk
# Detect if WireGuard exists in kernel tree
$(info LINUX_DIR: $(LINUX_DIR))
WG_IN_TREE_DIR := $(strip $(wildcard $(LINUX_DIR)/drivers/net/wireguard))
$(info WG_IN_TREE_DIR: $(WG_IN_TREE_DIR))
$(info Checking path: $(WG_IN_TREE_DIR))

# This sets PKG_SOURCE, PKG_VERSION, PKG_BUILD_DIR etc.
include $(CURDIR)/build-oot.mk

# Conditional dependency prefix for MediaTek mt798x/filogic targets.
# Cannot be split across lines: Make's backslash-newline inserts a space,
# which would break OpenWrt's '||' dependency expression parser.
MT798X_DEP := +TARGET_mediatek_mt7981||TARGET_mediatek_mt7986||TARGET_mediatek_mt7987||TARGET_mediatek_mt7988||TARGET_mediatek_filogic

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
		$(MT798X_DEP):kmod-crypto-lib-chacha20poly1305 \
		$(MT798X_DEP):kmod-crypto-lib-curve25519
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(KERNEL_MAKE_FLAGS) \
		$(AWG_EXTRA_MAKE_FLAGS) \
		M="$(AWG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(BUILDFLAGS) $(INTREE_EXTRA_CFLAGS) $(WG_EXTRA_CFLAGS) $(AWG_EXTRA_CFLAGS)" \
		modules
endef

$(eval $(call KernelPackage,amneziawg))

