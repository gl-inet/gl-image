# GL-BE10000 Bootloader

This directory contains U-Boot 2025.04-rc3 and the MediaTek TF-A snapshot
`atf-20250304-5ea9f9736` for GL-BE10000 (MT7987). The bootloader is built
separately from the OpenWrt firmware in the parent directory.

## Build Environment

Use a case-sensitive GNU/Linux filesystem. On Ubuntu, install:

```sh
sudo apt install build-essential gcc-aarch64-linux-gnu \
  device-tree-compiler bison flex libssl-dev python3 python3-pyelftools
```

Install these dependencies before building without network access. The platform
DRAM and eFuse objects supplied with the TF-A snapshot are retained under
`atf-20250304-5ea9f9736/plat/mediatek/mt7987/drivers/` as build inputs.

## Build U-Boot

Run from this directory:

```sh
export CROSS_COMPILE=aarch64-linux-gnu-
make -C Uboot-upstream gl_be10000_defconfig
make -C Uboot-upstream -j"$(nproc)"
```

The output is `Uboot-upstream/u-boot.bin`.

## Build TF-A

Run from this directory after building U-Boot:

```sh
cp Uboot-upstream/u-boot.bin atf-20250304-5ea9f9736/u-boot.bin
make -C atf-20250304-5ea9f9736 gl_be10000_defconfig
make -C atf-20250304-5ea9f9736 -j"$(nproc)"
```

The boot images are:

```text
atf-20250304-5ea9f9736/build/mt7987/release/fip.bin
atf-20250304-5ea9f9736/build/mt7987/release/bl2.img
```

Boot images are not sysupgrade images. Installing them requires a separate
bootloader flashing procedure; the parent firmware build and normal firmware
upgrade do not install these images.

Copyright and license notices are preserved in the supplied source files,
`Uboot-upstream/Licenses/`, and `atf-20250304-5ea9f9736/license.rst`.
