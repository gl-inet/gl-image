# BE14000 OpenWrt Script Sources

`100-openwrt-shipping-scripts.patch` contains 18 script sources for this
release. Seventeen modify original files across `base-files`, `netifd`,
`ppp`, `uqmi`, `fstools`, `firewall` and `comgt`. One adds the modified
smstools3 script at `package/utils/smstools3/shipping/scripts/sendsms`.

`manifest.json` maps each source path to its installed path and records
input/output SHA256 values. The package Makefiles declare GPL-2.0 for
base-files, netifd, uqmi, fstools and smstools3; GPL-2.0+ for comgt;
BSD-4-Clause for ppp; and ISC for firewall. Preserve the individual file
notices and the applicable license texts.

## Apply the Source Patch

From the root of a separate copy of the versioned OpenWrt source tree:

```sh
patch -p1 --batch --fuzz=0 --dry-run -i corresponding-source/openwrt-shipping/100-openwrt-shipping-scripts.patch
patch -p1 --batch --fuzz=0 -i corresponding-source/openwrt-shipping/100-openwrt-shipping-scripts.patch
```

The six-file `../base-files-shipping/` patch is a subset; do not apply it on
top of this patch. `/etc/profile` contains a `%PATH%` placeholder replaced
during package installation.

The patch restores script modifications supplied by the GL base-files and
cellular packages, including sysupgrade, reset, PPP, QMI, 3G and NCM scripts.
The installed `10-mount` and `20-firewall` files correspond to
`fstools/files/mount.hotplug` and `firewall/files/firewall.hotplug` respectively.

For smstools3, the base source is `smstools3-3.1.21.tar.gz` plus
`package/utils/smstools3/patches/`. To use its supplied shipping script,
replace the prepared source's `scripts/sendsms` with the added file before
building that package.

## Default Image

This full source patch is not applied automatically to the public image.
Some scripts call GL private services or helpers absent from that image.
The build applies the independent base-files changes through
`scripts/apply-be14000-shipping-scripts.sh` instead. Installing the full
scripts requires supplying their runtime dependencies.
