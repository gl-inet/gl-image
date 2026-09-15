#!/bin/sh

. /usr/share/libubox/jshn.sh
. /lib/functions.sh

[ $# -ne 2 ] && {
    echo "Usage: $0 'GL.iNet-5G' 'goodlife-cd'"
    exit 1
}

SSID="$1"
KEY="$2"

APs=

find_wifi_ifname() {
    local ifname mode disabled

    config_get mode "$1" mode
    config_get ifname "$1" ifname
    config_get disabled "$1" disabled

    [ "$disabled" = "1" ] && return
    [ "$mode" != "ap" ] && return

    APs="$APs $ifname"
}

disable_auto_switch() {
    uci set repeater.@main[0].auto='0'
    uci commit repeater
    ubus call repeater reload
    ubus call repeater set_exit '{"exit": true}'
}

check_ap() {
    local ifname

    for ifname in $APs
    do
        iwinfo $ifname info | grep NOHT && {
            echo "AP $ifname Fail"
            /etc/init.d/repeater stop
            return 1
        }
    done

    return 0
}

check_status() {
    local state fail_type ifname
    local cnt=20

    while [ $cnt -gt 0 ];
    do
        let cnt=cnt-1

        json_load "$(ubus call repeater status)"
        json_get_vars state
        json_get_vars fail_type

        [ $state -eq 2 ] && {
            check_ap && return 0 || break
        }

        [ $state -eq 3 ] && {
            echo "Connect Fail: $fail_type"
            break
        }

        sleep 1
    done

    [ $cnt -eq 0 ] && echo "Check timeout"

    /etc/init.d/repeater stop
    return 1
}

do_test() {
    local i=0

    while [ true ];
    do
        let i=i+1
        echo "Test $i..."

        ubus call repeater disconnect
        sleep 2

        /etc/init.d/log restart

        json_init
        json_add_string ssid "$SSID"
        json_add_string key "$KEY"
        ubus call repeater connect "$(json_dump)"

        sleep 5

        check_status || break
    done
}

config_load wireless
config_foreach find_wifi_ifname wifi-iface

disable_auto_switch
do_test
