![OpenWrt logo](versions/4.9.0/include/logo.png)

GL-MT3600BE source releases:

* [Firmware 4.9.0 build and upgrade guide](versions/4.9.0/GL-MT3600BE_FIRMWARE_GUIDE.md)
* [Firmware 4.8.7 build and upgrade guide](versions/4.8.7/GL-MT3600BE_FIRMWARE_GUIDE.md)

## Configuration types

Each version directory may contain more than one `.config` file. The
configuration files have different purposes and are not selected automatically
by the OpenWrt build system.

| Configuration | Purpose |
| --- | --- |
| `gl-mt3600be-open-source.config` | Clean build containing OpenWrt and other publicly redistributable components, without GL.iNet proprietary modules and applications. |
| `gl-mt3600be-baseline.config` | Internal shipping baseline containing the GL.iNet proprietary modules and applications captured from the corresponding shipping build. |
| `gl-mt3600be-ccs-validation.config` | CCS validation build that enables the GPL/copyleft GL.iNet kernel modules covered by this source release, while excluding GL.iNet proprietary applications and Web UI components. |

To select a configuration, copy it to `.config` before running `make defconfig`:

```sh
cp configs/gl-mt3600be-open-source.config .config
make defconfig
```

The `open-source` configuration is the Clean configuration under this source
release terminology. The `baseline` configuration records the corresponding
shipping build for audit and comparison. The `ccs-validation` configuration
enables the publicly available GPL/copyleft components that must be checked
against the distributed firmware. Each version included in this source
release provides all three configurations in its respective `configs/` directory.

Do not use a baseline configuration from another firmware version to reproduce
a shipping image. Package selections, feed revisions, patches and hardware
options must match the target firmware version.

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
