#!/bin/sh
# shellcheck disable=SC2164

. /lib/functions/gl_util.sh

repeater_dbg_dir="/tmp/repeater_dbg"
repeater_dbg_file="/tmp/repeater_dbg/repeater_log"
repeater_log_file="/tmp/repeater_dbg/logread"
repeater_kmwan_file="/tmp/repeater_dbg/kmwan_log"
wifi_ifaces=""


rm  -rf "$repeater_dbg_dir"
mkdir -p "$repeater_dbg_dir"

[ -f "/var/log/gl-repeater/portal.html" ] && {
    echo "==============Detect result in /var/log/gl-repeater/portal.html:"
    cat /var/log/gl-repeater/portal.html

    cp /var/log/gl-repeater/portal.html "$repeater_dbg_dir" 
} || {
    echo "==============/var/log/gl-repeater/portal.html does not exist" | tee -a "$repeater_dbg_file"
}

exe_and_record() 
{
    local cmd="$1"
    echo | tee -a "$repeater_dbg_file"
    echo "==================================================================" | tee -a "$repeater_dbg_file"
    echo "$cmd" | tee -a "$repeater_dbg_file"
    echo "==================================================================" | tee -a "$repeater_dbg_file"
    eval "$cmd" | tee -a "$repeater_dbg_file"
}

fetch_wifi_iface()
{
    config_get mode $1 mode
    config_get mld $1 mld
    config_get ifname $1 ifname
    config_get iface_network $1 network

    [ "$mode" != "ap" ] || [ "$iface_network" = "guest" ] || [ "$mld" != "" ] && return

    [ "$ifname" != "" ] && iface=$ifname || iface=$1

    [ -z "$wifi_ifaces" ] && wifi_ifaces="$iface" || wifi_ifaces="$wifi_ifaces $iface"
}

ifname=$(ifstatus wwan | jsonfilter -e '@.device')

exe_and_record "echo ifname: $ifname"

[ -n "$ifname" ] || exit 1

#debug for portal detection
exe_and_record "iwinfo $ifname info"

exe_and_record "ip route"

exe_and_record "nslookup captive.apple.com"

exe_and_record "nslookup www.msftconnecttest.com"

exe_and_record "curl -s -v http://captive.apple.com/hotspot-detect.html 2>&1"

exe_and_record "curl -s -v http://www.msftconnecttest.com/connecttest.txt 2>&1"

exe_and_record "ping 1.1.1.1 -c 3 -I $ifname"
exe_and_record "ping 8.8.8.8 -c 3 -I $ifname"

#backup logread
logread > $repeater_log_file

#check wireless dev status and information
config_load wireless
config_foreach fetch_wifi_iface wifi-iface

for interface in $wifi_ifaces; do
    stat_file="/tmp/repeater_dbg/wireless_status_$interface"

    exe_and_record "echo 'check status of $interface, and save log in $stat_file'"
    [ $(which iwpriv) ] && [ $(uci -q get wireless.@wifi-device[0].type | grep "mtk") ] && {
        count=1
        while [ $count -le 5 ]
        do
            iwpriv $interface show stat
            iwpriv $interface show MibBucket
            iwpriv $interface show mibinfo
            iwpriv $interface show stainfo=1
            iwpriv $interface stat >> $stat_file
            sleep 2
            count=$(($count+1))
        done
    }

    [ $(which cfg80211tool) ] && exe_and_record "cfg80211tool $interface get_chutil"

    logread >> $stat_file
done

#debug for kmwan
exe_and_record "echo 'Turn on kmwan debug mode for 15 seconds, and save log in repeater_kmwan_file'"
echo 4 > /proc/gl-kmwan/debug && sleep 15 && echo 0 > /proc/gl-kmwan/debug

logread > $repeater_kmwan_file

#package debug files
rm -rf /tmp/repeater_dbg.tar
rm -rf /www/repeater_dbg.tar

cd /tmp
tar -cf repeater_dbg.tar repeater_dbg/
ln -s /tmp/repeater_dbg.tar /www/
