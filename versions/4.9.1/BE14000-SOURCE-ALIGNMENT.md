# BE14000 4.9.1 Source Alignment

Updated 2026-09-18. This note describes source/configuration changes after the
post-build firmware comparison. These changes have NOT been compiled.

## Historical dnsmasq Version

This CCS reproduction snapshot selects the shipping dnsmasq 2.85-16 recipe and
its eight patches. It replaces the previous 2.92-4 recipe, including the newer
recipe's 2026 security patches. This is a historical-source reproduction, NOT a
security upgrade or a recommendation to deploy the old version. The previous
recipe and all its patches are retained in the external before-alignment backup.

## Source Provenance

| Destination | Source repository snapshot | Changes after import |
| --- | --- | --- |
| package/network/services/dropbear | gl_feed_common 487db81d10d5cf28d4a4968ebda719071081568c, dropbear/ | None; selects 2024.86-1 instead of the old core recipe |
| package/network/services/dnsmasq | shipping mt7988 f340dd7f99fe611ff401bcba4351727aceffb096, same relative path | Freeze the recorded release suffix at 16 |
| package/utils/busybox | shipping mt7988 f340dd7f99fe611ff401bcba4351727aceffb096, same relative path | Freeze release suffix at 5 |
| package/network/services/samba4 | gl_feed_common 487db81d10d5cf28d4a4968ebda719071081568c, samba4/ | Root-relative helper includes and independent Btrfs VFS option |

BusyBox's shipping patch 532-login-ignore-signal.patch originates from commit
b143d413254c3e884ca3efdbcbbffafddb59c00c. Its custom feature configuration is
copied from the shipping build configuration, not guessed from executable names.

procd, odhcpd and uboot-envtools recipes were verified identical to the shipping
snapshot before freezing their recorded AUTORELEASE suffixes at 2, 6 and 16.
This preserves known release metadata when the original Git history is not
present; it does not claim a new code fix in these otherwise identical recipes.

The local Samba recipe takes precedence over the feed recipe. The previous
package/feeds/gl_feed_common/samba4 symlink was uninstalled; the feed checkout
and its revision were not changed. Do not force-install a competing Samba or
Dropbear feed recipe over these local packages. Ordinary feed install without
`-f` preserves core packages. OpenVPN remains provided by gl_vpn_extra and the
Go toolchain by gl_feed_common, as in the previous handoff.

## Configuration Changes

Both configs/gl-be14000-open-source.config and
configs/gl-be14000-ccs-validation.config now include:

- OpenVPN DCO and the shipping OpenVPN feature options.
- iptables-mod-fullconenat and the other available packages identified by the
  shipping GPL/LGPL-declared payload gap list (62 selections, some already on).
- The full effective BusyBox custom and Dropbear option sets from shipping.
- SAMBA4_SERVER_BTRFS=y to include btrfs.so without selecting kmod-fs-btrfs.
- coreutils and coreutils-timeout, with the shipping BusyBox timeout selection.

The baseline configuration is unchanged. gl-sdk4-arp-scan remains explicitly
disabled and its source directory remains absent, at the user's request.
No proprietary application/Web UI restoration is intended by these selections.
Declared license fields were used to identify candidates, not as a completed
independent license determination.

mtk-base-files now emits the imported SDK revision f5c0ef68 rather than relying
on a .git directory inside versions/4.9.1. mtk-base-files and wifi-profile retain
their shipping package version 1, independent of the public repository revision.
The rootfs overlay declares glversion=4.9.1 and version.type=opensource. It does
not impersonate shipping release2 build 1053 or its build timestamp.

## Verification, Without Compilation

- Both templates and the current configuration expand with make defconfig.
- All 859 explicitly requested alignment values survive expansion; no duplicate
  config keys, recursive dependencies or Kconfig errors were found.
- A standalone GNU make test confirms the Btrfs VFS selection works without
  selecting the Btrfs kernel driver; no compiler is invoked by this test.
- Verified source-archive SHA256 values and applied the complete patch series in
  temporary directories: Dropbear 9, dnsmasq 8, BusyBox 17, Samba 15; all passed.
- Baseline and gl-sdk4-arp-scan exclusion verified; previous firmware unchanged.

These checks do not prove compilation success, installation ownership closure,
runtime functionality, ABI compatibility or final firmware hash equality.
Known missing proprietary payloads are not restored. The previously observed
63 differing modules and 18 additional modules must be reassessed after a new
build; no blind kernel/config or binary replacement was made to hide them.

## Operator Handoff

The current .config has already been updated and expanded. No new feed update
or config copy is required in this workspace. Compilation is left to the user.
The old firmware under bin/ is still the previously successful build.

Auditable backup, exact patch, per-file hashes, source provenance, configuration
snapshots and checks are under:
`/workspace/home/pan.li/ccs-results/be14000-4.9.1-open-source-20260917/difference-fixes-20260918/`

The final patch is relative to the task's starting state, including earlier
mwctl/wifi-profile fixes. It is not a substitute for those earlier imports.

Final artifacts:
- alignment-final.patch: canonical patch; supersedes the initial/followup patches.
- before-alignment.tar.gz: original changed files, configuration and baseline.
- changed-files-final.json: before/after SHA256 inventory, including new files.
- source-imports.json: exact source repository commits and imported file hashes.
- required-config.json and package-selection-map.json: configuration rationale.
- test-final.json and expanded-*.config: configuration/static verification.
- source-patch-checks.json and *-patch-check.log: verified archive patch tests.
- patch-verification.json: replay from backup, byte equality and reverse checks.

The initial patch generator mishandled missing final newlines in Samba cross
answers. Those files were restored exactly to the source snapshot and the final
patch was replayed from the original backup with every resulting byte verified.
Imported upstream patch files retain their original whitespace for provenance.
