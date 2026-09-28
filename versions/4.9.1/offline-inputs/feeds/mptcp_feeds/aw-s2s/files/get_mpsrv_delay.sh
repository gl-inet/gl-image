#!/bin/sh

. /usr/share/libubox/jshn.sh

INPUT_FILE=/tmp/astrowarp_server_list
OUTPUT_FILE=/etc/mptun/server_delay
TMP_FILE=/tmp/astrowarp_server_delay
LOOP_CNT=""
TAG=mp_delay
TIMEOUT=20

ip_is_valid()
{
    local ret=-1

    if [ -n "$(echo $1|grep -E "^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$")" ]
    then
        local p1=$(echo $1|awk -F'.' '{print $1}')
        local p2=$(echo $1|awk -F'.' '{print $2}')
        local p3=$(echo $1|awk -F'.' '{print $3}')
        local p4=$(echo $1|awk -F'.' '{print $4}')
        [ $p1 -le 255 -a $p2 -le 255 -a $p3 -le 255 -a $p4 -le 255 ] && ret=0
    fi

    echo $ret
}

add_array_member()
{
    json_add_object
    json_add_string "regionCode" "$1"
    json_add_string "latencyTime" "$2"
    json_close_object
}

build_json_msg()
{
    json_init
    json_add_array 'data'
    for i in $(seq 1 $1); do
        local regioncode=$(cat $TMP_FILE|sed -n ''$i'p'|awk -F':' '{print $1}')
        local time=$(cat $TMP_FILE|sed -n ''$i'p'|awk -F':' '{print $2}')
        [ -n "$regioncode" -a -n "$time" ] && add_array_member "$regioncode" "$time"
    done
    json_close_array
    json_dump > $OUTPUT_FILE
    rm $INPUT_FILE $TMP_FILE
}

get_ping_ret()
{
    local ret

    [ $(ip_is_valid $2) -ne 0 ] && {
        logger -s -t $TAG "$2 is not a correct ip address"
        echo "$1:-1" >> $TMP_FILE
        return
    }

    ret=$(ping $2 -A -c3 -W2|tail -n 1|awk -F'=' '{print $2}'|awk -F'/' '{print $2}')
    [ -z "$ret" ] && ret=-1

    echo "$1:$ret" >> $TMP_FILE
}

main()
{
    local start_time=$(date +'%s')
    local flag=0
    local ret_count=0

    echo -n "" > $TMP_FILE

    [ -e $INPUT_FILE ] || return
    LOOP_CNT=$(cat $INPUT_FILE|grep -v ^$| wc -l)

    for i in $(seq 1 $LOOP_CNT); do
        local regioncode=$(cat $INPUT_FILE|sed -n ''$i'p'|awk -F',' '{print $1}')
        local ip=$(cat $INPUT_FILE|sed -n ''$i'p'|awk -F',' '{print $2}')

        if [ -n "$regioncode" -a -n "$ip" ]; then
            [ $flag -eq 0 ] && flag=1
            get_ping_ret $regioncode $ip $i &
        fi
    done

    while [ $(cat $TMP_FILE|wc -l) -lt $LOOP_CNT -a $(date +'%s') -lt $(expr $start_time + $TIMEOUT) ]
    do
        :
    done

    ret_count=$(cat $TMP_FILE|wc -l)
    [ $flag -eq 1 ] && build_json_msg $ret_count
}

main
