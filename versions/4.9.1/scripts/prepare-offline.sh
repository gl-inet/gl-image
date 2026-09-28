#!/bin/sh
set -eu
GL_OFFLINE_PREPARING=1
export GL_OFFLINE_PREPARING

cd "$(dirname "$0")/.."
inputs=offline-inputs

(cd "$inputs" && sha256sum -c dl.sha256 >/dev/null && sha256sum -c go-mod-cache.sha256 >/dev/null)

mkdir -p feeds dl tmp/go-mod-cache/cache/download
while IFS="$(printf '\t')" read -r name commit _source _overlay; do
    [ "$name" = feed ] && continue
    [ -n "$name" ] || continue
    if [ ! -e "feeds/$name" ]; then
        cp -a "$inputs/feeds/$name" "feeds/$name"
        cp -a "$inputs/feed-metadata/$name" "feeds/$name/.git"
    fi
    [ -d "feeds/$name/.git" ] || { echo "Missing feed metadata: $name" >&2; exit 1; }
    [ "$(git -C "feeds/$name" rev-parse HEAD)" = "$commit" ] ||
        { echo "Feed revision mismatch: $name" >&2; exit 1; }
done < "$inputs/feeds.lock.tsv"

while IFS="$(printf '\t')" read -r name path commit; do
    [ "$name" = feed ] && continue
    [ -n "$name" ] || continue
    if [ ! -e "feeds/$name/$path/.git" ]; then
        cp -a "$inputs/submodule-metadata/$path" "feeds/$name/$path/.git"
    fi
    [ "$(git -C "feeds/$name/$path" rev-parse HEAD)" = "$commit" ] ||
        { echo "Submodule revision mismatch: $name/$path" >&2; exit 1; }
done < "$inputs/submodules.lock.tsv"

for source_file in "$inputs"/dl/*; do
    target="dl/${source_file##*/}"
    if [ -e "$target" ] || [ -L "$target" ]; then
        [ ! -L "$target" ] && cmp -s "$source_file" "$target" ||
            { echo "Existing download differs: $target" >&2; exit 1; }
    else
        cp -a "$source_file" "$target"
    fi
done
rsync -a --ignore-existing "$inputs/go-mod-cache/cache/download/" tmp/go-mod-cache/cache/download/
(cd tmp && sha256sum -c ../offline-inputs/go-mod-cache.sha256 >/dev/null)
sha256sum -c "$inputs/dl.sha256" >/dev/null

test -f offline-sources/linux-5.4.281/Makefile
test -f offline-sources/bpftools-5.10.10/tools/lib/bpf/Makefile
test -f offline-sources/linux-firmware-20211216/inside-secure/eip197_minifw/ifpp.bin

apply_offline_patch() {
    feed=$1
    patchfile="$(pwd)/$2"
    if patch -d "feeds/$feed" -p1 --batch --forward --dry-run -R -i "$patchfile" >/dev/null 2>&1; then
        return
    fi
    patch -d "feeds/$feed" -p1 --batch --fuzz=0 --dry-run -i "$patchfile" >/dev/null
    patch -d "feeds/$feed" -p1 --batch --fuzz=0 -i "$patchfile" >/dev/null
}

apply_offline_patch gl_feed_common scripts/feed-patches/gl_feed_common/100-source-sha256.patch
apply_offline_patch gl_feed_common scripts/feed-patches/gl_feed_common/101-offline-go.patch
apply_offline_patch mptcp_feeds scripts/feed-patches/mptcp_feeds/100-offline-go.patch
apply_offline_patch gl_vpn_extra scripts/feed-patches/gl_vpn_extra/100-ovpn-dco-source-sha256.patch
apply_offline_patch gl_vpn_extra scripts/feed-patches/gl_vpn_extra/101-amneziawg-source-sha256.patch

./scripts/feeds update -i -a
./scripts/feeds install -a
# feeds install scans packages before creating the remaining links. Rebuild its
# file list as well as metadata, even when the nested makes share SCAN_COOKIE.
rm -f tmp/.packageinfo tmp/info/.files-packageinfo-*
rm -f tmp/info/.files-packageinfo.mk
GL_OFFLINE_PREPARING=1 OPENWRT_BUILD= make defconfig
GL_OFFLINE_PREPARING=1 OPENWRT_BUILD= make prereq

test "$(grep -c '^CONFIG_PACKAGE_.*=y$' .config)" -eq 508 ||
    { echo "Unexpected package selection count" >&2; exit 1; }
echo "Offline inputs ready; continuing build."
