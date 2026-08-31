# In-Tree Build Configuration
# This file is included when WireGuard is found in kernel tree

PKG_VERSION:=in-tree
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/amneziawg-intree
# AWG_BUILD_DIR, use in main Makefile
AWG_BUILD_DIR:=$(PKG_BUILD_DIR)

# No extra compiler flags for in-tree build
WG_EXTRA_CFLAGS:=

# Include package.mk here for in-tree build
include $(INCLUDE_DIR)/package.mk

INTREE_EXTRA_CFLAGS:= \
		-I$(PKG_BUILD_DIR)/compat/kstrtox/include \
		-I$(PKG_BUILD_DIR)/compat/gso/include \
		-I$(PKG_BUILD_DIR)/compat/sprintf/include \
		-I$(PKG_BUILD_DIR) \
		-include $(PKG_BUILD_DIR)/compat/compat.h \
		-DGL_NOT_USE_COMPAT_CRYPTO \
		-DGL_NOT_COMPAT_OLD_IP_TUNNEL_IF

define Build/Prepare
	$(call Build/Prepare/Default)
	mkdir -p $(PKG_BUILD_DIR)
	@echo "[amneziawg-InTree] WireGuard is in-tree, copying sources into $(PKG_BUILD_DIR)"

	# Copy WireGuard sources from kernel tree
	$(CP) -v $(LINUX_DIR)/drivers/net/wireguard/*.c $(PKG_BUILD_DIR)/
	$(CP) -v $(LINUX_DIR)/drivers/net/wireguard/*.h $(PKG_BUILD_DIR)/
	mkdir -p $(PKG_BUILD_DIR)/uapi
	$(CP) -rv $(WG_IN_TREE_DIR)/selftest $(PKG_BUILD_DIR)/ 2>/dev/null || true
	$(CP) -v $(LINUX_DIR)/include/uapi/linux/wireguard.h $(PKG_BUILD_DIR)/uapi/wireguard.h 2>/dev/null || true

	# Apply common patches if they exist
	@echo "[amneziawg-InTree] Applying patches for in-tree build"
	- if [ -d "$(CURDIR)/common_patches" ]; then \
		for patch in $(CURDIR)/common_patches/*.patch; do \
			[ -f "$$$$patch" ] && echo "Applying $$$$patch" && patch -d $(PKG_BUILD_DIR) -F3 -t -p0 < "$$$$patch" || true; \
		done; \
	fi

	# Apply board-specific patches (required)
	if [ -d "$(CURDIR)/board_spec/$(BOARD)/$(SUBTARGET)/patches" ]; then \
		for patch in $(CURDIR)/board_spec/$(BOARD)/$(SUBTARGET)/patches/*.patch; do \
			[ -f "$$$$patch" ] && echo "Applying $$$$patch" && patch -d $(PKG_BUILD_DIR) -F3 -t -p1 < "$$$$patch" || true; \
		done; \
	else \
		echo "[amneziawg-InTree] ERROR: No board-specific patches found for $(BOARD)/$(SUBTARGET) in-tree build!"; \
		exit 1; \
	fi

	# Copy src/ directory if exists (for custom Makefile, etc.)
	[ ! -d $(CURDIR)/src/ ] || $(CP) -fpR $(CURDIR)/src/* $(PKG_BUILD_DIR)/
endef
