#!/bin/ash
# 设置策略路由
set_route() {
    local tcpmark="$(uci -q get mptun.global.tcpmark || echo 0x1)"
    ip rule del pref 10 2>/dev/null
    ip rule add fwmark ${tcpmark}/${tcpmark} table 10 pref 10
    ip route add local 0.0.0.0/0 dev lo table 10
    #local address use main route table
    ip rule add from all lookup main suppress_prefixlength 1 pref 9
}


#清除
clean_route() {
    ip rule del pref 10
    ip route flush table 10
    ip rule del pref 9
}

case $1 in
    "set")
        set_route
    ;;
    "clean")
        clean_route
    ;;
esac
