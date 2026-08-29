# MTK binary runtime boundary

This package contains only MTK runtime IPKs selected for the customer-buildable
SDK. It does not contain vendor source code or a private build system. The
OpenWrt build extracts only the runtime payload from each IPK. Redistribution
requires the applicable vendor and open-source license approvals.

- `mtk-wifi-runtime`: MT7993 Wi-Fi kernel modules, firmware, control programs,
  configuration scripts and the optional diagnostic module.
- `mtk-phy-firmware`: firmware loaded by the source-built MT7987 internal
  2.5G PHY driver.
- `mtk-network-runtime`: binary mapfilter, forwarding and QoS kernel modules.
- `manifests/`: SHA-256 inventories for the original IPKs and extracted files.

All kernel modules are restricted to Linux `5.4.281`, the MT7987 target and the
validated kernel ABI. Rebuild the entire binary module set and repeat ABI checks
after changing the kernel version, kernel configuration or relevant exports.
