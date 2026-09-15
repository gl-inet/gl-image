#!/bin/sh

# shellcheck disable=SC2068,SC3043

. /usr/share/libubox/jshn.sh
. /lib/functions.sh
. /lib/functions/parental_control.sh

INTERVAL=60
WEEK_CUR=1
TIME_CUR=0
SCHEDULE_STATUS_DIR="/var/run/pc_schedule"

debug_print()
{
    [ "$DEBUG" = "1" ] || return 0
    logger -t 'parental_control' $@
}

get_current_time()
{
    local d="$(date '+%w %H%M%S')"
    WEEK_CUR="$(echo $d|cut -d ' ' -f 1)"
    TIME_CUR="$(echo $d|cut -d ' ' -f 2)"
    if [ "$WEEK_CUR" = "0" ]; then
        WEEK_CUR="7"
    fi
    debug_print "current time is week=$WEEK_CUR time=$TIME_CUR"
}

time_in_range()
{
    local week=$1
    local begin=$2
    local end=$3

    if [ "$begin" = "23:59" ]; then
        begin="23:59:59"
    else
        begin="${begin}:00"
    fi
    if [ "$end" = "23:59" ]; then
        end="23:59:59"
    else
        end="${end}:00"
    fi

    local begin_num="$(echo $begin | awk -F: '{printf $1$2$3}')"
    local end_num="$(echo $end | awk -F: '{printf $1$2$3}')"

    local is_everyday=0
    if [ "$week" -eq "0" ]; then
        is_everyday=1
    fi

    local begin_week="$week"
    local end_week="$week"

    if [ "$end_num" -le "$begin_num" ]; then
        end_week=$((($end_week % 7) + 1))
    fi

    local week_match_start=0
    local week_match_end=0
    
    if [ "$is_everyday" -eq 1 ]; then
        week_match_start=1
        week_match_end=1
    else
        [ "$begin_week" -eq "$WEEK_CUR" ] && week_match_start=1
        [ "$end_week" -eq "$WEEK_CUR" ] && week_match_end=1
    fi

    [ "$week_match_start" -eq 0 -a "$week_match_end" -eq 0 ] && return 1

    if [ "$end_num" -le "$begin_num" ]; then
        if [ "$week_match_start" -eq 1 -a "$TIME_CUR" -ge "$begin_num" ]; then
            return 0
        elif [ "$week_match_end" -eq 1 -a "$TIME_CUR" -lt "$end_num" ]; then
            return 0
        fi
    else
        if [ "$week_match_start" -eq 1 -a "$TIME_CUR" -ge "$begin_num" -a "$TIME_CUR" -lt "$end_num" ]; then
            return 0
        fi
    fi

    return 1
}

get_current_status()
{
    local id rule
    while read -r line; do 
        id="$(echo $line|awk -F ' ' '{print $1}')"
        rule="$(echo $line|awk -F ' ' '{print $2}')"
        [ $id = "ID" -a $rule = 'Rule_ID' ] && continue
        [ -n $id ] && [ -n $rule ] && eval ${id}_status=$rule
    done < /proc/parental-control/group
}

group_default_rule()
{
    group_default_cb(){
        local id=$1
        local rule 
        config_get rule "$id" "default_rule"
        [ -n "$rule" ] && {
            eval ${id}_rule=\$rule
        }
    }
    config_foreach group_default_cb group
}

is_rule_change()
{
    local group=$1
    local new=$2
    local old

    eval old=\$${group}_status
    [ $old = $new ] && return 1
    return 0
}

write_schedule_status()
{
    local id=$1
    local schedule
    eval shcedule=\$${id}_schedule
    if [ -n "$shcedule" ];then
        echo "$shcedule" > ${SCHEDULE_STATUS_DIR}/$id
        eval ${id}_schedule=""
    else
        [ -f "${SCHEDULE_STATUS_DIR}/$id" ] && rm -f ${SCHEDULE_STATUS_DIR}/$id 2>/dev/null
    fi
}

do_set_groups_rule()
{
    local change=0
    json_init
    json_add_int "op" $SET_GROUP
    json_add_object "data"
    json_add_array "groups"

    set_groups_cb(){
        local id=$1
        local rule macs enabled schedules_enabled
        config_get enabled "$1" "enabled"
        [ "$enabled" != "1" ] && return
        config_get schedules_enabled "$1" "schedules_enabled"
        [ "$schedules_enabled" != "1" ] && return
        eval rule=\$${id}_rule
        write_schedule_status $id
        is_rule_change $id $rule || return 0
        change=1
        debug_print "rule change,group $id use rule $rule"
        config_get macs "$id" "macs"
        json_add_object ""
        json_add_string "id" "$id"
        json_add_string "rule" $rule
        [ -n "$macs" ] && {
            json_add_array "macs"
            for mac in $macs;do
                json_add_string "" $mac
            done
            json_select ..
        }
        json_select ..
    }
    config_foreach set_groups_cb group

    [ "$change" -eq 1 ] && {
        json_str=`json_dump`
        config_apply "$json_str"
        json_cleanup
        clean_client_conntrack
    }
}

schedule_for_each()
{
    load_schedule_cb(){
        local config=$1
        local week begin end rule group enabled
        config_get enabled "$1" "enabled"
        [ "$enabled" != "1" ] && return
        config_get week "$config" "week"
        config_get begin "$config" "begin"
        config_get end "$config" "end"
        config_get group "$config" "group"
        
        time_in_range "$week" "$begin" "$end" && {
            config_get group "$config" "group"
            config_get rule "$config" "rule"
            eval ${group}_rule=\$rule
            eval ${group}_schedule=\$config
            debug_print "time is in range $week from $begin to $end"
        }
    }
    group_default_rule
    config_foreach  load_schedule_cb schedule
}

init_parental_control()
{
    mkdir -p $SCHEDULE_STATUS_DIR
    rm -f $SCHEDULE_STATUS_DIR/*
    load_base_config
    clean_group
    clean_rule
    load_rule
    load_group
}

check_ntp_valid()
{
    while [ ! -f "/var/state/dnsmasqsec" ];do
        logger -t 'parental_control' "ntpd say time is invalid, sleep 5s"
        sleep 5
    done
}

#check_ntp_valid
config_load parental_control_v2
init_parental_control
clean_client_conntrack
while true;do
    get_current_status
    get_current_time
    schedule_for_each
    do_set_groups_rule
    sleep $INTERVAL
done

exit 0

