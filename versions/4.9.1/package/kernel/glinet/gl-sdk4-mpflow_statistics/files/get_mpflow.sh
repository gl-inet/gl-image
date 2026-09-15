#!/bin/sh
. /usr/share/libubox/jshn.sh

RX_FILE=/proc/gl-mpflow/active_rx
TX_FILE=/proc/gl-mpflow/active_tx
DRX_FILE=/proc/gl-mpflow/deading_rx
DTX_FILE=/proc/gl-mpflow/deading_tx
DATA_FILE=/tmp/mpflow.json

STAT_TIME=2
INIT_FLAG=""
[ -e $RX_FILE ] || return
now_min=$(cat $RX_FILE|sed -n '1p' |cut -d' ' -f1)

handle_data()
{
    local rxdev txdev iface cnt

    rxdev=$(echo $1|cut -d' ' -f1)
    txdev=$(echo $1|cut -d' ' -f1)
    [ "$rxdev" = "$txdev" ] || return

    iface=$(uci show mptun|grep $rxdev|cut -d'.' -f2)
    [ -n "$iface" ] || return

    [ $# -eq 4 ] && {
        json_init
        json_add_object "path"
    }
    json_add_object $iface

    cnt=$3
    json_add_array 'rx'
    for i in $(seq 2 $cnt); do
        local rx_data=$(echo $1|cut -d' ' -f$i)
        json_add_int $i $rx_data
    done
    json_close_array

    json_add_array 'tx'
    for i in $(seq 2 $cnt); do
        local tx_data=$(echo $2|cut -d' ' -f$i)
        json_add_int $i $tx_data
    done
    json_close_array
    json_close_object
}

handle_proc()
{
    local loop_cnt cnt
    let loop_cnt=now_min+2

    cnt=$(cat $1|sed -n '1p'|wc -w)

    for i in $(seq 2 $cnt); do
        local rx_data=$(cat $1|awk '{print $'$i'}')
        local tx_data=$(cat $2|awk '{print $'$i'}')
        [ -z "$rx_data" -o -z "$tx_data" ] && continue
        if [ -z "$INIT_FLAG" ]; then
            INIT_FLAG=1
            handle_data "$rx_data" "$tx_data" "$loop_cnt" $INIT_FLAG
        else
            handle_data "$rx_data" "$tx_data" "$loop_cnt"
        fi
    done
}

handle_proc $RX_FILE $TX_FILE
handle_proc $DRX_FILE $DTX_FILE

[ -z "$INIT_FLAG" ] && return

json_close_object
flow_data=$(json_dump)

echo $flow_data > $DATA_FILE
