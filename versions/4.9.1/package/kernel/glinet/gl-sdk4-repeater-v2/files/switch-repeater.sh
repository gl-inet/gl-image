#!/bin/sh

action=$1

[ "$action" = "on" ] && {
    status=$(ubus call repeater status | jsonfilter -e @.state_s)

    [ "$status" = "connected" ] && exit 0

    count=1
    while [ $count -le 5 ]
    do
        saved_list=$(ubus call gl-session call "{\"module\":\"repeater\",\"func\":\"get_saved_ap_list\",\"params\":{}}" | jsonfilter -e @.result | jsonfilter -e @.res)
        config=$(echo "$saved_list" | jsonfilter -e '$[0]')

        [ "$(echo $config | grep "ssid")" = "" ] && sleep 1 || break
 
        count=$(($count+1))
    done

    [ "$(uci -q get repeater.@main[0].disabled)" = "1" ] && {
        uci set repeater.@main[0].disabled="0"
        uci commit repeater
        ubus call repeater reload
    }

    [ "$(echo $config | grep "ssid")" ] && ubus call repeater connect "$config"
}

[ "$action" = "off" ] && {
    count=1
    while [ $count -le 5 ]
    do
        [ "$(pgrep -f "gl-repeater")" = "" ] && sleep 1 || break
 
        count=$(($count+1))
    done

    [ "$(uci -q get repeater.@main[0].disabled)" = "0" ] && {
        uci set repeater.@main[0].disabled="1"
        uci commit repeater
        ubus call repeater reload
    }

    ubus call repeater disconnect
}

sleep 5
