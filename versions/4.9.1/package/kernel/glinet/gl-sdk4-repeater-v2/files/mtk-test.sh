#!/bin/sh

ifname=apclix0
ssid="test"
bssid=""
key=12345678
channel=40

ifconfig $ifname up

iwpriv $ifname set ApCliEnable=0
iwpriv $ifname set ApCliSsid="$ssid"
iwpriv $ifname set ApCliBssid="$bssid"
iwpriv $ifname set ApCliAuthMode=WPA2PSK
iwpriv $ifname set ApCliEncrypType=AES
iwpriv $ifname set ApCliWPAPSK="$key"

iwconfig apclix0 | grep -q "Channel:$channel" || iwpriv $ifname set Channel=$channel

iwpriv $ifname set ApCliEnable=1

ubus call network.interface.wwan add_device "{\"name\":\"$ifname\"}"
