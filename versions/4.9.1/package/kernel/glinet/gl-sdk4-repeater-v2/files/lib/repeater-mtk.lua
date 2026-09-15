-- Author: Jianhui Zhao <jianhui.zhao@gl-inet.com>

local link = require 'eco.ip'.link
local socket = require 'eco.socket'
local rtnl = require 'eco.rtnl'
local nl = require 'eco.nl'
local file = require 'eco.file'
local ubus = require 'eco.ubus'
local time = require 'eco.time'
local log = require 'eco.log'
local sys = require 'eco.sys'
local uci = require 'uci'

local iwinfo = require 'iwinfo'
local iwpriv = require 'iwpriv'

local M = {}
local disconnect_timer = nil

local function ap_is_disabled(ifname)
    local c = uci.cursor()
    local sid = 'wifi2g'

    if ifname == 'rax0' then
        sid = 'wifi5g'
    end

    local disabled = c:get('wireless', sid, 'disabled') == '1'
    c:close()
    return disabled
end

local function ifdown_disabled_ap(ifname)
    if ap_is_disabled(ifname) then
        sys.exec('ifconfig', ifname, 'down'):wait()
    end
end

local function get_iface_mac(iface)
    local path = '/sys/class/net/' .. iface .. '/address'

    if not file.access(path) then
        log.err('get macaddr fail,', iface, 'not exist')
        return nil
    end

    return file.readfile(path, 'l'):upper()
end

function M.init()
    -- wait until system has started 50s
    time.sleep(50 - sys.uptime())

    sys.exec('ifconfig', 'ra0', 'up'):wait()
    sys.exec('ifconfig', 'apcli0', 'up'):wait()
    sys.exec('ifconfig', 'apclix0', 'up'):wait()

    iwpriv.set('apcli0', 'ApCliEnable', 0)
    iwpriv.set('apclix0', 'ApCliEnable', 0)

    ifdown_disabled_ap('ra0')
end

function M.lookup_phy(s)
    local sid = s['.name']

    if sid == 'mt798111' or sid == 'mt798611' then
        return 'ra0', 'apcli0'
    elseif sid == 'mt798112' or sid == 'mt798612' then
        return 'rax0', 'apclix0'
    else
        return nil
    end
end

function M.bypass_cac(dev)
    if dev.band == '5g' then
        iwpriv.set(dev.iface, 'ByPassCac', 1)
    end
end

local function parse_event(msg)
    local nlh = msg:next()
    if not nlh then
        return nil
    end

    if nlh.type ~= rtnl.RTM_NEWLINK then
        return nil
    end

    local attrs = msg:parse_attr(rtnl.IFINFOMSG_SIZE)
    if not attrs[rtnl.IFLA_IFNAME] then
        return nil
    end

    local ifname = nl.attr_get_str(attrs[rtnl.IFLA_IFNAME])

    if not attrs[rtnl.IFLA_WIRELESS] then
        return nil
    end

    local data = nl.attr_get_payload(attrs[rtnl.IFLA_WIRELESS])
    local cmd = string.unpack('I2', data:sub(3))
    local IWEVCUSTOM = 0x8c02
    if cmd ~= IWEVCUSTOM then
        return nil
    end

    return ifname, string.unpack('I2', data:sub(11))
end

local function create_event_sock()
    local sock, err = nl.open(nl.NETLINK_ROUTE)
    if not sock then
        log.err('open NETLINK_ROUTE:', err)
        return false
    end

    local ok, err = sock:bind(rtnl.RTNLGRP_LINK)
    if not ok then
        log.err('bind RTNLGRP_LINK:', err)
        sock:close()
        return false
    end

    return sock
end

local function wait_scan_done(dev)
    local sock, err = create_event_sock()
    if not sock then
        return false, err
    end

    local IW_SCAN_COMPLETED_EVENT_FLAG = 0x0211

    local deadtime = sys.uptime() + 20

    while sys.uptime() < deadtime do
        local msg, err = sock:recv(nil, 3.0)
        if not msg then
            if err ~= 'timeout' then
                sock:close()
                return false, 'recv netlink fail:' .. err
            end
        else
            local ifname, ev = parse_event(msg)
            if ifname == dev.phy and ev == IW_SCAN_COMPLETED_EVENT_FLAG then
                sock:close()
                return true
            end
        end
    end

    sock:close()
    return false, 'timeout'
end

local function scan_dump(ifname)
    local index = 0
    local survey = {}
    local total

    while true do
        local data = iwpriv.get_site_survey2(ifname, index) or ''

        if data:find('No BssInfo') then
            break
        end

        if not total then
            total = data:match('Total=(%d+)')
            if not total then
                break
            end
            total = tonumber(total)
        end

        local pos = data:find('No')
        if not pos then
            break
        end

        data = data:sub(pos)

        local widths = {}

        widths.ch = {data:find('Ch '), 3}
        widths.ssid = {data:find('SSID '), 32}
        widths.bssid = {data:find('BSSID '), 17}
        widths.security = {data:find('Security '), 22}
        widths.signal = {data:find('Rssi'), 3}
        widths.wmode = {data:find('W%-Mode'), 11}
        widths.nt = {data:find('NT'), 2}

        if data:match('SSID_Len') then
            widths.ssid_len = {data:find('SSID_Len'), 2}
        else
            widths.ssid_len = {data:find('Len'), 2}
        end

        pos = data:find('%d+')
        if not pos then
            break
        end

        data = data:sub(pos)

        local function get_field(line, width, no_trim)
            local value = line:sub(width[1], width[1] + width[2])
            if no_trim then
                return value
            end

            return value:match('%S+')
        end

        local nindex = index

        while true do
            if not data:match('^%d+') then
                break
            end
            nindex = tonumber(data:match('^%d+'))
            local channel = tonumber(get_field(data, widths.ch))
            local ssid = get_field(data, widths.ssid, true)
            local bssid = get_field(data, widths.bssid)
            local security = get_field(data, widths.security)
            local signal = tonumber(get_field(data, widths.signal))
            local nt = get_field(data, widths.nt)
            local ssid_len = tonumber(get_field(data, widths.ssid_len))

            if security == 'NONE' then
                security = 'OPEN'
            end

            local authmode = security:match('([%w-]+)/?%w*')

            if nt == 'In' and (authmode:match('PSK') or authmode:match('FT') or authmode == 'OPEN') then
                local caps = { ['ESS'] = true }

                local wpa, rsn

                if authmode ~= 'OPEN' then
                    if authmode:match('WPAPSK') then
                        wpa = { auth_suites = { ['PSK'] = true } }
                    end

                    if authmode:match('WPA2PSK') then
                        rsn = { auth_suites = { ['PSK'] = true } }
                    end

                    if authmode:match('WPA3PSK') or authmode:match('FT%-SAE') then
                        if rsn then
                            rsn.auth_suites['SAE'] = true
                        else
                            rsn = { auth_suites = { ['SAE'] = true } }
                        end
                    end

                    caps['PRIVACY'] = true
                end

                survey[#survey + 1] = {
                    bssid = bssid,
                    ssid = ssid:sub(0, ssid_len),
                    channel = channel,
                    signal = signal,
                    caps = caps,
                    wpa = wpa,
                    rsn = rsn
                }
            end

            data = data:sub(widths.ssid_len[1])

            local off = data:find('\n')
            if not off then
                break
            end

            data = data:sub(off + 1)
        end

        if nindex == index or index + 1 == total then
            break
        end

        index = nindex + 1
    end

    return survey
end

function M.scan(dev, ssid)
    local ifname = dev.phy

    if ap_is_disabled(ifname) then
        sys.exec('ifconfig', ifname, 'up'):wait()
    end

    if dev.phy == 'mt7628' then
        iwpriv.set(ifname, 'SiteSurvey', ssid or '')
    else
        iwpriv.set(ifname, 'ScanSSID', ssid or '')
        iwpriv.set(ifname, 'PartialScanNumOfCh', 5)
        iwpriv.set(ifname, 'PartialScan', 1)
    end

    local ok, err = wait_scan_done(dev)
    if not ok then
        log.err(ifname, ': wait scan fail:', err)
        return nil
    end

    local survey = scan_dump(ifname)

    ifdown_disabled_ap(ifname)

    return survey
end

local function notify_event(cond, ctx, ev)
    local ok
    local cnt = 40

    for _, key in ipairs({ 'key', 'disconnected', 'connected' }) do
        ctx[key] = ev[key]
    end

    repeat
        ok = cond:signal()
        if not ok then
            time.sleep(0.2)
            cnt = cnt - 1
        end
    until ok or cnt < 1
end

function M.listen_event(ctx, cond)
    local dev = ctx.dev

    local sock, err = create_event_sock()
    if not sock then
        return false, err
    end

    ctx.sock = sock

    time.at(1, function()
        local s = M.get_status(dev)
        if s.bssid == string.upper(ctx.bss.bssid) then
            notify_event(cond, ctx, { connected = true })
        end
    end)

    local IW_PAIRWISE_HS_TIMEOUT_EVENT_FLAG = 0x020b
    local IW_STA_LINKDOWN_EVENT_FLAG = 0x0210

    eco.run(function()
        while true do
            local msg, err = sock:recv()

            if ctx.exit then break end

            if not msg then
                log.err('recv event fail:', err)
                notify_event(cond, ctx, { disconnected = true})
                break
            else
                local ifname, ev = parse_event(msg)
                if ifname == dev.iface then
                    if ev == 0x9999 then
                        notify_event(cond, ctx, { connected = true })
                    elseif ev == IW_PAIRWISE_HS_TIMEOUT_EVENT_FLAG then
                        notify_event(cond, ctx, { key = true })
                    elseif ev == IW_STA_LINKDOWN_EVENT_FLAG then
                        notify_event(cond, ctx, { disconnected = true})
                    end
                end
            end
        end

        sock:close()

        log.info('listen event exited')
    end)

    return true
end

function M.get_status(dev)
    local iface = dev.iface
    local s = iwinfo.info(iface) or {}

    return {
        macaddr = get_iface_mac(iface),
        bssid = s.bssid,
        ssid = s.ssid,
        channel = s.channel,
        signal = s.signal,
        htmode = s.htmode
    }
end

local function uci_encryption_to_mtk(encryption)
    local AuthMode = 'OPEN'
    local EncrypType = 'NONE'
    local is_wpa = false

    if encryption:match('^psk') or encryption:match('^sae') or encryption:match('^wpa') then
        if encryption:match('^sae') then
            AuthMode = 'WPA3PSK'
        elseif encryption:match('^psk2') then
            AuthMode = 'WPA2PSK'
        elseif encryption:match('^psk') then
            AuthMode = 'WPAPSK'
        elseif encryption:match('^wpa3') then
            AuthMode = 'WPA3'
        elseif encryption:match('^wpa2') then
            AuthMode = 'WPA2'
        else
            AuthMode = 'WPA'
        end

        if encryption:match('mixed') then
            if encryption:match('^sae') then
                AuthMode = 'WPA2PSKWPA3PSK'
            elseif encryption:match('^psk') then
                AuthMode = 'WPAPSKWPA2PSK'
            elseif encryption:match('^wpa') then
                AuthMode = 'WPA1WPA2'
            end
        end

        local ciphers = {}

        EncrypType = ''

        if encryption:match('tkip') then
            ciphers['TKIP'] = true
        end

        if encryption:match('aes') or encryption:match('ccmp') then
            ciphers['AES'] = true
        end

        if ciphers['TKIP'] then
            EncrypType = 'TKIP'
        end

        if ciphers['AES'] then
            EncrypType = EncrypType .. 'AES'
        end

        if EncrypType == '' then
            EncrypType = 'AES'
        end

        if encryption:match('^wpa') then
            is_wpa = true
        end
    end

    return AuthMode, EncrypType, is_wpa
end

local function remove_net_dev(network)
    local netpath = 'network.interface.' .. network
    local s = ubus.call(netpath, 'status')
    if not s then
        log.err('an exception occurs: "' .. network .. '" not exists')
        return
    end

    local device = s.device
    if not device then
        return
    end

    if device == 'br-lan' then
        s = ubus.call('network.device', 'status', { name = device }) or {}
        for _, m in ipairs(s['bridge-members'] or {}) do
            if m:match('apcli') then
                log.info('remove "' .. m .. '" from ' .. network)
                link.set(m, { nomaster = true })
            end
        end
    else
        log.info('remove "' .. device .. '" from ' .. network)
        ubus.call(netpath, 'remove_device', { name = device })
    end
end

local function generate_random_macaddr(cnt)
    local nums = {}

    cnt = cnt or 6

    for i = 1, cnt do
        local n = math.random(1, 255)

        if i == 1 then
            n = n & 0xfe
        end

        nums[i] = string.format('%02x', n)
    end

    return table.concat(nums, ':')
end

local function set_iface_mac(ifname, mac)
    local cnt = 10
    local ok

    repeat
        link.set(ifname, { down = true })
        ok = link.set(ifname, { address = mac })
        if not ok then
            time.sleep(0.1)
            cnt = cnt - 1
        end
    until ok or cnt < 1
end

local function update_macaddr_other(current)
    for _, ifname in ipairs({ 'apcli0', 'apclix0' }) do
        if ifname ~= current.ifname and file.access('/sys/class/net/' .. ifname) then
            if get_iface_mac(ifname) == current.mac then
                set_iface_mac(ifname, generate_random_macaddr())
                sys.exec('ifconfig', ifname, 'up'):wait()
            end
        end
    end
end

local function update_macaddr(dev, mac)
    local iface = dev.iface

    if not mac or mac == '' then
        return
    end

    mac = mac:upper()

    -- In MTK solutions, configuration is required regardless of whether the MAC address is modified
    -- to prevent the driver from using an incorrect MAC address that causes abnormal packet transmission and reception
    log.info('update macaddr of', iface, 'to', mac)

    set_iface_mac(iface, mac)

    update_macaddr_other({ ifname = iface, mac = mac })

    return true
end

local function restore_channel(dev)
    local c = uci.cursor()
    local channel = c:get('wireless', dev.sid, 'channel')
    c:close()

    local ifnames

    if dev.band == '5g' then
        ifnames = { 'rax0', 'rax1' }
    else
        ifnames = { 'ra0', 'ra1' }
    end

    for _, ifname in ipairs(ifnames) do
        if file.access('/sys/class/net/' .. ifname) then
            local res = link.get(ifname)
            if res and res.up then
                local s = iwinfo.info(ifname) or {}
                local cmd

                if channel == 'auto' then
                    cmd = 'AutoChannelSel=3'
                elseif s.channel ~= tonumber(channel) then
                    cmd = 'Channel=' .. channel
                end

                if cmd then
                    sys.exec('iwpriv', ifname, 'set', cmd):wait()
                    log.info(ifname, 'restore channel to', channel)
                end
                break
            end
        end
    end
end

local function delete_route_by_dev(ifname)
    for line in io.lines('/proc/net/route') do
        local fields = {}

        for field in line:gmatch('%S+') do
            fields[#fields + 1] = field
        end

        if fields[1] == ifname then
            local dest = tonumber(fields[2], 16)
            local mask = tonumber(fields[8], 16)

            dest = socket.inet_ntoa(dest)
            mask = socket.inet_ntoa(mask)

            os.execute(string.format('ip route del %s/%s dev %s', dest, mask, ifname))
        end
    end
end

function M.connect_bss(net, bss, encryption, macaddr, self)
    local dev = net.dev
    local iface = dev.iface

    self.configuring = true
    for _, d in ipairs(self.devices) do
        if dev.sid ~= d.sid then
            sys.exec('ifconfig', d.iface, 'up'):wait()
            iwpriv.set(d.iface, 'ApCliEnable', 0)
            restore_channel(d)
        end
    end

    local AuthMode, EncrypType, is_wpa = uci_encryption_to_mtk(encryption)

    if is_wpa then
        log.err('not support eap')
        self.configuring = false
        return false
    end

    update_macaddr(dev, macaddr)

    remove_net_dev(net.network)

    time.sleep(1)
    sys.exec('ifconfig', iface, 'up'):wait()

    iwpriv.set(iface, 'ApCliEnable', 0)
    iwpriv.set(iface, 'ApCliSsid', bss.ssid)
    iwpriv.set(iface, 'ApCliBssid', bss.bssid)
    iwpriv.set(iface, 'ApCliAuthMode', AuthMode)
    iwpriv.set(iface, 'ApCliEncrypType', EncrypType)
    iwpriv.set(iface, 'ApCliWPAPSK', net.key)
    iwpriv.set(iface, 'ApCliPMFMFPC', 1)
    iwpriv.set(iface, 'ApCliDelPMKIDList', 1)
    iwpriv.set(iface, 'ApCliEnable', 1)

    sys.exec('iwpriv', iface, 'set', 'Channel=' .. bss.channel):wait()
    self.configuring  = false

    if not net.wds then
        ubus.call('network.interface.' .. net.network, 'add_device', { name = dev.iface, ['link-ext'] = false })
    end

    time.sleep(1)

    M.bypass_cac(dev)

    -- Maybe the DFS CAC starts late in the driver, confirm again.
    time.at(3, function()
        M.bypass_cac(dev)
    end)

    return true
end

function M.disconnect(net, self)
    remove_net_dev(net.network)

    local dev = net.dev
    if not dev then
        return
    end

    iwpriv.set(dev.iface, 'ApCliEnable', 0)

    delete_route_by_dev(dev.iface)

    restore_channel(dev)
    if disconnect_timer == nil then
        disconnect_timer = time.at(10, function()
            local c = uci.cursor()
            local repeater_disabled = c:get('repeater', '@main[0]', 'disabled')
            if repeater_disabled ~= '1' then
                disconnect_timer:set(10)
                return
            end
            local wlan_info = ubus.call('network.interface.wwan', 'status')
            if wlan_info and wlan_info.up then
                log.info("need close sta interface")
                remove_net_dev(net.network)
                iwpriv.set(dev.iface, 'ApCliEnable', 0)
                delete_route_by_dev(dev.iface)
                restore_channel(dev)
                -- also make web disabled
                self.state = self.states.DISABLED
            end

            if wlan_info and not wlan_info.up then
                disconnect_timer:cancel()
                disconnect_timer = nil
                return
            end

            ubus.call('repeater', 'disconnect')
            disconnect_timer:set(10)
        end)
    end
end

return M
