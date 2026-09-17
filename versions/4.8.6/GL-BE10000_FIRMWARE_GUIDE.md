# GL-BE10000 Firmware Guide

This guide applies to the source release for GL-BE10000 firmware 4.8.6.

## Release mapping

```text
Version:   4.8.6 release1
Image:     be10000-4.8.6_release1-1001-0723-1784782120.bin
SHA256:    ea53fc7dc71c80df659792005dffbc60bc095fa062486ff98c031afb66fa3892
Download:  https://fw.gl-inet.com/firmware/be10000/release/be10000-4.8.6_release1-1001-0723-1784782120.bin
```

The public source tree does not include independent GL.iNet proprietary
applications or the GL.iNet web interface. MediaTek Wi-Fi and network
components that are not distributed as source are packaged as version-pinned,
ABI-locked runtime IPKs. The Airoha AN8811HB PHY is built from its public GPL
source and is not included as a binary PHY IPK.

This is a build candidate with outstanding source-license reviews and device
tests, not a completed correspondence or hardware-validation claim. See the
review status below and `RELEASE-MANIFEST.json` for the recorded build results.

The mapfilter, mtfwd, mtqos, mt_wifi, mt_wifi_osal and mtk_hwifi module payloads
are imported from the exact shipping image above. Their hashes are recorded in
`package/firmware/mtk-binary-runtime/manifests/shipping-modules-sha256.txt`.
The runtime package preserves these six files, plus connac_if, mt7990,
mt7990_dbg, mt7991, mt_spectrum, mtk_pci, mtk_warp and mtk_wed, without
stripping them again. Other kernel modules retain the existing stripping
process. Prebuilt user-space payloads are not stripped a second time.

## Build Configuration Files

The version directory includes a ready-to-use `.config` with the CCS-validation
settings. The default build does not require copying a configuration file.
Independent GL.iNet proprietary applications and Web UI remain excluded.
The named configurations below are retained for audit, recovery and compatibility.

### `configs/gl-be10000-baseline.config`

Configuration captured from the matching internal BE10000 4.8.6 shipping build.
It may reference GL.iNet proprietary modules, applications and internal feeds
that are not included in this public source release. Use it for internal audit
and comparison only.

### `configs/gl-be10000-open-source.config`

Compatibility template for the public firmware image. Its configuration options
are currently identical to the CCS-validation template. It is not a separate,
smaller build profile in this release.

### `configs/gl-be10000-ccs-validation.config`

Release template for the default build and CCS verification. The GL kernel modules listed below are
selected with `=y` in both this configuration and the public configuration,
so they are built and installed into the firmware image. Proprietary
applications and Web UI components are excluded.

## Build environment

Use a 64-bit, case-sensitive GNU/Linux filesystem and build as a regular user.
The reference environment is Ubuntu 22.04 with GCC/G++ 9, GNU Make 4.3 and
Python 3.10. At least 8 GiB RAM and 30 GiB free disk space are recommended.

### Docker

From the repository root:

```sh
docker build \
  --build-arg BUILD_UID="$(id -u)" \
  --build-arg BUILD_GID="$(id -g)" \
  -t gl-be10000-builder .

docker run --rm -it \
  -v "$PWD:/workspace" \
  gl-be10000-builder
```

### Native Ubuntu 22.04

```sh
sudo apt update
sudo apt install -y \
  bash ca-certificates build-essential clang flex bison g++ gawk \
  gcc-multilib g++-multilib gcc-9 g++-9 \
  gcc-9-multilib g++-9-multilib gettext git ccache \
  libncurses-dev libssl-dev libelf-dev \
  python3 python3-distutils python3-setuptools \
  rsync subversion swig unzip zlib1g-dev file wget perl quilt \
  xz-utils zstd time xsltproc
```

## Build the firmware

Start from the repository root, either on the native host or inside the Docker
container. The included `.config` already selects the default CCS-validation build.

Update and install feeds successfully before running `make defconfig`.
The build system must first discover the feed
packages; otherwise, selections such as `kmod-amneziawg` and `amneziawg-tools`
can be removed from `.config`. If feeds were unavailable during an earlier
`make defconfig`, install them, restore the release defaults as described below
and rerun `make defconfig`.

For the default public firmware and CCS validation, run in this order:

```sh
cd versions/4.8.6
./scripts/feeds update -a
./scripts/feeds install -a
make defconfig
make prereq
make -j"$(nproc)"
```

The `configs/gl-be10000-baseline.config` file is for internal shipping-build
audit and comparison only. It references proprietary packages that are not
included in this public source release, so it is not a standalone build
configuration and must not be used for the default build.

### Restore release defaults (optional)

This is only needed after deleting or customizing `.config`, or after a
`make defconfig` run with unavailable feeds. `make distclean` also removes
`.config` and the installed feeds. Complete feed setup first, then run from
the version directory:

```sh
cp configs/gl-be10000-ccs-validation.config .config
make defconfig
```

This recovery `cp` command replaces local configuration changes. Preserve a custom
`.config` before restoring the release defaults. For a logged build, use Bash
with `set -o pipefail` before piping `make` output to `tee`, so a failed build
does not appear successful.

When building from a Git worktree, its `.git` file may refer to a Git directory
outside the mounted path. A standalone checkout follows the Docker example
above directly. For a worktree, mount both it and its owning Git directory at
their original absolute paths, and use that absolute working directory inside
the container; otherwise Git-based version detection can fail.

The sysupgrade image is generated at:

```text
bin/targets/mediatek/mt7987/openwrt-mediatek-mt7987-glinet_gl-be10000-squashfs-sysupgrade.bin
```

Do not change the Linux 5.4.281 kernel ABI unless matching MediaTek runtime
modules are supplied and validated for the new ABI.

### Package sources and release patches

`configs/feed-package-sources.json` selects the source provider for packages
that appear in more than one feed or in the core tree. The included
`scripts/feeds` applies these selections during the normal commands above;
individual package override commands are not required.

`feed-patches/<feed>/` contains release-specific fixes applied before feed
indexing. A patch that neither applies nor is already present stops the update.
The VPN feed patch records the verified SHA256 of the upstream ovpn-dco archive.
Feed revisions stay fixed; do not replace them with branch heads.

This version directory restores the shipping dnsmasq 2.85-16 source and patches.
The earlier public tree's dnsmasq 2.92 is retained in Git history. This source
release reproduces historical package inputs; it is not a statement that those
versions contain every later security fix.

The `hostapd-common`, `hostapd-utils`, `wpa-cli` and `wpad-openssl` packages
are built from the release hostapd source. The matching bundled hostapd/wpad
IPKs are not installed by the Wi-Fi runtime, avoiding duplicate files.

Both build configurations enable Samba, OpenVPN, nginx and the release's
other retained public applications. `mtk-tools-runtime` supplies the retained
MediaTek mesh and diagnostic payloads. Its source-image mapping, file hashes
and discovered license notices are recorded under
`package/firmware/mtk-binary-runtime`. The GPL scripts and original recipe
for `mtk-base-files` are also retained there in editable form. Binary packaging
does not replace source or notice obligations for covered components.

The release `version.date` and OpenSSL optimization setting preserve the
shipping build inputs. Restoring historical inputs does not guarantee
byte-identical output across different build paths or toolchain environments.

### Shipping kernel configuration

`configs/gl-be10000-shipping-kernel.config` contains the kernel configuration
extracted from the exact shipping firmware. The target kernel configuration is
based on that file, with explicit exceptions for the existing PHY binary runtime
and build-managed initramfs settings.

This matters even when the same source revisions and rootfs packages are used:
kernel options can change structure layouts used by modules. Additional kernel
modules may be compiled to preserve the release configuration without being
installed into the firmware. Package selection still controls rootfs contents.

The public configuration sets `CONFIG_PACKAGE_kmod-mac80211=m` and
`CONFIG_PACKAGE_MAC80211_DEBUGFS=y`. This preserves the shipping cfg80211
debugfs implementation without installing mac80211 into the rootfs.

The GPL exFAT library and utilities are rebuilt from
`package/libs/gl-sdk4-nas-exfat`, separately from proprietary NAS applications.

### GL kernel modules

The repeater, mpflow statistics, tertf, black/white-list, DNS marking, kmwan
and parental-control module sources are available under `package/kernel/glinet`.
These seven modules, together with the fan and hardware information modules
from the pinned GL common feed, are selected with `=y` in both build
configurations. AmneziaWG, OpenVPN DCO, wifidog-ng and full-cone NAT kernel
modules are also selected from their pinned feeds.

The GL common fan package also installs its fan-control program and support
files. Selecting a kernel package installs any companion files defined by its
package recipe. The GL.iNet proprietary application suite is not restored by
these selections.

The source tree includes the mpflow kernel symbol export patch. These
configuration selections do not establish successful compilation, module
loading or device boot; verify those after rebuilding the image.

`package/kernel/glinet/SOURCES.json` records the imported module revisions.
Original shipping boot/network/upgrade scripts that differ from the public
integration are retained in `corresponding-source/shipping-integration`.

### Shipping script integration

`corresponding-source/shipping-script-overlay` contains the full nine-file
shipping script patch and its source mapping. This full patch is not applied
by default because several scripts require omitted GL application services.

The normal build automatically applies the independent subset in
`scripts/rootfs-patches/shipping-independent` after package post-installation
and before image creation. It restores the shipping boot and firewall-default
scripts, the upgrade RAM filesystem tools, and the independent sysupgrade
changes. No extra build command or feeds update is needed for this patch.

Public reset, automount and cellular protocol implementations remain in use.
GL screen, session and cellular services are not added. Input and output file
hashes guard the runtime patch: if an affected package or a custom `files/`
overlay changes these scripts, review and update the patch and hashes instead
of bypassing the check. Script restoration is not a claim of device-tested
upgrade, reset, wireless or cellular functionality.

### NTFS mounting

The fstools source and patches retain the shipping UFSD mount behavior.
The additional public-tree ENODEV fallback has been removed to match the
release inputs. The public configuration includes ntfs-3g, but this alone
does not establish that the shipping automount path works without UFSD.
Automatic mounting and disk read/write operation require device testing.

### PHY source status

The AN8811HB source is included under
`target/linux/mediatek/files-5.4/drivers/net/phy/air_an8811hb.c` and declares
GPL-2.0+. The integration patch adds its Kconfig and Makefile entries. The
shipping-style configuration builds it into the kernel with
`CONFIG_AIR_AN8811HB_PHY=y`; no `air_an8811hb.ko` is installed in rootfs.
The PHY firmware data and the driver source remain separate components.

## Install the firmware

When the router is still running GL.iNet firmware, open
`http://192.168.8.1`, select **System > Upgrade > Local Upgrade**, upload the
generated sysupgrade image and do not retain settings.

Alternatively, upload and install it over SSH:

```sh
scp -O \
  bin/targets/mediatek/mt7987/openwrt-mediatek-mt7987-glinet_gl-be10000-squashfs-sysupgrade.bin \
  root@192.168.8.1:/tmp/firmware.bin

ssh root@192.168.8.1
sysupgrade -n /tmp/firmware.bin
```

Do not interrupt power during the upgrade. After the open-source firmware
boots, its default LAN address is `192.168.1.1`. LuCI is included in the public
configuration. The proprietary GL.iNet web interface is not included.

Open `http://192.168.1.1/` from a computer connected to a LAN port. The BE10000
first-boot defaults enable uhttpd and disable nginx autostart so their default
port 80 listeners do not conflict. nginx remains installed; configure a distinct
listener before enabling it manually. These defaults are intended for an upgrade
without retained settings, as shown above.

## Source layout

- `configs/gl-be10000-baseline.config`: internal shipping-build configuration.
- `.config`: ready-to-use default CCS-validation configuration.
- `configs/gl-be10000-open-source.config`: compatibility template with the same options as the default profile.
- `configs/gl-be10000-ccs-validation.config`: template for restoring the default CCS-validation configuration.
- `configs/gl-be10000-shipping-kernel.config`: kernel configuration extracted from the shipping image.
- `feeds.conf.default`: pinned feed revisions.
- `package/firmware/mtk-binary-runtime`: MediaTek runtime IPKs and manifests.
- `target/linux/mediatek`: kernel, DTS, board support and image definitions.
- `package/kernel/glinet`: retained GL.iNet kernel module sources.
- `configs/feed-package-sources.json`: release package provider mapping.
- `feed-patches`: fixes applied to pinned feeds during update.
- `corresponding-source/shipping-integration`: original release integration scripts.
- `configs/gl-be10000-shipping-kernel.config`: exact kernel configuration extracted from the release image.
- `package/libs/gl-sdk4-nas-exfat`: GPL exFAT library and utilities.

## Crash log extraction source

`package/utils/gl-oopslog` contains the GPLv2 crash-log extraction tool from
the shipping logread package. `package/libs/gl-sdk4-uci` contains its libguci
C library dependency. Both are built by the default configuration; proprietary
log RPC, Lua and service scripts are not included. The original C files are
unmodified. libguci matches the shipping binary byte for byte; oopslog differs
only in its embedded compilation date/time strings. Device execution has not
been tested.

No new license has been assigned to libguci. Copyright-holder authorization
for its redistribution remains outstanding before external source delivery.

## Verification and review status

The candidate was built and extracted for comparison with the shipping image.
It has not been flashed or tested for boot, Wi-Fi/PHY operation, module loading
or disk read/write behavior. The installation instructions describe the intended
procedure; they are not evidence that this candidate passed a device test.

The latest checked image contained all 234 shipping kernel modules with
matching file hashes. The current source tree also includes subsequent
user-space changes that have not yet been rebuilt and compared. Configuration
validation alone does not confirm the contents of the next firmware image.

Other outstanding reviews are the libguci license authorization, the LGPL
WireGuard code embedded in the omitted s2s plugin, and the GPL declaration in
the omitted arp-scan package. Omitting an application from this candidate does
not establish that its shipped version has no source obligations.

For each rebuilt image, retain its package manifest, actual `.config`,
`config.buildinfo`, `feeds.buildinfo`, `version.buildinfo` and SHA256. Regenerate
the comparison and SBOM when the image changes. Do not use a previous image's
report as proof for a new build. A clean build using public inputs remains a
separate final verification step.
