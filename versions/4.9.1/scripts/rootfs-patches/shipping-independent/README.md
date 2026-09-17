# BE14000 Rootfs Base-Files Patch

`prepare_rootfs` invokes `scripts/apply-be14000-shipping-scripts.sh` after
post-install scripts and file overlays, before timestamp normalization.
It applies `100-independent-shipping-base-files.patch` to the assembled rootfs.

| Path | Behavior |
| --- | --- |
| `bin/config_generate` | Default log size. |
| `etc/init.d/boot` | Guarded USB initialization and boot-time Wi-Fi handling. |
| `lib/functions.sh` | MMC partition lookup. |
| `lib/functions/system.sh` | MMC MAC extraction helper. |

`input.sha256` and `output.sha256` identify the expected file contents.
The helper accepts either unpatched inputs or already-patched outputs,
and rejects other contents, symlink destinations and paths outside the rootfs.
For an input mismatch, check the package versions and file overlays before
rebuilding.

The additional `/etc/profile` testmode and `/lib/upgrade/stage2` GL-screen
changes are supplied under `corresponding-source/base-files-shipping/` and
in the consolidated `corresponding-source/openwrt-shipping/` patch. They are
not automatically installed because the related GL applications are excluded.
