[ ! -f "/etc/config/mptun" ] && exit
dns_mark="$(uci -q get mptun.global.dnsmark)"
[ -z "$dns_mark" ] && {
    uci -q set mptun.global.dnsmark='0x8'
    uci commit mptun
    dns_mark='0x8'
}

[ -n "$(uci -q get firewall.adguard_home)" ] && \
    [ -z "$(uci -q get firewall.adguard_home.mark)" ] && \
    uci set firewall.adguard_home.mark="!$dns_mark/$dns_mark"

[ -n "$(uci -q get firewall.adguard_home_guest)" ] && \
    [ -z "$(uci -q get firewall.adguard_home_guest.mark)" ] && \
    uci set firewall.adguard_home_guest.mark="!$dns_mark/$dns_mark"

[ -n "$(uci -q get firewall.dns_over_lan)" ] && \
    [ -z "$(uci -q get firewall.dns_over_lan.mark)" ] && \
    uci set firewall.dns_over_lan.mark="!$dns_mark/$dns_mark"

[ -n "$(uci -q get firewall.dns_over_guest)" ] && \
    [ -z "$(uci -q get firewall.dns_over_guest.mark)" ] && \
    uci set firewall.dns_over_guest.mark="!$dns_mark/$dns_mark"

uci commit firewall
