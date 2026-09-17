![OpenWrt logo](include/logo.png)

## GL-BE14000 4.9.1 source release

This tree preserves the OpenWrt build system and contains the GL-BE14000 board
DTS, image definition, network mapping, upgrade support and GL.iNet hardware
drivers as source code. MediaTek components that are not distributed as source
are packaged under `package/firmware/mtk-be14000-binary-runtime/` as ABI-locked runtime
files for Linux 5.4.281.

| Area | Delivery form |
| --- | --- |
| OpenWrt framework, board support and GL.iNet hardware drivers | Source code |
| MT7990/MT7991 Wi-Fi stack and control programs | Binary runtime package |
| RTL8261 PHY and YT92xx switch support | Binary runtime package |
| MediaTek mapfilter, forwarding and QoS extensions | Binary runtime package |

The binary runtime packages are selected by the versioned device configuration
and are included automatically in the generated firmware image.

### Build GL-BE14000

Use a case-sensitive GNU/Linux filesystem and install the normal OpenWrt build
dependencies. Then run:

```sh
./scripts/feeds update -a
./scripts/feeds install -a
cp configs/gl-be14000-open-source.config .config
make defconfig
make -j1
```

The sysupgrade image is generated under `bin/targets/mediatek/mt7988/`.
Do not change the kernel version or ABI-relevant kernel configuration unless
matching MediaTek runtime modules are supplied for the new ABI.

For the complete release-specific build, CCS validation, source mapping and
installation instructions, read `GL-BE14000_FIRMWARE_GUIDE.md`. The generic
OpenWrt Quickstart above is for development and does not reproduce the shipping
firmware by itself.

## Build Configuration Files

- `configs/gl-be14000-baseline.config`: internal shipping-build configuration
  for audit and comparison only; it is not a standalone customer-buildable
  configuration.
- `configs/gl-be14000-open-source.config`: public firmware configuration with
  independent GL.iNet proprietary applications and Web UI excluded.
- `configs/gl-be14000-ccs-validation.config`: CCS validation configuration that
  enables the published GPL/copyleft components without restoring proprietary
  applications or the commercial Web UI.

## OpenWrt

OpenWrt Project is a Linux operating system targeting embedded devices. Instead
of trying to create a single, static firmware, OpenWrt provides a fully
writable filesystem with package management. This frees you from the
application selection and configuration provided by the vendor and allows you
to customize the device through the use of packages to suit any application.
For developers, OpenWrt is the framework to build an application without having
to build a complete firmware around it; for users this means the ability for
full customization, to use the device in ways never envisioned.

Sunshine!

## Development

To build your own firmware you need a GNU/Linux, BSD or MacOSX system (case
sensitive filesystem required). Cygwin is unsupported because of the lack of a
case sensitive file system.

### Requirements

You need the following tools to compile OpenWrt, the package names vary between
distributions. A complete list with distribution specific packages is found in
the [Build System Setup](https://openwrt.org/docs/guide-developer/build-system/install-buildsystem)
documentation.

```
gcc binutils bzip2 flex python3 perl make find grep diff unzip gawk getopt
subversion libz-dev libc-dev rsync which
```

### Quickstart

1. Run `./scripts/feeds update -a` to obtain all the latest package definitions
   defined in feeds.conf / feeds.conf.default

2. Run `./scripts/feeds install -a` to install symlinks for all obtained
   packages into package/feeds/

3. Run `make menuconfig` to select your preferred configuration for the
   toolchain, target system & firmware packages.

4. Run `make` to build your firmware. This will download all sources, build the
   cross-compile toolchain and then cross-compile the GNU/Linux kernel & all chosen
   applications for your target system.

### Related Repositories

The main repository uses multiple sub-repositories to manage packages of
different categories. All packages are installed via the OpenWrt package
manager called `opkg`. If you're looking to develop the web interface or port
packages to OpenWrt, please find the fitting repository below.

* [LuCI Web Interface](https://github.com/openwrt/luci): Modern and modular
  interface to control the device via a web browser.

* [OpenWrt Packages](https://github.com/openwrt/packages): Community repository
  of ported packages.

* [OpenWrt Routing](https://github.com/openwrt/routing): Packages specifically
  focused on (mesh) routing.

## Support Information

For a list of supported devices see the [OpenWrt Hardware Database](https://openwrt.org/supported_devices)

### Documentation

* [Quick Start Guide](https://openwrt.org/docs/guide-quick-start/start)
* [User Guide](https://openwrt.org/docs/guide-user/start)
* [Developer Documentation](https://openwrt.org/docs/guide-developer/start)
* [Technical Reference](https://openwrt.org/docs/techref/start)

### Support Community

* [Forum](https://forum.openwrt.org): For usage, projects, discussions and hardware advise.
* [Support Chat](https://webchat.oftc.net/#openwrt): Channel `#openwrt` on **oftc.net**.

### Developer Community

* [Bug Reports](https://bugs.openwrt.org): Report bugs in OpenWrt
* [Dev Mailing List](https://lists.openwrt.org/mailman/listinfo/openwrt-devel): Send patches
* [Dev Chat](https://webchat.oftc.net/#openwrt-devel): Channel `#openwrt-devel` on **oftc.net**.

## License

OpenWrt is licensed under GPL-2.0


## Build Configuration Files

This source release provides three configuration files:

### `configs/gl-be14000-baseline.config`

Configuration captured from the internal shipping firmware build for audit and comparison purposes. It may reference GL.iNet proprietary components that are not included in this public source release.

### `configs/gl-be14000-open-source.config`

Configuration for building the public open-source firmware image. Independent GL.iNet proprietary applications and Web UI components are excluded.

### `configs/gl-be14000-ccs-validation.config`

Configuration for CCS verification. It enables the GPL/copyleft kernel components corresponding to the distributed firmware, including the retained GL.iNet kernel modules, public MTK kernel modules, VPN modules, and port-forward module source. Independent proprietary applications and Web UI components are excluded.

## CCS Build

For the public open-source image:

```sh
cp configs/gl-be14000-open-source.config .config
make defconfig
make prereq
make -j"$(nproc)"
```

For CCS validation:

```sh
cp configs/gl-be14000-ccs-validation.config .config
make defconfig
make prereq
make -j"$(nproc)"
```

The CCS configuration is for source and module correspondence validation. It is not the commercial GL.iNet firmware configuration and does not restore proprietary applications or the GL.iNet Web UI.

## Corresponding Source Notes

- `feeds.conf.default` pins OpenWrt, MTK, GL.iNet public, VPN, and MPTCP feed revisions.
- `package/kernel/glinet/` contains retained GL.iNet kernel-module source, including `gl-sdk4-port-forward`.
- `target/linux/mediatek/patches-5.4/` contains the kernel patches used by this release.
- `package/kernel/linux/modules/crypto.mk` contains the package definition required for `chacha20poly1305.ko`.
- `package/firmware/mtk-be14000-binary-runtime/` contains ABI-locked MediaTek runtime binaries that are distributed under their applicable license terms.
