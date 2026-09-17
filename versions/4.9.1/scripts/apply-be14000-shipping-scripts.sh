#!/bin/sh
set -eu

[ "$#" -eq 1 ] && [ -d "$1" ] || {
    echo "Usage: $0 ROOTFS_DIRECTORY" >&2
    exit 1
}
root=$(readlink -f "$1")
[ "$root" != / ] || { echo "Refusing host root filesystem" >&2; exit 1; }
tree=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
patchdir="$tree/scripts/rootfs-patches/shipping-independent"

for rel in bin/config_generate etc/init.d/boot lib/functions/system.sh lib/functions.sh; do
    [ -f "$root/$rel" ] && [ ! -L "$root/$rel" ] || {
        echo "Shipping-script patch: missing or symlink destination: $rel" >&2
        exit 1
    }
    case "$(readlink -f "$root/$rel")" in
        "$root"/*) ;;
        *) echo "Shipping-script patch: destination escapes rootfs: $rel" >&2; exit 1 ;;
    esac
done

if (cd "$root" && sha256sum -c "$patchdir/output.sha256" >/dev/null 2>&1); then
    echo "Shipping-script patch already applied"
elif (cd "$root" && sha256sum -c "$patchdir/input.sha256" >/dev/null 2>&1); then
    patch -d "$root" -p1 --batch --fuzz=0 --forward --dry-run \
        -i "$patchdir/100-independent-shipping-base-files.patch"
    patch -d "$root" -p1 --batch --fuzz=0 --forward \
        -i "$patchdir/100-independent-shipping-base-files.patch"
    (cd "$root" && sha256sum -c "$patchdir/output.sha256")
else
    echo "Shipping-script inputs changed; review the release patch before building" >&2
    exit 1
fi
