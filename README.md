![OpenWrt logo](versions/4.8.6/include/logo.png)

GL-BE10000 source releases:

* [Firmware 4.8.6 build and upgrade guide](versions/4.8.6/GL-BE10000_FIRMWARE_GUIDE.md)

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

For GL-BE10000, enter the required release directory under `versions/` and
run these commands in order:

```sh
./scripts/feeds update -a
./scripts/feeds install -a
make defconfig
make prereq
make -j"$(nproc)"
```

Each release directory includes a default `.config` matching its CCS-validation
configuration. No configuration copy is needed for the default build.

Ensure feed updates and installation complete successfully before running
`make defconfig`. Otherwise, package selections from
unavailable feeds, such as `kmod-amneziawg`, can be removed from `.config`.
The feeds are pinned to release revisions; keep those revisions unchanged.

The default build includes the retained public GPL/copyleft components and
excludes independent GL.iNet proprietary applications and Web UI. The named
CCS-validation template is retained for restoring defaults; the open-source
template currently selects the same options. The baseline configuration is for
audit and comparison only. See the release guide linked above for restoring
defaults, build environment requirements and installation instructions.

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
