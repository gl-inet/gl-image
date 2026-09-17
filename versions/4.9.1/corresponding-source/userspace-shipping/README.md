# BE14000 Userspace Source Patches

## Default Package Patches

These patches are applied by normal OpenWrt package processing, with no
additional manual step:

| Package | Patch and behavior |
| --- | --- |
| `fstools` | `0100-automount.patch`, `0102-mount-options.patch`: automount defaults and FAT/NTFS mount options. |
| `iw` | `999-mtk-001-fix-mcs-set.patch`: EHT MCS capability parsing. |
| `iwinfo` | `0001-add-iwinfo-support-for-be10000.patch`, `0002-add-support-EHT320M.patch`: Wi-Fi 7, Lua htmode and EHT320 support. |
| `netifd` | `0003-set-wireless-iface-isolate-to-0.patch`: bridge isolation handling for MTK `ra*` interfaces. |
| `swconfig` | `001-be14000-shipping-link-speed.patch`: 2.5G/10G speed names. |
| `wireless-tools` | `007-iwconfig_cannot_show_32_bytes_ssid.patch`: full-length SSID handling. |

## Additional Source Patches

The following files are supplied here for source reconstruction and are not
part of the default runtime patch sequence:

- `0103-mtk-ntfs-mount-by-ufsd.patch` forces NTFS mounts to use UFSD. Apply
  with `patch -p1` inside the fstools source directory after its package
  patches. UFSD is not included in the default public configuration, which
  uses ntfs-3g; enabling this patch requires a compatible UFSD driver.
- `100-swconfig-switch-script.patch` restores
  `package/network/config/swconfig/files/switch.sh`. Apply with `patch -p1`
  at the OpenWrt root. The script uses `eth_ports_config_map` to avoid
  resetting WAN ports. That GL-specific configuration is absent from the
  default public image.
- `101-wireless-tools-install.patch` changes `iwpriv` from a link to
  `iwconfig` into a link to `mwctl`. Apply with `patch -p1` at the OpenWrt
  root when using that MTK runtime integration.

The netifd startup script is supplied in
`../openwrt-shipping/100-openwrt-shipping-scripts.patch`. Its changes are
separate from the netifd C-source patch listed above.

`changes.json` identifies patch files and their hashes. `verification.json`
records hashes for public-default and reconstructed shipping sources. Keep
the source archive revisions and patch order fixed when using these manifests.
