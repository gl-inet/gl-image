# Independent Shipping Script Changes

The normal BE10000 4.8.6 build applies this patch from `prepare_rootfs` in
`include/rootfs.mk`, after package post-install scripts and user file overlays,
before timestamp normalization and image creation. Both regular and per-device
rootfs assembly use this hook. The scripts directory is also copied by the
existing ImageBuilder/SDK packaging rules; those exports have not been built
or tested as part of this change.

| File | Applied change |
| --- | --- |
| `/etc/init.d/boot` | Restore shipping `sync` before kmodloader instead of the public `wifi check`. |
| `/etc/uci-defaults/99-firewall` | Restore the shipping WAN ping and firewall rule-name handling instead of the MTK PPTP helper script. |
| `/lib/upgrade/stage2` | Restore copying pidof and xargs into the upgrade RAM filesystem; do not restore GL screen process exemptions. |
| `/sbin/sysupgrade` | Restore shipping installed-package backup format, fw_printenv invocation and stage-two fallback; omit GL utility imports, display calls and private process handling. |

The first two files match the shipping scripts exactly. The latter two are
deliberate public-runtime adaptations, not byte-identical shipping scripts.
Private-dependent reset, automount and 3g/ncm/qmi scripts are not replaced.
No private binaries or supporting GL shell libraries are imported.

`input.sha256` identifies the four unmodified public-image files.
`output.sha256` identifies the four patched files. The helper validates all
inputs before applying anything, rejects missing or unexpected files and
escaping symlinks, and accepts a fully applied patch without applying it twice.
Custom images that omit a required file or replace it through `files/` must
update this version-specific integration explicitly. Do not remove the checks
just to conceal differences.

Tests used isolated copies of the public firmware scripts. Output hashes,
unchanged excluded scripts, repeat application, mismatch rejection, path
checks, shell syntax and Make hook expansion passed. No firmware compilation
or hardware validation was performed. In particular, boot, Wi-Fi startup,
firewall defaults and upgrade/backup behavior need a rebuilt-image test.

The complete unadapted nine-file source patch is separately retained under
`corresponding-source/shipping-script-overlay` and is not auto-applied.
