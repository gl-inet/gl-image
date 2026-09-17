# GL-BE14000 Firmware Guide

This guide applies to the source release for GL-BE14000 firmware 4.9.1.

## Release mapping

```text
Version:   4.9.1 release2
Image:     be14000-4.9.1_release2-1053-0722-1784729880.bin
SHA256:    77d314e7bbeaa92b0fc5272be18e630550b062e88b7f5a91a76af2fd1965e460
Download:  https://fw.gl-inet.com/firmware/be14000/release/be14000-4.9.1_release2-1053-0722-1784729880.bin
```

The public source tree does not include independent GL.iNet proprietary
applications or the GL.iNet web interface. MediaTek Wi-Fi, PHY and network
components that are not distributed as source are packaged as version-pinned,
ABI-locked runtime IPKs.

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

## Build the firmware

Run from the repository root, either on the native host or inside the Docker
container:

```sh
cd versions/4.9.1
./scripts/feeds update -a
./scripts/feeds install -a
make defconfig
make prereq
make -j"$(nproc)"
```

The sysupgrade image is generated at:

```text
bin/targets/mediatek/mt7988/openwrt-mediatek-mt7988-gl-be14000-squashfs-sysupgrade.bin
```

Do not change the Linux 5.4.281 kernel ABI unless matching MediaTek runtime
modules are supplied and validated for the new ABI.

## Install the firmware

When the router is still running GL.iNet firmware, open
`http://192.168.8.1`, select **System > Upgrade > Local Upgrade**, upload the
generated sysupgrade image and do not retain settings.

Alternatively, upload and install it over SSH:

```sh
scp -O \
  bin/targets/mediatek/mt7988/openwrt-mediatek-mt7988-gl-be14000-squashfs-sysupgrade.bin \
  root@192.168.8.1:/tmp/firmware.bin

ssh root@192.168.8.1
sysupgrade -n /tmp/firmware.bin
```

Do not interrupt power during the upgrade. After the open-source firmware
boots, its default LAN address is `192.168.1.1`. LuCI is included in the public
configuration. The proprietary GL.iNet web interface is not included.

## Default and Recovery Configuration

The independent version-directory `.config` is the default CCS/public build
configuration. Normal builds do not copy a template or require menuconfig.
Install the pinned feeds before `make defconfig`. Existing, correctly installed
feeds need not be updated again for every local rebuild.

- `configs/gl-be14000-ccs-validation.config` restores the default selection.
- `configs/gl-be14000-open-source.config` is the compatibility template; its
  normalized selections are the same as the CCS template, not a second build.
- `configs/gl-be14000-baseline.config` is unchanged shipping audit evidence,
  not a customer-buildable default and not a kernel configuration file.

Only to recover a missing or damaged configuration, after installing feeds:

```sh
cp -p .config .config.user-backup  # omit if .config does not exist
cp configs/gl-be14000-ccs-validation.config .config
make defconfig
```

Modules selected `m` are built but not installed into the default rootfs.
Some preserve shipping kernel compile-time inputs needed by other modules.
The public image does not restore the proprietary GL.iNet applications or UI.
`make distclean` removes configuration and build inputs; it is not routine
incremental-build cleanup. The default `.config` must be included in the
published source snapshot as well as these recovery templates.

## Feed and Patch Mapping

The public build uses the exact revisions in `feeds.conf.default`. The release includes:

- OpenWrt, LuCI, routing and telephony feeds.
- MediaTek's pinned public feed.
- GL.iNet public `gl_feed_common` and `gl_feed_21_02`.
- Public VPN and MPTCP feeds at pinned revisions.
- Kernel patches under `target/linux/mediatek/patches-5.4/`.
- The OpenWrt kernel package definition for `chacha20poly1305.ko`.
- Retained GL.iNet kernel-module source under `package/kernel/glinet/`, including `gl-sdk4-port-forward`.

## Validation Status (2026-09-18)

This is a delivery candidate, not a completed CCS or licensing acceptance.
The latest compared public image, built before the input changes below, has
SHA256 `ecb79224fdc247389c2e0bf6867d2aa53e4b73763895d8ace72a89528151e2b1`.
Its full module comparison against the shipping image above found 245 shipping
modules: 230 byte-identical, 15 different, zero missing, and 18 public extras.
Userspace ELF comparison found 260 identical, 197 different, 186 shipping-only
and 47 public-only files. These are inventory counts, not licensing decisions.
Of the 197 differences, 180 Samba files have identical allocated sections,
one differs only in GNU Build ID, and 16 require additional investigation.

Subsequent input corrections restore five HNAT source files from verified
shipping commits, align TRACE/FireWire/MBIM/ZRAM and USB PHY/BSG configuration,
and deactivate the unshipped WireGuard header enum extension. These changes
have not been compiled. The previous image hash and comparison must not be
presented as validation of the changed inputs.

Remaining acceptance work includes rebuilding and comparing the changed inputs,
hostapd source/runtime correspondence, the remaining module and userspace
differences, missing shipping payload mapping, binary redistribution and source
scope review, a clean public-dependency build, and device installation/function
tests. Installation instructions above describe the intended workflow and are
not a record of a device test performed in this round. The historical 2026-09-15
comparison does not establish acceptance of this snapshot.

## Source layout

- `configs/`: baseline, public-build and CCS-validation configurations.
- `feeds.conf.default`: pinned feed URLs and revisions.
- `package/kernel/glinet/`: retained GL.iNet kernel-module source.
- `package/kernel/linux/modules/`: OpenWrt kernel module package definitions.
- `package/firmware/mtk-be14000-binary-runtime/`: ABI-locked MediaTek runtime IPKs.
- `target/linux/mediatek/`: kernel, DTS, board support, image definitions and patches.
