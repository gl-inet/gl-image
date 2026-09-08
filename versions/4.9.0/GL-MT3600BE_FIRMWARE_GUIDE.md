# GL-MT3600BE Firmware Guide

## Build environment

Use a 64-bit, case-sensitive GNU/Linux environment. The reference environment
verified for this source tree is:

```text
Operating system: Ubuntu 22.04.5 LTS
Architecture:     x86_64
GCC:              9.5.0
GNU Make:         4.3
Python:           3.10.12
Git:              2.34.1
```

Build as a regular user, not as `root`, and use a build path without spaces.
Do not build on a case-insensitive filesystem. When using WSL2, keep the source
tree in the Linux filesystem rather than under `/mnt/c`.

Recommended resources:

```text
CPU:       4 cores minimum, 8 or more recommended
Memory:    8 GiB minimum, 16 GiB or more recommended
Disk:      30 GiB free minimum, 50 GiB or more recommended
Network:   Internet access for feeds and source downloads
```

Two build methods are supported. Docker is recommended because it provides the
same compiler and dependencies for every build. A native Ubuntu 22.04 build is
also supported.

## Build with Docker

Install Docker Engine, then build the build-environment image from the
repository root. Passing the current UID and GID allows the container to write
build output without changing ownership of the source tree:

```sh
docker build \
  --build-arg BUILD_UID="$(id -u)" \
  --build-arg BUILD_GID="$(id -g)" \
  -t gl-mt3600be-builder .
```

Start the container with the repository mounted at `/workspace`:

```sh
docker run --rm -it \
  -v "$PWD:/workspace" \
  gl-mt3600be-builder
```

Run the commands in [Build the firmware](#build-the-firmware) inside the
container. Build output remains in the repository on the host. This Docker
workflow has been verified with a clean source tree to produce the
GL-MT3600BE factory and sysupgrade images.

## Build on Ubuntu 22.04

Install the native build dependencies:

```sh
sudo apt update
sudo apt install -y \
  bash ca-certificates build-essential clang flex bison g++ gawk \
  gcc-multilib g++-multilib \
  gcc-9 g++-9 gcc-9-multilib g++-9-multilib \
  gettext git ccache \
  libncurses-dev libssl-dev libelf-dev \
  python3 python3-distutils python3-setuptools \
  rsync subversion swig unzip zlib1g-dev \
  file wget perl quilt xz-utils zstd time xsltproc
```

Select GCC and G++ 9:

```sh
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 90
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 90
sudo update-alternatives --set gcc /usr/bin/gcc-9
sudo update-alternatives --set g++ /usr/bin/g++-9
```

The package names follow the
[OpenWrt build system setup](https://openwrt.org/docs/guide-developer/toolchain/install-buildsystem)
for Ubuntu 22.04. If the prerequisite check reports a missing package, install
that package before continuing.

## Build the firmware

Run from the repository root:


```sh
cd versions/4.9.0
```

```sh
./scripts/feeds update -a
./scripts/feeds install -a
cp configs/gl-mt3600be-open-source.config .config
make defconfig
make prereq
make -j"$(nproc)"
```

No GL.iNet-specific feed preparation script is required.

After a successful build, use this firmware image:

```text
bin/targets/mediatek/mt7987/openwrt-mediatek-mt7987-glinet_gl-mt3600be-squashfs-sysupgrade.bin
```

## Installation

Connect the computer to the GL-MT3600BE LAN. The router uses
`192.168.8.1` by default.

### GL.iNet web interface

When the router is running standard GL.iNet firmware, the locally built image
can be uploaded from the GL.iNet web interface:

1. Open `http://192.168.8.1` and sign in.
2. Open **System > Upgrade** and select **Local Upgrade**.
3. Select the generated `sysupgrade.bin` image.
4. Do not retain settings when moving from standard GL.iNet firmware to the
   open-source firmware.
5. Start the upgrade and wait for the router to reboot. Do not interrupt power.

### SSH

Upload the firmware to the router:

```sh
scp -O \
  bin/targets/mediatek/mt7987/openwrt-mediatek-mt7987-glinet_gl-mt3600be-squashfs-sysupgrade.bin \
  root@192.168.8.1:/tmp/firmware.bin
```

The `-O` option uses the legacy SCP protocol supported by Dropbear.

Log in to the router and start the upgrade:

```sh
ssh root@192.168.8.1
sysupgrade -n /tmp/firmware.bin
```

The SSH connection closes while the router writes the image and reboots. Do
not interrupt power during this process. After the reboot, the open-source
firmware provides LAN and SSH access at `192.168.8.1`.

The current default configuration does not include LuCI or the proprietary
GL.iNet web interface. The GL.iNet web upgrade method is therefore available
only while the device is still running standard GL.iNet firmware.
