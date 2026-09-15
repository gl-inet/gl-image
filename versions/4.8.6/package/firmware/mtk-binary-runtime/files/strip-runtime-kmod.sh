#!/bin/sh
# Keep the exact released bytes of modules imported from the shipping image.
case "${1##*/}" in
    mapfilter.ko|mtfwd.ko|mtqos.ko|mt_wifi.ko|mt_wifi_osal.ko|mtk_hwifi.ko)
        ;;
    *)
        exec "$(dirname "$0")/../../../../scripts/strip-kmod.sh" "$@"
        ;;
esac
