#!/bin/sh
# These imported modules already match the released BE14000 firmware.
if [ -n "$CROSS" ] && [ "$#" -eq 1 ] && [ -f "$1" ]; then
    case "${1##*/}" in
        mapfilter.ko|mtfwd.ko|mtqos.ko|rtl8261x-mdio.ko|ytswconfig.ko|\
        connac_if.ko|mt7990.ko|mt7990_dbg.ko|mt7991.ko|mt_spectrum.ko|\
        mtk_pci.ko|mtk_wed.ko|mtk_warp.ko)
            exit 0
            ;;
    esac
fi
exec "$(dirname "$0")/../../../../scripts/strip-kmod.sh" "$@"
