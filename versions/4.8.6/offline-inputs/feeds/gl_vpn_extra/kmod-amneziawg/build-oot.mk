# Out-of-Tree Build Configuration
# This file is included when WireGuard is not found in kernel tree

PKG_VERSION:=1.0.20241112
PKG_RELEASE:=1

PKG_SOURCE:=v$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/amnezia-vpn/amneziawg-linux-kernel-module/archive/refs/tags/
PKG_HASH:=3c70dce9aec00c217021037b475fbcb792ab701d72bfd696c866f8740b96218d

PKG_SOURCE_SUBDIR:=amneziawg-linux-kernel-module-$(PKG_VERSION)
PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/$(PKG_SOURCE_SUBDIR)
AWG_BUILD_DIR:=$(PKG_BUILD_DIR)/src

# Extra compiler flags for out-of-tree build
WG_EXTRA_CFLAGS:=-Wno-error=stringop-overread

# Include package.mk here for out-of-tree build
include $(INCLUDE_DIR)/package.mk

# Build/Prepare: unpack source and apply compat patches
define Build/Prepare
	$(call Build/Prepare/Default)
	@echo "[amneziawg-OOT] Applying compat patches"
	mkdir -p $(PKG_BUILD_DIR)/patches
	$(CP) -rvf $(CURDIR)/compat-patches/*.patch $(PKG_BUILD_DIR)/patches/ 2>/dev/null || true
	cd "$(PKG_BUILD_DIR)" && \
	ls patches/*.patch | xargs -n1 basename | sort -V > patches/series && \
	QUILT_PATCHES=patches quilt push -a || true;
endef
