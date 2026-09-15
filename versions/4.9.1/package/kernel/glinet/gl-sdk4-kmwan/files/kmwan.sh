. /usr/share/libubox/jshn.sh
. /lib/functions/network.sh
. /lib/functions.sh

SET_BASE="0"
FLUSH_CFG="1"
ADD_DEV="2"
RM_DEV="3"
ENABLE_PROBE="4"
STOP_PROBE="5"
SYNC_ROUTE="6"
FORCE_DEAD="7"
RESTORE_DETECT="8"

config_load kmwan

config_apply()
{
    test -z "$1" && return 1
    if [ -e "/proc/gl-kmwan/config" ];then
        logger -t kmwan "config json str=$1"
        echo "$1" >/proc/gl-kmwan/config
    fi
}

netcell_set_base_config()
{
    local sensitivity mode rtmode
    config_load kmwan
    config_get sensitivity "global" "sensitivity"
    config_get mode "global" "mode"
    rtmode=$(uci get glconfig.general.mode)
    if [ -n "$rtmode" -a "$rtmode" = "passthrough" ]; then
        rtmode=1
    else
        rtmode=0
    fi

    json_init
    json_add_int "op" $SET_BASE
    json_add_object "data"
    json_add_int "sensitivity" $sensitivity
    json_add_string "mode" "$mode"
    json_add_int "rtmode" $rtmode

    json_str=`json_dump`
    config_apply "$json_str"
    json_cleanup
}

modem_get_force_ip()
{
    local interface="$1"
    local loop_cnt=0
    local force_ip
    local ip_type

    config_load network
    [ -n "$(echo $interface|grep _6)" ] && return
    config_get ip_type "$interface" "ip_type"
    [ -n "$ip_type" -a "$ip_type" = "IPV6" ] && return

    [ -z "$(echo $interface|grep modem|grep 4)" ] && interface=${interface}_4
    while true; do
        force_ip=$(ubus call network.interface.$interface status 2>/dev/null|jsonfilter -q -e '@["ipv4-address"][0]["address"]')
        let loop_cnt=loop_cnt+1
        [ $loop_cnt -gt 40 -o -n "$force_ip" ]  && break
        sleep 1;
    done;
    [ -z "force_ip" ] && force_ip="0.0.0.0"
    echo "$force_ip"
}

load_netcell_from_config()
{
    local found=0
    config_load kmwan
    json_init
    json_add_int "op" $ADD_DEV
    json_add_object "data"
    json_add_array "cells"
    find_config_cb(){
        local member=$1
        local disabled interface tracks netdev track_mode addr_type force_ip
        config_get disabled "$member" "disabled"
        [ "$disabled" = "1" ] && return
        config_get interface "$member" "interface"
        network_get_device netdev "$interface"
        [ -z "$netdev" ] && return
        found=1
        config_get track_mode "$member" "track_mode"
        config_get tracks "$member" "tracks"
        config_get addr_type "$member" "addr_type"
        force_ip=$(ubus call network.interface.$interface status 2>/dev/null|jsonfilter -q -e '@["ipv4-address"][0]["address"]')
        if [ -z "$force_ip" -a -n "$(echo $interface|grep modem)" ]; then
            add_netcell $interface &
            return
        fi
        [ -z "$force_ip" ] && force_ip="0.0.0.0"
        json_add_object ""
        json_add_string "interface" "$interface"
        json_add_string "netdev" "$netdev"
        json_add_string "track_mode" "$track_mode"
        json_add_int "addr_type" "$addr_type"
        json_add_string "force_ip" "$force_ip"
        [ -n "$tracks" ] && {
            json_add_array "tracks"
            for item in $tracks;do
                local _type _ip
                _type="$(echo $item|cut -d ',' -f1)"
                _ip="$(echo $item|cut -d ',' -f2)"
                [ -z "$_type" -o -z "$_ip" ] && continue
                json_add_object ""
                json_add_string "type" "$_type"
                json_add_string "ip" "$_ip"
                json_select ..
            done
            json_select ..
        }
        json_select ..
    }
    config_foreach find_config_cb member

    [ "$found" -eq 1 ] && {
        json_str=`json_dump`
        config_apply "$json_str"
    }
    json_cleanup
}

netcell_set_interface_metric()
{
    config_load kmwan
    find_metric_cb(){
        local config=$1
        local disabled interface metric
        config_get disabled "$config" "disabled"
        [ "$disabled" = "1" ] && return
        config_get interface "$config" "interface"
        config_get metric "$config" "metric"
        [ -n "$interface" ] && [ -n "$metric" ] && {
            #echo "[netcell_set_interface_metric]interface $interface metric $metric" >/dev/console
            uci -q set network."$interface".metric="$metric"
        }
    }
    config_foreach find_metric_cb member
    uci commit network
    [ -z "$1" ] && ubus call network reload
}

add_arp_table()
{
    local gw=$(ubus call network.interface.$1 status|jsonfilter -e '@.route[0]["nexthop"]')
    [ -z "$gw" ] && return
    local info=$(ip neigh show | grep -E ''$gw' ' | grep -E ''$2'')
    [ -z "$info" ] && ping -c1 $gw -I $2 -W1
}

add_netcell()
{
    [ $# -lt 1 ] && return
    local found=0
    config_load kmwan
    json_init
    json_add_int "op" $ADD_DEV
    json_add_object "data"
    json_add_array "cells"
    for netcell in $@;do
      find_member_cb(){
        local config=$1
        local disabled interface tracks netdev track_mode addr_type
        config_get disabled "$config" "disabled" #"0"
        [ "$disabled" = "1" ] && return
        config_get interface "$config" "interface"
        [ "$interface" = "$netcell" ] || return
        network_get_device netdev "$netcell"
        [ -z "$netdev" ] && return
        case $1 in
        wan|tethering|wwan|secondwan|usbwan)
            add_arp_table $1 $netdev
            ;;
        *)
            ;;
        esac
        found=1
        config_get track_mode "$config" "track_mode" #"force"
        config_get tracks "$config" "tracks"
        config_get addr_type "$config" "addr_type" 
        json_add_object ""
        json_add_string "interface" "$netcell"
        json_add_string "netdev" "$netdev"
        json_add_string "track_mode" "$track_mode"
        json_add_int "addr_type" "$addr_type"
        force_ip=$(ubus call network.interface.$interface status 2>/dev/null|jsonfilter -q -e '@["ipv4-address"][0]["address"]')
        if [ -z "$force_ip" -a -n "$(echo $interface|grep modem)" ]; then
            force_ip=$(modem_get_force_ip $interface)
        fi
        [ -z "$force_ip" ] && force_ip="0.0.0.0"
        json_add_string "force_ip" "$force_ip"
        [ -n "$tracks" ] && {
            json_add_array "tracks"
            for item in $tracks;do
                local _type _ip
                _type="$(echo $item|cut -d ',' -f1)"
                _ip="$(echo $item|cut -d ',' -f2)"
                [ -z "$_type" -o -z "$_ip" ] && continue
                json_add_object ""
                json_add_string "type" "$_type"
                json_add_string "ip" "$_ip"
                json_select ..
            done
            json_select ..
        }
        json_select ..
      }
      config_foreach find_member_cb member
    done

    [ "$found" -eq 1 ] && {
        json_str=`json_dump`
        config_apply "$json_str"
    }
    json_cleanup
}

netcell_common_ops()
{
    local modem_list
    local is_iface
    [ $# -lt 1 ] && return
    [ $(echo $1|grep modem) ] && modem_list=$(cat /proc/gl-kmwan/config|grep modem|cut -d':' -f1)

    json_init
    json_add_int "op" $2
    json_add_object "data"
    json_add_array "cells"
    if [ -n "$modem_list" ]; then
        for item in $modem_list; do
            [ -n "$(ubus list|grep -E '^network.interface.'$item'$')" ] || {
                json_add_string "" "$item"
                [ "$item" = "$1" ] && is_iface=true
                continue
            }
            [ "$(ubus call network.interface.$item status|jsonfilter -e '@.up')" = "true" ] || {
                json_add_string "" "$item"
                [ "$item" = "$1" ] && is_iface=true
                continue
            }
        done
    fi

    [ -z "$is_iface" ] && json_add_string "" "$1"
    json_str=`json_dump`
    config_apply "$json_str"
    json_cleanup
}

remove_netcell()
{
    netcell_common_ops "$1" "$RM_DEV"
}

start_probe()
{
    netcell_common_ops "$1" "$ENABLE_PROBE"
}

stop_probe()
{
    netcell_common_ops "$1" "$STOP_PROBE"
}

force_dead()
{
    netcell_common_ops "$1" "$FORCE_DEAD"
}

restore_detect()
{
    netcell_common_ops "$1" "$RESTORE_DETECT"
}

flush_netcell()
{
    json_init
    json_add_int "op" $FLUSH_CFG
    json_add_object "data"
    json_str=`json_dump`
    config_apply "$json_str"
    json_cleanup
}

sync_route_netcell()
{
    find_member_cb(){
        local config=$1
        local interface netdev
        config_get interface "$config" "interface"
        network_get_device netdev "$interface"
        [ -z "$netdev" ] && return
        case $1 in
        wan|tethering|wwan|secondwan|usbwan)
            add_arp_table $1 $netdev
            ;;
        *)
            ;;
        esac
    }
    config_foreach find_member_cb member
    json_init
    json_add_int "op" $SYNC_ROUTE
    json_add_object "data"
    json_str=`json_dump`
    config_apply "$json_str"
    json_cleanup
}

del_unuse_modem_cfg()
{
    [ -n "$(echo $1|grep modem)" ] || return
    local modem_cnt=$(uci show kmwan|grep interface|grep modem|wc -l)
    [ $modem_cnt -gt 2 ] || return
    local modem_list=$(uci show kmwan|grep interface|grep modem|cut -d'.' -f2)

    for item in $modem_list; do
        local modem_prefix=$item
        local item_6

        [ -n "$(echo $item|grep '_6')" ] && continue
        [ -n "$(ubus list |grep modem|cut -d'.' -f3|grep -E '^'$item'$')" ] && continue
        [ -n "$(echo $item|grep -E '_4$')" ] && modem_prefix=${item%_*}

        item_6="$modem_prefix""_6"
        uci delete kmwan.$item
        uci delete kmwan.$item_6
        uci commit kmwan
        sync
    done
}
