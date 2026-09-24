# Offline build: GL-BE10000 4.8.6

Clone the `gl-be10000` branch once while online. After the clone completes,
building GL-BE10000 4.8.6 does not require a network connection. Use a 64-bit,
case-sensitive GNU/Linux filesystem and install the build dependencies in
`GL-BE10000_FIRMWARE_GUIDE.md` before disconnecting the network.

Clone and build:

```sh
git clone --branch gl-be10000 https://github.com/gl-inet/gl-image.git
cd gl-image
# The remaining commands work without network access.
cd versions/4.8.6
make -j"$(nproc)"
```

`offline-inputs/feeds/` contains pinned feed source and a snapshot of the
`wifidog-ng` submodule. `offline-inputs/feed-metadata/` retains the Git
commit/tree information needed for the release's package version strings.
`offline-inputs/dl/` holds individual upstream package archives, and
`offline-inputs/go-mod-cache/` holds Go module source archives. The preparation
script checks the downloaded input hashes, copies them into OpenWrt's ignored
`feeds/`, `dl/` and `tmp/` work directories, then indexes local feeds with
`feeds update -i -a`. Do not run `feeds update -a` offline.

`offline-sources/linux-5.4.281/` is the unpatched kernel source;
`offline-sources/bpftools-5.10.10/` contains only the 5.10 sources used to
build libbpf/bpftools; `offline-sources/linux-firmware-20211216/` includes
the EIP197 firmware payloads and their upstream WHENCE notice. The build reads
these directories locally rather than downloading the corresponding tarballs.
The selected MTK runtime IPKs are already in `package/firmware/`.

The resulting image and package manifest are in
`bin/targets/mediatek/mt7987/`. Refer to
`GL-BE10000_FIRMWARE_GUIDE.md` for firmware installation.
