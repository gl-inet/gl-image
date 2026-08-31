#!/bin/sh
set -eu

TOPDIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$TOPDIR"

./scripts/feeds update -a

# package/feeds contains generated symlinks only. Recreate them in release
# precedence order so pinned GL packages win over public packages with the
# same source-package name.
if [ -d package/feeds ]; then
	find package/feeds -type l -delete
fi

for feed in \
	gl_feed_common \
	gl_feed_smstools3 \
	gl_vpn_extra \
	glinet \
	packages \
	luci \
	routing \
	telephony \
	mtk_openwrt_feed
do
	./scripts/feeds install -a -f -p "$feed"
done
