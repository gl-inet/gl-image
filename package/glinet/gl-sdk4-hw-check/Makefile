include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-hw-check
PKG_VERSION:=ad85675270fd
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define KernelPackage/gl-sdk4-hw-check
  CATEGORY:=Kernel modules
  SUBMENU:=GL.iNet modules
  TITLE:=GL.iNet hardware check
  DEPENDS:=+kmod-gl-sdk4-hw-info
  KCONFIG:=CONFIG_SECURITY_GL_HW_CHECK=y
endef

Build/Prepare=
Build/Compile=

$(eval $(call KernelPackage,gl-sdk4-hw-check))
