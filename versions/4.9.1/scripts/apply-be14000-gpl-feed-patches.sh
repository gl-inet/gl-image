#!/bin/sh
set -eu

if [ "$#" -ne 1 ] || [ ! -d "$1" ]; then
    printf 'Usage: %s OPENWRT_TREE\n' "$0" >&2
    false
fi

tree=$(CDPATH= cd -- "$1" && pwd -P)
feed="$tree/feeds/mtk_openwrt_feed"
patchdir="$tree/scripts/feed-patches/mtk_openwrt_feed"
expected=5fbb639c7ab50e92553da94f5920185859fd1f2b

if [ ! -d "$feed/.git" ] || [ ! -f "$patchdir/100-be14000-crypto-eip-pce.patch" ]; then
    printf 'Missing pinned MediaTek feed or BE14000 source patch\n' >&2
    false
fi

actual=$(git -C "$feed" rev-parse HEAD)
if [ "$actual" != "$expected" ]; then
    printf 'MediaTek feed revision %s is not the expected %s\n' "$actual" "$expected" >&2
    false
fi

if (cd "$feed" && sha256sum -c "$patchdir/output.sha256" >/dev/null 2>&1); then
    printf 'BE14000 GPL feed patch already applied\n'
elif (cd "$feed" && sha256sum -c "$patchdir/input.sha256" >/dev/null 2>&1); then
    patch -d "$feed" -p1 --batch --fuzz=0 --dry-run \
        -i "$patchdir/100-be14000-crypto-eip-pce.patch"
    patch -d "$feed" -p1 --batch --fuzz=0 \
        -i "$patchdir/100-be14000-crypto-eip-pce.patch"
    (cd "$feed" && sha256sum -c "$patchdir/output.sha256")
else
    printf 'MediaTek GPL sources differ from both pinned input and shipping output\n' >&2
    false
fi
