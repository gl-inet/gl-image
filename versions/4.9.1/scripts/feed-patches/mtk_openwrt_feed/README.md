# BE14000 MediaTek Module Source Patch

The public MediaTek feed is pinned to commit
`5fbb639c7ab50e92553da94f5920185859fd1f2b`.
`100-be14000-crypto-eip-pce.patch` supplies changes to 17 files in
`feed/kernel/crypto-eip` and `feed/kernel/pce` corresponding to the shipping
feed revision `42be1aec3f61fda84e928821bc5a032b8075d645`.

`./scripts/feeds update -a` invokes
`scripts/apply-be14000-gpl-feed-patches.sh` after obtaining the MediaTek feed
and before creating its package index. The helper checks the feed commit
and `input.sha256`, applies the patch without fuzz, and checks `output.sha256`.
Repeated updates accept the patched state. Unexpected revisions or contents
cause an error; check the configured feed and local edits before retrying.

Preserve the copyright and license notices in the EIP/DDK and PCE source
files. The patch applies only to those source directories; it does not
change other MediaTek packages.

The cfg80211/backports patch is supplied separately under
`package/kernel/mac80211/patches/subsys/914-be14000-shipping-cfg80211.patch`.
Kernel target patches, including nftables hardware-offload support, are in
`target/linux/mediatek/patches-5.4/`.
