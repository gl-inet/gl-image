. /lib/functions/vpn_func/fw4_func.sh

TYPE="EDGE"
DEBUG_SERVER="$(cat /etc/mptun/debugip 2>/dev/null)"
listenPort=""
AW_P2P_PORT='58080'

AW_DNS_PORT='1553'
DNS_MARK=$(uci -q get mptun.global.dns_mark || echo 0x8)

AW_TMP_DIR="/tmp/run/aw_tmp_dir"
mkdir -p ${AW_TMP_DIR}

use_fw4='0'
[ -n "$(which fw4)" ] && use_fw4='1'

service_mode="$(uci -q get mptun.global.service_mode)"
[ "${service_mode}" = '' ] && service_mode='classic'
IPRB_identity="$(uci -q get mptun.IPRB.identity)"

notify_IPRB_status() {
    $(curl -H 'glinet: 1' -s -k http://127.0.0.1/rpc -d '{"jsonrpc":"2.0","id":15,"method":"call","params":["","mptun","push_IPRB_status_to_wb",{}]}')
}

gateway_set_tcptun() {
    local port key
    local cfg="$1"
    port="$(jsonfilter -i $cfg -q -e  '@.tcpTun.port')"
    key="$(jsonfilter -i $cfg -q -e  '@.tcpTun.users[0].key')"
    mac="$(jsonfilter -i $cfg -q -e  '@.tcpTun.users[0].mac')"
    [ -z "$port" -o -z "$key" ] && return 1
cat >/var/run/mpnode.json <<EOF
{
  "inbounds": [
    {
      "tag": "mptunin",
      "listen": "0.0.0.0",
      "port": $port,
      "protocol": "vmess",
      "settings": {
        "clients": [
            {
            "level": 0,
            ${mac:+\"email\": \"$mac\",} 
            "id": "$key"
            }
EOF
    local i=0
    while true;do
    let i=i+1
    mac=$(jsonfilter -i $cfg -q -e  "@.tcpTun.users[$i].mac")
    key=$(jsonfilter -i $cfg -q -e  "@.tcpTun.users[$i].key")
    [ -z "$key" ] && break
cat >>/var/run/mpnode.json <<EOF
            ,
            {
            "level": 0,
            ${mac:+\"email\": \"$mac\",} 
            "id": "$key"
            }    
EOF
    done
cat >>/var/run/mpnode.json <<EOF
        ]
      }
    }
  ]
}
EOF
    return 0
}

set_tcptun() {
    local port host key
    local cfg="$1"
    host="$(jsonfilter -i $cfg -q -e  '@.tcpTun.address')"
    port="$(jsonfilter -i $cfg -q -e  '@.tcpTun.port')"
    key="$(jsonfilter -i $cfg -q -e  '@.tcpTun.users[0].key')"
    [ -z "$host" -o -z "$port" -o -z "$key" ] && return 1
    [ -n "$DEBUG_SERVER" ] && host="$DEBUG_SERVER"
cat >/var/run/mpnode.json <<EOF
{
  "outbounds": [
    {
      "tag": "mptunout",
      "settings": {
        "vnext": [
          {
            "address": "$host",
            "port": $port,
            "users": [
              {
                "id": "$key"
              }
            ]
          }
        ]
      }
    }
  ]
}
EOF
    return 0
}



format_udp_cfg() {
    local FILE="$2"
    local proto_type="$3"
    local interface="$(jsonfilter -q -i $1 -e  '@.udpTun.interface')"
    local peers="$(jsonfilter -q -i $1 -e  '@.udpTun.peers')"
    [ -z "$interface" -o -z "$peers" ] && return 1

    local awg_params="$(jsonfilter -q -i $1 -e '@.udpTun.awgParams' | tr -d '{}"' | tr ',' '\n' | sed 's/:/ = /')"

    eval $(echo $interface|jsonfilter -q -e 'address=@.address' -e 'privateKey=@.privateKey')
    eval $(echo $interface|jsonfilter -q -e 'listenPort=@.listenPort')
cat >$FILE <<EOF
[Interface]
privateKey=$privateKey
${listenPort:+listenPort=$listenPort}
EOF

    [ "${proto_type}" = 'amneziawg' ] && echo "${awg_params:+$awg_params}" >>$FILE

    local i=0
    while true;do
        local peer=$(echo $peers|jsonfilter -q -e "@[$i]")

        [ -z "$peer" ] &&  break
        let i=i+1;
        eval $(echo $peer|jsonfilter -q -e 'endPoint=@.endPoint' -e 'allowedIps=@.allowedIps' \
         -e 'persistentKeepalive=@.persistentKeepalive' -e 'publicKey=@.publicKey')
        [ -n "$DEBUG_SERVER" ] && endPoint="$DEBUG_SERVER":${endPoint##*:}
cat >>$FILE <<EOF

[peer]
${endPoint:+endPoint=$endPoint}
persistentKeepalive=${persistentKeepalive:=25}
publicKey=$publicKey
${allowedIps:+allowedIps=$allowedIps}
EOF
    done
    return 0

}

set_udptun() {
    local WG_TOOL='/usr/bin/wg'
    local proto_type='wireguard'
    local TMPFILE="/var/run/mpudp.cfg"
    local MTU="$(jsonfilter -q -i $1 -e  '@.udpTun.interface.mtu')"
    local Address="$(jsonfilter -q -i $1 -e  '@.udpTun.interface.address')"
    local DEV=$2
    local MARK=$3

    case "${service_mode}" in
        "IPRB")
                WG_TOOL="/usr/bin/awg"
                proto_type="amneziawg"
            ;;
        *)
            WG_TOOL="/usr/bin/wg"
            proto_type="wireguard"
        ;;
    esac

    format_udp_cfg "$1" "${TMPFILE}" "${proto_type}" || return 1
    ip link del dev "${DEV}" 2>/dev/null
    ip link add dev "${DEV}" type "$proto_type"
    [ -n "$MTU" ] && ip link set mtu "$MTU" "${DEV}" 
    $WG_TOOL setconf "${DEV}" "${TMPFILE}" || return 1
    [ "${service_mode}" = "IPRB" ] && {
        uci set mptun.IPRB.state="connecting"
        uci -q delete mptun.IPRB.lastConnectedTime
        uci commit mptun
        notify_IPRB_status
    }
    ip address add dev "${DEV}" "${Address}" || return 1
    ip link set up dev "${DEV}" || return 1

    [ -z "$(uci -q get network.mpudptun_rule)" ] && {
        uci set network.mpudptun_rule=rule
        uci set network.mpudptun_rule.mark="$MARK/$MARK"
        uci set network.mpudptun_rule.lookup='30'
        uci set network.mpudptun_rule.priority='30'
        uci commit network
        /etc/init.d/network reload
    }

}

FW4_FILE_MPTUN_POST_RULESET='/usr/share/nftables.d/ruleset-post/aw_set_fw4.nft'
FILE_AW_DNS_RULE="${AW_TMP_DIR}/aw_dns_rule"
set_firewall()
{
    local cfg="$1"
    local DEV="$2"
    local srcips destips localips exitnode type enable_p2p
    local use_exit=$(uci -q get mptun.global.use_exit)
    local i=0

    local if_ip_addr_list=$(jsonfilter -i $cfg -q -e "@.udpTun.interface.address")
    if_ip_addr_list="$(echo "$if_ip_addr_list" | sed 's/[，,]/ /g; s/[[:space:]]\+/ /g')"
    local v4_addr_list="$(echo "$if_ip_addr_list" | tr ' ' '\n' | sed 's#/.*##' | grep -E '^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$')"

    localips=$(jsonfilter -i $cfg -q -e "@.udpTun.interface.allowedSrc")
    exitnode=$(jsonfilter -i $cfg -q -e "@.udpTun.interface.exitNode")
    type=$(jsonfilter -i $cfg -q -e "@.type")
    enable_p2p=$(jsonfilter -i $cfg -q -e "@.enableP2p")

    while true;do
            local client=$(jsonfilter -i $cfg -q -e "@.udpTun.clients[$i]")
            [ -z "$client" ] &&  break
            local resource=$(jsonfilter -i $cfg -q -e "@.udpTun.clients[$i].resource")
            local allowedsrc=$(jsonfilter -i $cfg -q -e "@.udpTun.clients[$i].allowedSrc")
            srcips=${srcips}${allowedsrc:+,$allowedsrc}
            destips=${destips}${resource:+,$resource}
            let i=i+1;
    done

    localips="$(echo "$localips"|sed 's/,/ /g')"
    srcips="$(echo "$srcips"|sed 's/,/ /g')"
    destips="$(echo "$destips"|sed 's/,/ /g')"

    if [ "$use_fw4" = '1' ] ;then
        echo '' >"$FW4_FILE_MPTUN_POST_RULESET"
    fi

    uci set firewall.mptun=zone
    uci set firewall.mptun.name='mptun'
    uci set firewall.mptun.device="${DEV:=mptun0}"
    uci set firewall.mptun.input='DROP'
    uci set firewall.mptun.output='ACCEPT'
    uci set firewall.mptun.forward='DROP'
    uci set firewall.mptun.masq='1'
    uci set firewall.mptun.mtu_fix='1'
    uci set firewall.mptun.custom_chains='0'
    uci set firewall.lan2mptun=forwarding
    uci set firewall.lan2mptun.src='lan'
    uci set firewall.lan2mptun.dest='mptun'

    uci set firewall.guest2mptun=forwarding
    uci set firewall.guest2mptun.src='guest'
    uci set firewall.guest2mptun.dest='mptun'

    if [ "$use_fw4" = '1' ] ;then
        echo "insert rule inet fw4 forward iifname \"br-*\" oifname \"${DEV:=mptun0}\" counter accept" >>"$FW4_FILE_MPTUN_POST_RULESET"
    else
        uci set firewall.br2mptun=rule
        uci set firewall.br2mptun.name='accept br-+ to mptun'
        uci set firewall.br2mptun.src='*'
        uci set firewall.br2mptun.dest='mptun'
        uci set firewall.br2mptun.proto='any'
        uci set firewall.br2mptun.extra="-i br-+"
        uci set firewall.br2mptun.target="ACCEPT"
    fi

    # IPRB mode, as the primary, allows DNS.
    if [ "${service_mode}" = 'IPRB' -a "${IPRB_identity}" = 'primary' ] ;then
        uci set firewall.mptun_allowe_dns=rule
        uci set firewall.mptun_allowe_dns.name='mptun allowe dns'
        uci set firewall.mptun_allowe_dns.src="mptun"
        uci set firewall.mptun_allowe_dns.proto='tcp udp'
        uci set firewall.mptun_allowe_dns.dest_port='53'
        uci set firewall.mptun_allowe_dns.target='ACCEPT'
    fi

    if [ "${service_mode}" = 'IPRB' -a "${enable_p2p}" = 'true' ] ;then
        uci set firewall.mptun_allowe_aw_p2p=rule
        uci set firewall.mptun_allowe_aw_p2p.name='mptun allowe aw p2p'
        uci set firewall.mptun_allowe_aw_p2p.src="mptun"
        uci set firewall.mptun_allowe_aw_p2p.proto='tcp udp'
        uci set firewall.mptun_allowe_aw_p2p.dest_port="${AW_P2P_PORT}"
        uci set firewall.mptun_allowe_aw_p2p.target='ACCEPT'
    fi

    uci set firewall.mptun_local_rule=rule
    uci set firewall.mptun_local_rule.name='mptun local rule'
    uci set firewall.mptun_local_rule.src="mptun"
    uci -q delete firewall.mptun_local_rule.proto
    uci add_list firewall.mptun_local_rule.proto='all'

    # Forwarding is not allowed by default in IPRB mode.
    if [ "${service_mode}" != 'IPRB' ] ;then
        uci set firewall.mptun_forward_rule=rule
        uci set firewall.mptun_forward_rule.name='mptun forward rule'
        uci set firewall.mptun_forward_rule.src="mptun"
        uci set firewall.mptun_forward_rule.dest="*"
        uci -q delete firewall.mptun_forward_rule.proto
        uci add_list firewall.mptun_forward_rule.proto="all"
        uci set firewall.mptun_forward_rule.target="ACCEPT"
    fi

    # DNAT does not exist in IPRB mode.
    if [ "${service_mode}" != 'IPRB' ] ;then
        local rule_target="accept"
        if [ "${use_fw4}" = '1' ]; then
            [ -z "$srcips" -o -z "$destips" ] && rule_target="drop"
        else
            uci set firewall.mptun_subnet_rule=rule
            uci set firewall.mptun_subnet_rule.name='mptun subnet rule'
            uci set firewall.mptun_subnet_rule.src="mptun"
            uci set firewall.mptun_subnet_rule.dest='*'
            uci -q delete firewall.mptun_subnet_rule.proto
            uci add_list firewall.mptun_subnet_rule.proto='all'
            uci -q delete firewall.mptun_subnet_rule.src_ip
            for i in $srcips;do
                uci add_list firewall.mptun_subnet_rule.src_ip="$i"
            done
            #uci -q delete firewall.mptun_subnet_rule.dest_ip
            uci -q delete firewall.mptun_subnet_rule.extra
            for i in $destips;do
                uci set firewall.mptun_subnet_rule.extra='-i mptun+ -m conntrack --ctstate DNAT'
                #uci add_list firewall.mptun_subnet_rule.dest_ip="$i"
            done
            if [ -z "$srcips" -o -z "$destips" ];then
                uci set firewall.mptun_subnet_rule.target='DROP'
            else
                uci set firewall.mptun_subnet_rule.target='ACCEPT'
            fi
        fi
    fi

    uci -q delete firewall.mptun_local_rule.src_ip
    for i in $localips;do
        uci add_list firewall.mptun_local_rule.src_ip="$i"
    done
    if [ -z "$localips" ];then
        uci set firewall.mptun_local_rule.target='DROP'
    else
        uci set firewall.mptun_local_rule.target='ACCEPT'
        # IPRB mode, just allowes icmp.
        if [ "${service_mode}" = 'IPRB' ] ;then
            uci -q delete firewall.mptun_local_rule.proto
            uci add_list firewall.mptun_local_rule.proto='icmp'
            for addr in "$v4_addr_list" ;do
                uci add_list firewall.mptun_local_rule.dest_ip="$addr"
            done
        fi
    fi

     [ "$use_exit" = "1" ] && {
        if [ "${use_fw4}" = '1' ] ;then
            echo "chain=\"aw_redirect\";option=\"meta l4proto tcp meta mark and $DNS_MARK == $DNS_MARK\";target=\"redirect to :$AW_DNS_PORT\"" >> "${FILE_AW_DNS_RULE}"
            echo "chain=\"aw_redirect\";option=\"meta l4proto udp meta mark and $DNS_MARK == $DNS_MARK\";target=\"redirect to :$AW_DNS_PORT\"" >> "${FILE_AW_DNS_RULE}"
            echo "chain=\"dns_accept\";option=\"tcp dport $AW_DNS_PORT meta mark and $DNS_MARK == $DNS_MARK\";target=\"accept\"" >> "${FILE_AW_DNS_RULE}"
            echo "chain=\"dns_accept\";option=\"udp dport $AW_DNS_PORT meta mark and $DNS_MARK == $DNS_MARK\";target=\"accept\"" >> "${FILE_AW_DNS_RULE}"
        else
            echo "chain=\"aw_redirect\";option=\"-p tcp -m mark --mark $DNS_MARK/$DNS_MARK\";target=\"-j REDIRECT --to-ports $AW_DNS_PORT\"" >> "${FILE_AW_DNS_RULE}"
            echo "chain=\"aw_redirect\";option=\"-p udp -m mark --mark $DNS_MARK/$DNS_MARK\";target=\"-j REDIRECT --to-ports $AW_DNS_PORT\"" >> "${FILE_AW_DNS_RULE}"
        fi
     }

    [ "$TYPE" = "GATEWAY" -a -n "$listenPort" ] && {
        uci set firewall.mptun_allow_listen=rule
        uci set firewall.mptun_allow_listen.name='mptun allow listen'
        uci set firewall.mptun_allow_listen.src="wan"
        uci -q delete firewall.mptun_allow_listen.proto
        uci add_list firewall.mptun_allow_listen.proto='tcp'
        uci add_list firewall.mptun_allow_listen.proto='udp'
        uci set firewall.mptun_allow_listen.dest_port="$listenPort"
        uci set firewall.mptun_allow_listen.target='ACCEPT'

        uci set firewall.mptun2mptun=forwarding
        uci set firewall.mptun2mptun.src='mptun'
        uci set firewall.mptun2mptun.dest='mptun'
    }

    [ "$exitnode" = "true" -o "$type" = "EXIT" ] && {
        uci set firewall.mptun2wan=forwarding
        uci set firewall.mptun2wan.src='mptun'
        uci set firewall.mptun2wan.dest='wan'
    }

    [ "$type" = "GATEWAY" ] && {
        ip rule del from all iif mptun0 lookup mptcp_mptun0 2>/dev/null
        ip rule add from all iif mptun0 lookup mptcp_mptun0
    }
    uci commit firewall
    /etc/init.d/firewall reload 2>/dev/null

    # DNAT does not exist in IPRB mode.
    if [ "${service_mode}" != 'IPRB' ] ;then
        [ "${use_fw4}" = '1' ] && echo "insert rule inet fw4 forward_mptun iifname "mptun0" ct status dnat counter $rule_target" >>"$FW4_FILE_MPTUN_POST_RULESET"
    fi
}

set_dns() {
    local dns=$(uci -q get mptun.global.dns)
    local use_exit=$(uci -q get mptun.global.use_exit)
    [ -z "$dns" ] && {
        local cfg=$(uci -q get mptun.config.profile)
        if [ -n "$cfg" -a -e "$cfg" ]; then
            dns=$(jsonfilter -i $cfg -q -e "@.dns")
        fi
    }
    [ -z "$dns" ] && return
    [ ! "$use_exit" = "1" ] && return
    mkdir -p /tmp/dnsmasq.d.mptun
    [ -f /var/run/dnsmasq/dnsmasq.mptun.pid ] && kill $(cat /var/run/dnsmasq/dnsmasq.mptun.pid) 2>/dev/null
    [ -f /tmp/dnsmasq.d/mpaccess_conf ] && ln -sf /tmp/dnsmasq.d/mpaccess_conf /tmp/dnsmasq.d.mptun/mpaccess_conf
    [ ! -e /tmp/dnsmasq.d.mptun/mpaccess -a -d /tmp/dnsmasq.d/mpaccess ] && ln -s /tmp/dnsmasq.d/mpaccess /tmp/dnsmasq.d.mptun/mpaccess
    if [ "${use_fw4}" != '1' ]; then
        [ -e /tmp/dnsmasq.d.mptun/mptun_policy -a ! -e /tmp/dnsmasq.d/mptun_policy ] && ln -s /tmp/dnsmasq.d.mptun/mptun_policy /tmp/dnsmasq.d/
    fi
    /usr/sbin/dnsmasq -C /etc/dnsmasq.conf.mptun -x /var/run/dnsmasq/dnsmasq.mptun.pid --server=$dns --no-resolv
}

remove_dns() {
    [ -e /var/run/dnsmasq/dnsmasq.mptun.pid ] && kill $(cat /var/run/dnsmasq/dnsmasq.mptun.pid) 2>/dev/null
    [ -e /var/run/mptun_dns.json ] && rm /var/run/mptun_dns.json
    [ -e /var/run/mptun_dns_policy.json ] && rm /var/run/mptun_dns_policy.json
    [ -e /tmp/dnsmasq.d/mptun_policy ] && rm /tmp/dnsmasq.d/mptun_policy
    rm -rf /tmp/dnsmasq.d.mptun 2>/dev/null
    [ -e /tmp/dnsmasq.d/mptun_policy ] && rm /tmp/dnsmasq.d/mptun_policy
}

set_ip_policy(){
    local list="$1"
    local i
    for i in $list;do
        ipset add mproxy_skip $i
    done
}

set_domain_policy(){
    local list_j="$1"
    local list="$(echo $list_j|sed 's/\",//g;s/\[//g;s/\]//g;s/\"//g;')"
    local i
    local dir="/tmp/dnsmasq.d.mptun"
    local file="$dir/mptun_policy"
    mkdir -p "$dir"
    echo "#Automatically generated, do not edit" >$file
    for i in $list;do
       echo "server=/$i/127.0.0.1#53" >>$file
       if [ "${use_fw4}" = '1' ];then
           echo "nftset=/$i/#ip#mpnet#mproxy_skip" >>$file
       else
           echo "ipset=/$i/mproxy_skip" >>$file
       fi
    done
}

set_mac_policy(){
    local list="$(echo $1|sed 's/\",//g;s/\[//g;s/\]//g;s/\"//g;')"
    local i
    for i in $list; do
        local mac="$(echo $i|sed 's/\(..\)/\1:/g; s/:$//')"
        [ -n "$mac" ] && ipset add mproxy_mac_skip $mac
    done
}
set_policy(){
    local file="/etc/mptun/policy.json"
    local ips="$(jsonfilter -i $file  -e '@.alwaysUseLocalExitIpList'|sed 's/\",//g;s/\[//g;s/\]//g;s/\"//g;')"
    local domains="$(jsonfilter -i $file  -e '@.alwaysUseLocalExitDomainList')"
    local macs="$(jsonfilter -i $file  -e '@.policyMac')"
    [ -n "$ips" ] && set_ip_policy "$ips"
    [ -n "$domains" ] && set_domain_policy "$domains"
    [ -n "$macs" ] && set_mac_policy "$macs"
}

gen_nft_script() {
    local server="$1"
    local tcp_route_ips="$2"
    local udp_route_ips="$3"
    local udp_global_proto="$4"
    local tcptun_is_exist="$5"
    local tcpmark="$6"
    local udpmark="$7"
    local exit_is_dev=""
    local TCP_PROXY_PORT="1080"
    local MPPROTO="tcp,udp"

    #exit node
    local use_exit="$(uci -q get mptun.global.use_exit)"
    [ "$use_exit" = "1" -a -z "$tcptun_is_exist" ] && exit_is_dev=1
    [ -n "$use_exit" -a "$use_exit" = "0" ] && use_exit=""

    local lanmark="$(uci -q get mptun.global.lanmark || echo 0x4)"
    local dns_mark="$(uci -q get mptun.global.dns_mark || echo 0x8)"
    local tcpmark_mask_inv="$(printf '0x%x' $((~tcpmark & 0xffffffff)))"
    local udpmark_mask_inv="$(printf '0x%x' $((~udpmark & 0xffffffff)))"
    local dns_mark_mask_inv="$(printf '0x%x' $((~dns_mark & 0xffffffff)))"
    local mptun_flow_mark="$(printf '0x%x' $((tcpmark | lanmark)))"
    local mptun_flow_mark_mask_inv="$(printf '0x%x' $((~mptun_flow_mark & 0xffffffff)))"

    local item mproxy_set=""
    for item in $(echo "$udp_route_ips" | sed 's/,/ /g'); do
        if [ "$item" = "0.0.0.0/0" -o "$item" = "::/0" ]; then
            logger -t mptun "ignore 0.0.0.0/0 route"
        else
            if [ -z "$mproxy_set" ]; then
                mproxy_set="$item"
            else
                mproxy_set="$mproxy_set,$item"
            fi
        fi
    done

    #Prevent mproxy_set from being empty and causing nft script exception
    [ -z "$mproxy_set" ] && mproxy_set='224.0.0.1'

    local skip_addr='127.0.0.1/32, 224.0.0.0/4, 255.255.255.255/32, 169.254.0.0/16, 192.0.2.0/24, 198.18.0.0/15'
    local policy_ips policy_file="/etc/mptun/policy.json"
    [ -f "$policy_file" ] && {
        policy_ips="$(jsonfilter -q -i "$policy_file" -e '@.alwaysUseLocalExitIpList' | sed 's/ //g;s/\[//g;s/\]//g')"
        for item in "$policy_ips"; do
            skip_addr="$skip_addr,$item"
        done
    }

    local policy_macs="$(jsonfilter -q -i "$policy_file" -e '@.policyMac' | sed 's/",//g;s/\[//g;s/\]//g;s/"//g')"
    local mproxy_mac_set=''
    local mac=''
    for item in $policy_macs; do
        mac="$(echo "$item" | sed 's/\(..\)/\1:/g; s/:$//')"
        [ -z "$mac" ] && continue
        mproxy_mac_set="${mproxy_mac_set}${mproxy_mac_set:+, }${mac}"
    done

    command -v fw4_init_set_context >/dev/null 2>&1 || {
        logger -t mptun "fw4_func.sh not available"
        return 1
    }

    fw4_init_set_context "/var/run/mptun.nft" "ip mpnet" || return 1

    fw4_set_create_member_of_table "set" "mproxy_privateip" "type ipv4_addr; flags interval; elements = { 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16 }"
    fw4_set_create_member_of_table "set" "mproxy_set" "type ipv4_addr; flags interval; elements = { $mproxy_set }"
    fw4_set_create_member_of_table "set" "mproxy_skip" "type ipv4_addr; flags interval; elements = { $skip_addr }"
    fw4_set_create_member_of_table "set" "mproxy_mac_skip" "type ether_addr; flags interval;"
    [ -n "$mproxy_mac_set" ] && fw4_set_add_append_statement "add element $FW4_TABLE_NAME mproxy_mac_skip { $mproxy_mac_set }"


    fw4_set_create_chain "MPROXY_PREROUTING" "type filter hook prerouting priority mangle; policy accept;"
    fw4_set_create_chain "FORWARD" "type filter hook forward priority mangle; policy accept;"
    fw4_set_create_chain "POSTROUTING" "type filter hook postrouting priority srcnat; policy accept;"

    fw4_set_add_append_statement "add rule $FW4_TABLE_NAME MPROXY_PREROUTING iifname != \"br-*\" accept"
    [ -n "$use_exit" ] && {
        fw4_set_add_append_statement "add rule $FW4_TABLE_NAME MPROXY_PREROUTING udp dport 53 ether saddr != @mproxy_mac_skip counter packets 0 bytes 0 mark set mark and $dns_mark_mask_inv or $dns_mark"
        fw4_set_add_append_statement "add rule $FW4_TABLE_NAME MPROXY_PREROUTING tcp dport 53 ether saddr != @mproxy_mac_skip counter packets 0 bytes 0 mark set mark and $dns_mark_mask_inv or $dns_mark"
    }
    fw4_set_add_append_statement "add rule $FW4_TABLE_NAME MPROXY_PREROUTING ip daddr @mproxy_skip accept"
    fw4_set_add_append_statement "add rule $FW4_TABLE_NAME MPROXY_PREROUTING ether saddr @mproxy_mac_skip accept"
    fw4_set_add_append_statement "add rule $FW4_TABLE_NAME MPROXY_PREROUTING ip daddr @mproxy_set mark set mark and $udpmark_mask_inv or $udpmark"
    #出口节点
    [ -n "$use_exit" ] && fw4_set_add_append_statement "add rule $FW4_TABLE_NAME MPROXY_PREROUTING ip daddr @mproxy_privateip accept"

    #如果存在udp聚合peer,则应用这条规则
    [ -n "$udp_global_proto" ] && fw4_set_add_append_statement "add rule $FW4_TABLE_NAME MPROXY_PREROUTING ip protocol icmp mark and $udpmark != $udpmark counter packets 0 bytes 0 mark set mark and $udpmark_mask_inv or $udpmark"
    [ -n "$exit_is_dev" ] && fw4_set_add_append_statement "add rule $FW4_TABLE_NAME MPROXY_PREROUTING mark set mark and $udpmark_mask_inv or $udpmark counter packets 0 bytes 0 "
    [ -n "$tcptun_is_exist" ] && fw4_set_add_append_statement "add rule $FW4_TABLE_NAME MPROXY_PREROUTING ip daddr { $tcp_route_ips } meta l4proto { $MPPROTO } mark set mark and $tcpmark_mask_inv or $tcpmark tproxy ip to 127.0.0.1:$TCP_PROXY_PORT counter packets 0 bytes 0 accept"

    fw4_set_add_append_statement "add rule $FW4_TABLE_NAME FORWARD iifname \"mptun0\" oifname \"mptun0\" mark set mark and $mptun_flow_mark_mask_inv or $mptun_flow_mark counter packets 0 bytes 0"
    fw4_set_add_append_statement "add rule $FW4_TABLE_NAME POSTROUTING mark and $mptun_flow_mark == $mptun_flow_mark counter packets 0 bytes 0 return"


    local domains="$(jsonfilter -i $policy_file -e '@.alwaysUseLocalExitDomainList')"
    [ -n "$domains" ] && set_domain_policy "$domains"
}

set_ipt_script() {
    local servers="$1"
    local tcp_route_ips="$2"
    local udp_route_ips="$3"
    local udp_global_proto="$4"
    local tcptun_is_exist="$5"
    local tcpmark="$6"
    local udpmark="$7"

    local use_exit=$(uci -q get mptun.global.use_exit)

    ipset create mproxy_set hash:net 2>/dev/null
    ipset flush  mproxy_set 2>/dev/null
    for ip in `echo $udp_route_ips|sed 's/,/ /g'`;do
        if [ "$ip" = "0.0.0.0/0" ];then
            logger -t mptun "ignore 0.0.0.0/0 route"
        else
            ipset add mproxy_set $ip
        fi
    done

    local private_ip="192.168.0.0/16 10.0.0.0/8 172.16.0.0/12"
    ipset create mproxy_privateip hash:net 2>/dev/null
    ipset flush  mproxy_privateip 2>/dev/null
    for ip in `echo $private_ip|sed 's/,/ /g'`;do
        ipset add mproxy_privateip $ip
    done

    #local skip_ip="127.0.0.1/32 224.0.0.0/4 255.255.255.255/32 169.254.0.0/16 192.0.2.0/24 198.18.0.0/15 $servers"
    local skip_ip="127.0.0.1/32 224.0.0.0/4 255.255.255.255/32 169.254.0.0/16 192.0.2.0/24 198.18.0.0/15"
    ipset create mproxy_skip hash:net 2>/dev/null
    ipset flush  mproxy_skip 2>/dev/null
    for ip in `echo $skip_ip|sed 's/,/ /g'`;do
        ipset add mproxy_skip $ip
    done

    ipset create mproxy_mac_skip hash:mac 2>/dev/null
    ipset flush mproxy_mac_skip 2>/dev/null
 
    local IPTW="iptables -w 5"
    $IPTW -t mangle -N MPROXY_PREROUTING 2>/dev/null
    $IPTW -t mangle -F MPROXY_PREROUTING 2>/dev/null
    $IPTW -t mangle -C PREROUTING -j MPROXY_PREROUTING 2>/dev/null
    [ ! "$?" = "0" ] && $IPTW -t mangle -I PREROUTING -j MPROXY_PREROUTING 2>/dev/null

    $IPTW -t mangle -A MPROXY_PREROUTING ! -i br-+ -j RETURN

    if [ "$use_exit" = "1" ];then
        local dns_mark=$(uci -q get mptun.global.dns_mark || echo 0x8)
        $IPTW -t mangle -A MPROXY_PREROUTING  -p udp --dport 53 -m set ! --match-set mproxy_mac_skip src -j MARK --set-xmark $dns_mark/$dns_mark
        $IPTW -t mangle -A MPROXY_PREROUTING  -p tcp --dport 53 -m set ! --match-set mproxy_mac_skip src -j MARK --set-xmark $dns_mark/$dns_mark
        $IPTW -t mangle -A MPASS -m mark --mark $dns_mark/$dns_mark -j RETURN
    fi

    $IPTW -t mangle -D FORWARD -i mptun0 -o mptun0 -j MARK --set-xmark 0x5/0x5 2>/dev/null
    $IPTW -t mangle -I FORWARD -i mptun0 -o mptun0 -j MARK --set-xmark 0x5/0x5

    $IPTW -t mangle -D FORWARD -i mptun0 -o br-lan -m comment --comment SDWAN_IP_MAPPING_MARK -j MARK --set-xmark 0x4/0x4 2>/dev/null
    $IPTW -t mangle -I FORWARD -i mptun0 -o br-lan -m comment --comment SDWAN_IP_MAPPING_MARK -j MARK --set-xmark 0x4/0x4

    $IPTW -t nat -D POSTROUTING -m mark --mark 0x5 -j RETURN 2>/dev/null
    $IPTW -t nat -I POSTROUTING -m mark --mark 0x5 -j RETURN

    $IPTW -t mangle -A MPROXY_PREROUTING -m set --match-set mproxy_skip dst -j RETURN
    $IPTW -t mangle -A MPROXY_PREROUTING -m set --match-set mproxy_mac_skip src -j RETURN
    $IPTW -t mangle -A MPROXY_PREROUTING -m set --match-set mproxy_set dst -j MARK --set-xmark $udpmark/$udpmark

    if [ "$use_exit" = "1" ];then
        $IPTW -t mangle -A MPROXY_PREROUTING -m set --match-set mproxy_privateip dst  -j RETURN
        $IPTW -t mangle -A MPROXY_PREROUTING -m set --match-set mproxy_privateip dst  -j RETURN
    fi

    [ -n "$udp_global_proto" ] && {
        $IPTW -t mangle -A MPROXY_PREROUTING -p icmp -m mark --mark 0x0/$udpmark -j MARK --set-xmark $udpmark/$udpmark
    }

    [ "$use_exit" = "1" ] && [ -z "$tcptun_is_exist" ] && $IPTW -t mangle -A MPROXY_PREROUTING -j MARK --set-xmark $udpmark/$udpmark

    [ -n "$tcptun_is_exist" ] && {
        $IPTW -t mangle -A MPROXY_PREROUTING -p udp  -j TPROXY --on-port 1080 --tproxy-mark $tcpmark/$tcpmark
        $IPTW -t mangle -A MPROXY_PREROUTING -p tcp  -j TPROXY --on-port 1080 --tproxy-mark $tcpmark/$tcpmark

        $IPTW -t mangle -N MPASS 2>/dev/null
        $IPTW -t mangle -F MPASS 2>/dev/null
        $IPTW -t mangle -I MPROXY_PREROUTING -p tcp -m socket -j MPASS
        $IPTW -t mangle -A MPASS -j CONNMARK --restore-mark --nfmask $tcpmark --ctmask $tcpmark
        $IPTW -t mangle -A MPASS -m mark --mark 0x0/$tcpmark -j MARK --set-mark $tcpmark/$tcpmark
        $IPTW -t mangle -A MPASS -j CONNMARK --save-mark --nfmask $tcpmark --ctmask $tcpmark
        $IPTW -t mangle -A MPASS -j ACCEPT
    }
    set_policy
    set_dns
}

set_proxy() {
    local servers port host tcp_route_ips udp_route_ips
    local cfg="$1"
    local type="$(jsonfilter -i $cfg -q -e  '@.type')"
    host="$(jsonfilter -i $cfg -q -e  '@.tcpTun.address')"
    servers=${servers}${host:+,$host}
    port="$(jsonfilter -i $cfg -q -e  '@.tcpTun.port')"
    tcp_route_ips="0.0.0.0/0"
    [ -e /var/run/mpnode.json ] && tcptun_is_exist=1

    local i=0
    local global_cnt=0
    while true;do
            local peer=$(jsonfilter -i $cfg -q -e "@.udpTun.peers[$i]")
            [ -z "$peer" ] &&  break
            local endpoint=$(jsonfilter -i $cfg -q -e "@.udpTun.peers[$i].endPoint")
            local allowedips=$(jsonfilter -i $cfg -q -e "@.udpTun.peers[$i].allowedIps")
            endpoint=${endpoint%:*}
            [ -n "$DEBUG_SERVER" ] && endpoint="$DEBUG_SERVER"
            servers=${servers}${endpoint:+,$endpoint}
            udp_route_ips=${udp_route_ips}${allowedips:+,$allowedips}

            [ -n "$(echo $allowedips|grep 0.0.0.0/0)" ] && let global_cnt=global_cnt+1
            #如果对端地址与tcp地址相同，则为udp聚合peer,仅对udp和icmp协议做全局代理
            [ -n $host ] && [ "$host" = "$endpoint" ] && [ -n "$(echo $allowedips|grep 0.0.0.0/0)" ] && {
                    udp_global_proto="udp,icmp"
            }         
            let i=i+1;
    done
                         
    servers=${servers#*,}            
    tcp_route_ips=${tcp_route_ips#*,}
    udp_route_ips=${udp_route_ips#*,}
    #如果只有一个global路由，且为聚合的global,全局代理仅对udp和icmp协议生效，从代理路由中剔除0.0.0.0和::0
    [ -n "$udp_global_proto" ] && [ "$global_cnt" = "1" ] && {
        udp_route_ips="$(echo "$udp_route_ips"|sed 's/,0.0.0.0\/0//g'|sed 's/0.0.0.0\/0,//g'|sed 's/0.0.0.0\/0//g')"
        udp_route_ips="$(echo "$udp_route_ips"|sed 's/,::\/0//g'|sed 's/::\/0,//g'|sed 's/::\/0//g')"
    }
 
    if [ "${use_fw4}" = '1' ];then
        gen_nft_script "$servers" "$tcp_route_ips" "$udp_route_ips" "$udp_global_proto" "$tcptun_is_exist" "$2" "$3"
        set_dns
        fw4_set_finish_and_apply
    else
        set_ipt_script "$servers" "$tcp_route_ips" "$udp_route_ips" "$udp_global_proto" "$tcptun_is_exist" "$2" "$3"
    fi
    /usr/bin/proxy-route.sh "set"
}

stop_service_by_gl_session() {
    ubus call gl-session call "$1" >/dev/null
}

stop_conflict_service() {
    local tor_params='{"module":"tor", "func":"set_config", "params":{"enable":false, "manual": false}}'
    local tailscale_params='{"module":"tailscale", "func":"set_config", "params":{"enabled":false}}'
    local zerotier_params='{"module":"zerotier", "func":"set_config", "params":{"enabled":false}}'
    local netnat_params='{"module":"network", "func":"set_netnat_config", "params":{"enable":false}}'

    local tor_enable=$(uci -q get tor.global.enable)
    [ "$tor_enable" = "1" ] && stop_service_by_gl_session "$tor_params"

    local tailscale_enabled=$(uci -q get tailscale.settings.enabled)
    [ "$tailscale_enabled" = "1" ] && stop_service_by_gl_session "$tailscale_params"

    local zerotier_enabled=$(uci -q get zerotier.gl.enabled)
    [ "$zerotier_enabled" = "1" ] && stop_service_by_gl_session "$zerotier_params"

    [ -e /etc/config/ecm ] && {
        local netnat_enabled=$(uci -q get ecm.global.enabled)
        [ "$netnat_enabled" = "1" ] && stop_service_by_gl_session "$netnat_params"
    }

    local config=$(uci -q get network.ovpnclient.config)
    [ -n "$config" ] && {
        local group_id=$(uci get ovpnclient.$config.group_id)
        local client_id=$(uci get ovpnclient.$config.client_id)
        local tap_s2s_params='{"module": "vpn-client", "func":"set_tap_s2s", "params":{"client_id": '$client_id', "group_id": '$group_id', "enabled": false}}'
        [ -n "$group_id" -a -n "$client_id" ] && stop_service_by_gl_session "$tap_s2s_params"
    }

}

update_devinfo() {
    [ -z "$1" -o -n "$(echo $1|grep modem)" ] && return
    local status=$(ubus call network.interface.$1 status 2>/dev/null)
    [ -z "$status" ] && return
    local netdev=$(echo $status|jsonfilter -e '@.l3_device' 2>/dev/null)
    local sdev=$(uci get mptun.$1.device)
    [ -z "$netdev" -o "$netdev" = "$sdev" ] && return
    uci set mptun.$1.device=$netdev
    uci commit mptun
    sync
}

mptun_init() {
    local cfg=$1
    local dev=$2
    local udpmark=$3
    local tcpmark=$4
    TYPE="$(jsonfilter -i $cfg -q -e  '@.type' || echo EDGE)"

    stop_conflict_service

    set_tcptun "$cfg"
    set_udptun "$cfg" "$dev" "$udpmark"
    set_firewall "$cfg" "$dev"
    set_proxy "$cfg" "$tcpmark" "$udpmark"
    sync
    /etc/init.d/firewall reload 2>/dev/null
}

clean_proxy() {
    #rm  /etc/nftables.d/mptcp-proxy.nft 2>/dev/null
    /usr/bin/proxy-route.sh "clean" 2>/dev/null
    if [ "${use_fw4}" = '1' ];then
        nft delete table ip mpnet 2>/dev/null
    else
        local IPTW="iptables -w 5"
        $IPTW -t mangle -D PREROUTING -j MPROXY_PREROUTING 2>/dev/null
        $IPTW -t mangle -F MPROXY_PREROUTING 2>/dev/null
        $IPTW -t mangle -F MPASS 2>/dev/null
        $IPTW -t mangle -X MPROXY_PREROUTING 2>/dev/null
        $IPTW -t mangle -X MPASS 2>/dev/null
        $IPTW -t mangle -D FORWARD -i mptun0 -o br-lan -m comment --comment SDWAN_IP_MAPPING_MARK -j MARK --set-xmark 0x4/0x4 2>/dev/null
        $IPTW -t mangle -D FORWARD -i mptun0 -o mptun0 -j MARK --set-xmark 0x5/0x5 2>/dev/null
        $IPTW -t nat -D POSTROUTING -m mark --mark 0x5 -j RETURN 2>/dev/null
    fi
    ip rule del from all iif mptun0 lookup mptcp_mptun0 2>/dev/null
    [ -e /var/run/mpnode.json ] && rm /var/run/mpnode.json
}

clean_udptun() {
    [ -n "$(uci -q get firewall.mptun)" ] && {
        uci -q del firewall.mptun
        uci -q del firewall.lan2mptun
        uci -q del firewall.mptun_subnet_rule
        uci -q del firewall.mptun_local_rule
        uci -q del firewall.mptun_forward_rule
        uci -q del firewall.mptun_allow_listen
        uci -q del firewall.mptun2mptun
        uci -q del firewall.mptun2wan
        uci -q del firewall.dns_mptun
        uci -q del firewall.dns_mptun_guest

        # clean IPRB mode-specific rules
        uci -q del firewall.guest2mptun
        rm ${FW4_FILE_MPTUN_POST_RULESET} 2>/dev/null
        rm ${FILE_AW_DNS_RULE} 2>/dev/null
        uci -q del firewall.br2mptun

        uci -q del firewall.mptun_allowe_dns
        uci -q del firewall.mptun_allowe_aw_p2p

        uci commit firewall
        /etc/init.d/firewall reload 2>/dev/null
    }

    [ -n "$(uci -q get network.mpudptun_rule)" ] && {
        uci del network.mpudptun_rule
        uci commit network
        /etc/init.d/network reload
    }
    config_load "$CONF"
    local DEV
    config_get DEV "global" "mptun" "mptun0"
    ip link del dev "${DEV}" 2>/dev/null
    [ "${service_mode}" = "IPRB" ] && {
        uci -q set mptun.IPRB.state="disconnected"
        uci commit mptun
        rm -rf /tmp/wireguard/"${DEV}"
        notify_IPRB_status
    }
}

clean_p2p_info(){
    [ -n "$(ps|grep gl-p2p-daemon|grep -v grep)" ] && kill $(pgrep gl-p2p-daemon) 2>/dev/null
}

mptun_clean() {
    clean_proxy
    clean_udptun
    clean_p2p_info
    remove_dns
    sync
}

get_wan_name() {
    # Get the default route line from the main routing table
    default_route_line=$(ip route show table main 2>/dev/null | grep "^default" | grep -v "dead" | head -1)
    if [ -z "$default_route_line" ]; then
        echo ""
        return
    fi
    # Extract the interface name from the default route line
    interface=$(echo "$default_route_line" | sed -n 's/.* dev \([^ ]*\).*/\1/p')
    echo "$interface"
}

get_iface_mask() {
    # Interface name
    local iface="wan"

    # Call ubus to get the interface status
    local iface_status
    iface_status=$(ubus call network.interface.$iface status 2>/dev/null)

    # Check if the interface is up and extract the subnet mask
    if echo "$iface_status" | grep -q '"up": true'; then
        # Extract the IPv4 mask using jq or a manual parsing method
        local mask
        mask=$(echo "$iface_status" | sed -n 's/.*"mask":\s*\([0-9]*\).*/\1/p' | head -n 1)
        echo "$mask"
    else
        echo ""
    fi
}

get_arp_scan_ret(){
    local aw_enable=$(uci get mptun.global.enable)
    [ "$aw_enable" = "1" ] || return

    # Get the WAN interface name
    wan_name=$(get_wan_name)

    # Temporary file path to store the scan results
    tmp_file="/etc/mp_scan_ip_mac_info"

    # If the interface is not empty, get the subnet mask
    if [ -n "$wan_name" ]; then
        subnet_mask=$(get_iface_mask)

        if [ -n "$subnet_mask" ] && [ "$subnet_mask" -ge 22 ]; then
            gl-arp-scan -i "$wan_name" 2>/dev/null > "$tmp_file"
        else
            # Clear the contents of the temporary file
            cat /dev/null > "$tmp_file"
        fi
    else
        # Clear the contents of the temporary file
        cat /dev/null > "$tmp_file"
    fi
}

