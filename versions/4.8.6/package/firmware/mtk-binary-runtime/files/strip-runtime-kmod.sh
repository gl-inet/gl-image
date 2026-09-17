#!/bin/sh
# Keep the exact released bytes of modules imported from the shipping image.
case "${1##*/}" in
    mapfilter.ko|mtfwd.ko|mtqos.ko|mt_wifi.ko|mt_wifi_osal.ko|mtk_hwifi.ko|\
    connac_if.ko|mt7990.ko|mt7990_dbg.ko|mt7991.ko|mt_spectrum.ko|\
    mtk_pci.ko|mtk_warp.ko|mtk_wed.ko)
        ;;
    *)
        exec "$(dirname "$0")/../../../../scripts/strip-kmod.sh" "$@"
        ;;
esac
