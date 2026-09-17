# BE14000 Base-Files Source Patch

`100-base-files-shipping.patch` contains the six base-files source changes
listed in `input.sha256`. Apply it with `patch -p1` in a copy of
`package/base-files/files/`, then check `output.sha256` from that directory.

This patch is a subset of
`../openwrt-shipping/100-openwrt-shipping-scripts.patch`. Use one representation
for these files; do not apply both.

The default build applies four independent changes through
`scripts/rootfs-patches/shipping-independent/`. The full source patch also
contains `/etc/profile` testmode handling and `/lib/upgrade/stage2` handling
for the GL screen application. These additional changes are not installed
automatically because their GL application features are not in the public image.
