# GL-BE14000 Firmware Guide

This guide covers the GL-BE14000 source tree under `versions/4.9.1/`.

## Release Reference

```text
Version:   4.9.1 release2
Image:     be14000-4.9.1_release2-1053-0722-1784729880.bin
SHA256:    77d314e7bbeaa92b0fc5272be18e630550b062e88b7f5a91a76af2fd1965e460
Download:  https://fw.gl-inet.com/firmware/be14000/release/be14000-4.9.1_release2-1053-0722-1784729880.bin
```

The public build includes OpenWrt and LuCI. Independent GL.iNet proprietary
applications and GL.iNet Web UI are excluded. MediaTek Wi-Fi, PHY and network
runtime IPKs are included for Linux 5.4.281. The public build therefore has a
different feature and package selection from the commercial firmware above.

## Build Environment

Use a 64-bit, case-sensitive GNU/Linux filesystem and build as a regular user.
The reference environment is Ubuntu 22.04 with GCC/G++ 9, GNU Make 4.3 and
Python 3.10. At least 8 GiB RAM and 30 GiB free disk space are recommended.

### Docker

From the repository root:

```sh
docker build \
  --build-arg BUILD_UID="$(id -u)" \
  --build-arg BUILD_GID="$(id -g)" \
  -t gl-be14000-builder .

docker run --rm -it \
  -v "$PWD:/workspace" \
  gl-be14000-builder
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

## Build the Firmware

From the repository root, either on the host or inside the container:

```sh
cd versions/4.9.1
./scripts/feeds update -a
./scripts/feeds install -a
make defconfig
make prereq
make -j"$(nproc)"
```

The included `.config` is the default. Normal builds do not require copying
a template or running `menuconfig`. Feeds are pinned in `feeds.conf.default`;
install them before running `make defconfig`.

The resulting sysupgrade image is:

```text
bin/targets/mediatek/mt7988/openwrt-mediatek-mt7988-gl-be14000-squashfs-sysupgrade.bin
```

The target directory also contains the package manifest and build metadata.
To record the image checksum:

```sh
sha256sum bin/targets/mediatek/mt7988/*-sysupgrade.bin
```

The bundled kernel modules require the Linux 5.4.281 ABI. Changing the kernel
or ABI-relevant options requires matching runtime modules.

### Configuration Files

| File | Purpose |
| --- | --- |
| `.config` | Default selection for the public firmware build. |
| `configs/gl-be14000-baseline.config` | Commercial-build reference. May name proprietary packages absent from this tree; it is not a public-build template. |
| `configs/gl-be14000-open-source.config` | Public firmware template, excluding independent proprietary applications and GL.iNet Web UI. |
| `configs/gl-be14000-ccs-validation.config` | Template for building available GPL/copyleft components and retained kernel modules. |

Use the included `.config` with the build commands above. No configuration
copy step is required.

## Install the Firmware

From GL.iNet Web UI, open `http://192.168.8.1` and select
**System > Upgrade > Local Upgrade**. Upload the generated sysupgrade image
and disable keeping settings.

Alternatively, from the version directory on the build host:

```sh
scp -O \
  bin/targets/mediatek/mt7988/openwrt-mediatek-mt7988-gl-be14000-squashfs-sysupgrade.bin \
  root@192.168.8.1:/tmp/firmware.bin

ssh root@192.168.8.1
sysupgrade -n /tmp/firmware.bin
```

`sysupgrade -n` resets the previous settings. Do not interrupt power during
the upgrade. After boot, the public firmware's default LAN address is
`192.168.1.1`. LuCI is included; GL.iNet Web UI is not.

## Source and Runtime Layout

| Path | Contents and use |
| --- | --- |
| `feeds.conf.default` | Fixed OpenWrt, MediaTek, GL public, VPN and MPTCP feed revisions. |
| `configs/feed-package-sources.json` | Source selection when multiple feeds supply the same package. |
| `target/linux/mediatek/` | Kernel patches, DTS, board support and image definitions. |
| `package/kernel/glinet/` | Included GL kernel-module sources. |
| `package/kernel/mac80211/patches/` | Backports/cfg80211 patches. |
| `scripts/feed-patches/mtk_openwrt_feed/` | EIP/PCE patches applied by `feeds update` before package indexing. |
| `scripts/rootfs-patches/shipping-independent/` | Base-files patch applied automatically during rootfs assembly. |
| `package/firmware/mtk-be14000-binary-runtime/` | Wi-Fi, PHY and network runtime IPKs and license notices. |
| `corresponding-source/` | Additional source patches, manifests and component instructions. |

### Wpad

The default image uses the wpad binary in the MTK Wi-Fi runtime.
The `be14000-mtk-wifi-runtime` package supplies hostapd/wpad; the default
configuration does not build the hostapd source package.

The Wi-Fi runtime also installs the bundled `mwctl` executable and Wi-Fi
profile files from the supplied IPKs. These are not built as separate source
packages in the public configuration.

### Userspace Patches

The `fstools`, `iw`, `iwinfo`, `netifd`, `swconfig` and `wireless-tools`
patches in their package directories are applied by the normal build process.
The public image uses ntfs-3g. The UFSD-specific NTFS mount patch, the
GL-dependent switch port-reset script and the `iwpriv -> mwctl` installation
rule are supplied separately rather than applied to the default image.

See [Userspace Shipping Sources](corresponding-source/userspace-shipping/README.md)
for patch application paths and order.

### Additional Script Sources

[OpenWrt Shipping Scripts](corresponding-source/openwrt-shipping/README.md)
describes the consolidated script patch and its manifest. Some scripts depend
on GL services excluded from the public image. Apply these source-reconstruction
patches to a separate copy of the source tree; they are not default runtime
overlays.

The six-file patch in `corresponding-source/base-files-shipping/` is a subset
of the consolidated patch. Do not apply both to the same files.
