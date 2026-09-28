#!/bin/sh
# shellcheck disable=SC3037
# shellcheck disable=SC3003

. /lib/functions/gl_util.sh
. /lib/functions/mptun.sh

[ -n "$(echo "${ifname}" | grep "^mptun")" ] || exit 0

MPTUN_CONFIG="/etc/mptun/mpnet.json"

LOCKFILE=/tmp/run/iprb_wg_event.lock

mkdir -p /tmp/wireguard/"${ifname}"/peers
service_mode="$(uci -q get mptun.global.service_mode)"
#IPRB_disabled="$(uci -q get mptun.IPRB.disabled)"
IPRB_identity="$(uci -q get mptun.IPRB.identity)"
[ "${service_mode}" != "IPRB" ] && return 0
#[ "${IPRB_identity}" != "secondary" ] && return 0
WG_BIN='awg'

peerpubkey_safe="$(echo "$peerpubkey" | sed 's/[^A-Za-z0-9._-]//g')"

IPRB_update_p2p_status() {
    get_default_pub() {
        local q="0.0.0.0/0"
        jsonfilter -i "$MPTUN_CONFIG" \
            -e '@.udpTun.peers[*].allowedIps' \
            -e '@.udpTun.peers[*].publicKey' |
        while read ips; do
            read pub || break
            IFS=','
            for cidr in $ips; do
                [ "$cidr" = "$q" ] && echo "$pub"
            done
        done
    }
    get_current_pub() {
        local q="0.0.0.0/0"
        local cur_pub=""

        $WG_BIN show | while read line; do
            case "$line" in
                peer:*)
                    cur_pub="${line#peer: }"
                    ;;
                *"allowed ips:"*)
                    ips="${line#*allowed ips: }"
                    IFS=','
                    for cidr in $ips; do
                        cidr="$(echo "$cidr" | xargs)"
                        [ "$cidr" = "$q" ] && echo "$cur_pub"
                    done
                    ;;
            esac
        done
    }

    local default_pub="$(get_default_pub)"
    local current_pub="$(get_current_pub)"
    local old_p2p_status="$(uci -q get mptun.IPRB.is_p2p)"
    local current_p2p_status="${old_p2p_status}"

    [ -z "${old_p2p_status}" ] && old_p2p_status='0'

    if [ -n "$default_pub" -a -n "$current_pub" -a "$default_pub" != "$current_pub" ] ;then
        if [ "$peerpubkey" = "$current_pub" ] ;then
            if [ "${ACTION}" = "KEYPAIR-CREATED" ] ;then
                current_p2p_status='1'
            else
                current_p2p_status='0'
            fi
        fi
    else
        current_p2p_status='0'
    fi

    [ "${old_p2p_status}" != "${current_p2p_status}" ] && {
        uci -q set mptun.IPRB.is_p2p="${current_p2p_status}"
        uci commit mptun
        notify_IPRB_status
    }
}

[ "${IPRB_identity}" = "secondary" ] && IPRB_update_p2p_status

if [ "${ACTION}" = "KEYPAIR-CREATED" -a -n "$(echo "${ifname}" | grep "^mptun")" ]; then
    # lock
    count=1
    while [ -f "${LOCKFILE}" ] && [ $count -le 3 ]; do
        sleep 1
        count=$(($count+1))
    done
    rm -f "$LOCKFILE"
    echo $$ > ${LOCKFILE}
    trap 'rm -f "$LOCKFILE"; exit' EXIT INT TERM

    active_num="$(ls /tmp/wireguard/"${ifname}"/peers | wc -l)"
    [ -f /tmp/wireguard/"${ifname}"/peers/"${peerpubkey_safe}" ] || touch /tmp/wireguard/"${ifname}"/peers/"${peerpubkey_safe}"
    # unlock
    rm -f $LOCKFILE

    [ "${active_num}" -ne 0 -a "$(uci -q get mptun.IPRB.state)" = "connected" ] && return

    con_time="$(date +%s)"
    uci -q set mptun.IPRB.state="connected"
    uci -q set mptun.IPRB.lastConnectedTime="$con_time"
    uci commit mptun
    notify_IPRB_status
fi

if [ "${ACTION}" = "REKEY-TIMEOUT"  -a -n "$(echo "${ifname}" | grep "^mptun")" ]; then
     # lock
    count=1
    while [ -f "${LOCKFILE}" ] && [ $count -le 3 ]; do
        sleep 1
        count=$(($count+1))
    done
    rm -f "$LOCKFILE"
    echo $$ > ${LOCKFILE}
    trap 'rm -f "$LOCKFILE"; exit' EXIT INT TERM

    rm -f /tmp/wireguard/"${ifname}"/peers/"${peerpubkey_safe}"
    active_num="$(ls /tmp/wireguard/"${ifname}"/peers | wc -l)"

    # unlock
    rm -f $LOCKFILE

    [ "${active_num}" -ne 0 ] && return

    uci -q set mptun.IPRB.state="connecting"
    uci -q delete mptun.IPRB.lastConnectedTime
    uci commit mptun
    notify_IPRB_status

    # Try switching ports to reconnect and avoid disconnections caused by network changes.
    $WG_BIN set "${ifname}" listen-port 0
fi



