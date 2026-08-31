include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include ./version.mk

PKG_NAME:=gl-sdk4-hw-check
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/gl-sdk4-hw-check
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=GL.iNet hardware check
  DEPENDS:=+kmod-gl-sdk4-hw-info
  KCONFIG:=CONFIG_SECURITY_GL_HW_CHECK=y
endef

Build/Prepare=
Build/Compile=

$(eval $(call KernelPackage,gl-sdk4-hw-check))
