#!/bin/sh

. /usr/share/libubox/jshn.sh
FILE_PATH="/tmp/mplinks"
LOCAL_FILE_PATH="/tmp/local_resource"

#on or off
DEBUG_IP="off"

flag=""
res=""
ip=""
ip6=""
pub_ip=""
pub_ip6=""

build_jsonstr()
{
    if [ -n "$2" ]; then
        json_add_string "$1" "$2"
    else	
        json_add_string "$1" "$2"
    fi
}

add_linkinfo_by_json()
{
    [ "$DEBUG_IP" = "on" ] && ip="$pub_ip"
    json_add_object "$1"	
    json_add_string "name" "$2"
    build_jsonstr "ipv4" "$ip"
    build_jsonstr "ipv6" "$ip6"
    build_jsonstr "pubIpv4" "$pub_ip"
    build_jsonstr "pubIpv6" "$pub_ip6"

    json_close_object "$1"
}

add_local_resource_by_json()
{
    [ $# -ne 5 -a $# -ne 4 ] && return
    json_add_object "$1"
    json_add_string "network" "$2"
    json_add_string "type" "$3"
    json_add_string "interfaceName" "$4"
    [ $# -eq 5 ] && json_add_string "gatewayIp" "$5"
    json_close_object 
}

build_nullinfo_by_json()
{
    json_init
    json_add_array 'data'
    json_close_array

    res="$(json_dump)"
    json_cleanup
    if [ $# -ne 1 ]; then
        echo "$res" > $FILE_PATH
    else
        echo "$res" > $LOCAL_FILE_PATH
    fi
}

get_link_info()
{
    local ifaces=$(ubus list|grep interface.|grep -v -E 'loopback|lan|4|6')
    local i=0
    flag=""

    for item in $ifaces; do
        local online_dev=""
        ip="" pub_ip=""
        ip6="" pub_ip6=""
        local up ubus_data up6 ubus6_data

        if [ -n "$(echo $item|grep modem)" ]; then
            ubus_data="$(ubus call ${item}_4 status)"
        else
            ubus_data="$(ubus call $item status)"
        fi
        up="$(echo $ubus_data|jsonfilter -e "@.up")"
        [ $up = true ] && online_dev="$(echo $ubus_data|jsonfilter -e "@.l3_device")"
        [ -n "$online_dev" ] || continue

        ip="$(echo $ubus_data|jsonfilter -e '@["ipv4-address"][0]["address"]')"
        if [ -n "$(echo $item|grep modem)" ]; then
            ubus6_data="$(ubus call network.interface.${item}_6 status)" 2>/dev/null
        else
            ubus6_data="$(ubus call network.interface.${item}6 status)" 2>/dev/null
        fi

        [ -n "$ubus6_data" ] && {
            up6="$(echo $ubus6_data|jsonfilter -e '@.up')"
            [ "$up6" = "true" ] && ip6="$(echo $ubus6_data|jsonfilter -e '@["ipv6-address"][0]["address"]')"
        }

        pub_ip=$(curl --connect-timeout 2 --max-time 4 --interface $online_dev -s tool.gl-inet.com/ip -4)
        pub_ip6=$(curl --connect-timeout 2 --max-time 4 --interface $online_dev -s tool.gl-inet.com/ip -6)

        if [ -z "$flag" ]; then
            flag=1
            json_init
            json_add_array 'data'
            add_linkinfo_by_json "$i" "$online_dev" "$ip" "$pub_ip" "$ip6" "$pub_ip6"
        else
            add_linkinfo_by_json "$(expr $i + 1)" "$online_dev" "$ip" "$pub_ip" "$ip6" "$pub_ip6"
        fi
    done

    [ -z "$flag" ] && return

    json_close_array
    res="$(json_dump)"
    json_cleanup
    echo "$res" > $FILE_PATH
}

get_local_resource()
{
    local ifaces=$(ubus list|grep interface.|grep -v -E 'loopback|4|6')
    local i=0
    flag=""
    res=""

    for item in $ifaces; do
        local online_dev=""
        local local_ip mask local_gw type
        local up ubus_data network

        if [ -n "$(echo $item|grep modem)" ]; then
            ubus_data="$(ubus call ${item}_4 status)"
        else
            ubus_data="$(ubus call $item status)"
        fi
        [ -z "$ubus_data" ] && continue
        up="$(echo $ubus_data|jsonfilter -e "@.up")"
        [ $up = true ] && online_dev="$(echo $ubus_data|jsonfilter -e "@.l3_device")"
        [ -n "$online_dev" ] || continue
        local_ip="$(echo $ubus_data|jsonfilter -e '@["ipv4-address"][0]["address"]')"
        mask="$(echo $ubus_data|jsonfilter -e '@["ipv4-address"][0]["mask"]')"
        network=$(ipcalc.sh ${local_ip}/${mask}|grep NETWORK|cut -d'=' -f2)

        if [ -z "$(echo $item|grep lan)" ]; then
            local_gw="$(echo $ubus_data|jsonfilter -e '@["route"][0]["nexthop"]')"
            type="wan"
        else
            type="lan"
        fi

        if [ -z "$flag" ]; then
            flag=1
            json_init
            json_add_array 'data'
            add_local_resource_by_json "$i" "${network}/${mask}" "$type" "$online_dev" "$local_gw"
        else
            add_local_resource_by_json "$(expr $i + 1)" "${network}/${mask}" "$type" "$online_dev" "$local_gw" 
        fi
    done

    [ -z "$flag" ] && return

    json_close_array
    res="$(json_dump)"
    json_cleanup
    echo "$res" > $LOCAL_FILE_PATH
}

if [ "$1" = "local" ]; then
    get_local_resource
    [ -z "$flag" ] && build_nullinfo_by_json 1
else
    get_link_info
    [ -z "$flag" ] && build_nullinfo_by_json
fi
