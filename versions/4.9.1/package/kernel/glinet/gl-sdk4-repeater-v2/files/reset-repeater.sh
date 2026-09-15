. /lib/functions.sh 

fix_network() {
    uci delete repeater."$1".network
    uci delete repeater."$1".wds
}

config_load repeater
config_foreach fix_network network

uci set repeater.@main[0].auto='1'
uci set repeater.@main[0].disabled='1'

uci commit repeater

ubus call repeater reload
ubus call repeater disconnect
