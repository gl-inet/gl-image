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
cp configs/gl-be14000-open-source.config .config
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

## Source layout

- `configs/gl-be14000-open-source.config`: public device configuration.
- `feeds.conf.default`: pinned feed revisions.
- `package/firmware/mtk-be14000-binary-runtime`: MediaTek runtime IPKs and manifests.
- `target/linux/mediatek`: kernel, DTS, board support and image definitions.
- `package/glinet`: retained GL.iNet source packages.
