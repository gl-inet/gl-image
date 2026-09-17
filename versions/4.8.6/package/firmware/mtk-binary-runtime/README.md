# MediaTek Runtime Inputs

These inputs belong to the BE10000 release identified by the version-level
firmware guide. They are not interchangeable with another device or kernel ABI.

## Assembly

- `mtk-wifi-runtime` installs Wi-Fi drivers, firmware and vendor helpers.
- `mtk-network-runtime` installs the retained network extension modules.
- `mtk-phy-runtime` installs the existing internal PHY runtime.
- `mtk-tools-runtime` installs the retained mesh and diagnostic tools, including
  1905daemon, ated_ext, mapd/libmapd, qtt_daemon, sigma_daemon and sigma_dut,
  together with their retained MediaTek configuration files.

Hostapd and wpad are built from the release source package. Their old bundled
IPKs are skipped during runtime assembly. The supplied build configurations
also select hostapd-utils and wpa-cli.

Prebuilt user-space files are not stripped again. The kernel-module strip
wrapper preserves its explicit released-module list and otherwise delegates
to the ordinary OpenWrt module stripping script.

## Provenance and Licenses

`manifests/shipping-userspace.json` maps the imported payloads to the official
firmware SHA256, reference IPKs, package recipes and per-file checksums. It also
records the available license evidence. Payloads were matched against the
official image, not assumed correct because an IPK had the same filename.

The `*-ipk-sha256.txt` manifests describe bundled input archives. The
`*-payload-sha256.txt` manifests describe regular files inside those archives,
including unused hostapd inputs in the Wi-Fi group; they are not manifests of
the final assembled rootfs. The JSON mapping also records symlinks and modes
for the newly imported user-space payloads.

`files/tool-licenses` retains discovered upstream notices. Some package recipes
have no license field; some notices mix third-party permissions with MediaTek
restrictions. These records do not establish that every file is BSD-licensed
or that all necessary redistribution permissions have been confirmed.

`sources/mtk-base-files` retains the GPL script package and its original
build recipe, named `Makefile.reference` to prevent automatic discovery as a
second active OpenWrt package. The IPK also contains the installed scripts in
source form. This package must not be classified as proprietary merely because
it is transported in an IPK.

The mwctl recipe declares GPLv2 while its license file also contains BSD terms
and MediaTek-specific text. The sigma_dut notices contain both ISC and GPL
texts. The applicable terms must be mapped to the actual delivered files;
neither finding alone proves an unrestricted binary-only grant for the whole
package. No private vendor C source is imported by this runtime package.
