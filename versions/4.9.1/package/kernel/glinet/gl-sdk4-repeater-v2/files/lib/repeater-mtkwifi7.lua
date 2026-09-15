
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
local M = {}

local hostapd_special_chars = {
    ['"'] = true, ["\\"] = true, ["$"] = true, ["`"] = true
}

local function escape_special_chars(str, special_chars)
    return str:gsub(".", function(c)
        if special_chars[c] then
            return "\\" .. c
        else
            return c
        end
    end)
end

local function commit_sync(c, ...)
    for _, config in ipairs({...}) do
        c:commit(config)
    end

    eco.run(file.sync)
end

local function mtwifi_interface_is_up(name)
    local operstate_file = "/sys/class/net/"..name.."/operstate"
    local file = io.open(operstate_file, "r")
    if file then
        local state = file:read("*l")
        file:close()
        if state == "up" or state == "unknown" then
            return true
        end
    end

    return false
end

local function ap_is_disabled(dev,ifname)

    if not mtwifi_interface_is_up(ifname) then
        if ifname == "rai0" then
            ifname = "rai15"
        elseif ifname == "ra0" then
            ifname = "ra15"
        elseif ifname == "rax0" then
            ifname = "rax15"
        end
    end

    return ifname
end

local function get_iface_mac(iface)
    local path = '/sys/class/net/' .. iface .. '/address'

    if not file.access(path) then
        log.err('get macaddr fail,', iface, 'not exist')
        return nil
    end

    return file.readfile(path, 'l'):upper()
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

local function parse_event(msg)
    local nlh = msg:next()
    if not nlh then
        log.info('parse_event: no nlmsghdr')
        return nil
    end

    if nlh.type ~= rtnl.RTM_NEWLINK then
        log.info("Unknown nlmsg type: " .. nlh.type)
        return nil
    end

    local attrs = msg:parse_attr(rtnl.IFINFOMSG_SIZE)
    if not attrs[rtnl.IFLA_IFNAME] then
        log.info("No IFLA_IFNAME attribute found")
        return nil
    end

    local ifname = nl.attr_get_str(attrs[rtnl.IFLA_IFNAME])

    if not attrs[rtnl.IFLA_WIRELESS] then
        log.info("No IFLA_WIRELESS attribute found")
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

local function hapdctl_connect(dev)
    local ctl
    local ifnames

    if dev.band == '5g' then
        ifnames = 'rai0'
    else
        ifnames = 'ra0'
    end

    ctl = '/var/run/hostapd/' .. ifnames
    local sock = socket.unix_dgram()

    sock:bind('')

    local ok, err = sock:connect(ctl)
    if not ok then
        return nil, err
    end

    return sock
end

function mtkwifi7_get_ap_status(dev)
    local c = uci.cursor()
    local found = false

    for s in c:each('wireless', 'wifi-iface') do
        if s.disabled ~= '1' and s.mode == 'ap' then
            if c:get('wireless', s.device, 'disabled') ~= '1' then
                found = s.device == dev.sid
                if found then
                    break
                end
            end
        end
    end
    c:close()

    if not found then
        if not dev.phy then
            log.err('Waiting for the driver to create phy')
            return 'DISABLED'
        end
        return 'ENABLED'
    end

    local sock, err = hapdctl_connect(dev)
    if not sock then
        log.err('hapdctl_connect:', err)
        return 'DISABLED'
    end

    sock:send('STATUS')

    local data = sock:recv(1024, 1)

    sock:close()

    if not data then
        return 'DISABLED'
    end

    return data:match('state=(%C+)') or 'DISABLED'
end

local function wpactl_connect(dev, attach)
    local ctl = '/var/run/wpa_supplicant/global'

    if not file.access(ctl) then
        return nil, ctl .. ' does not exist'
    end

    local sock = socket.unix_dgram()

    sock:bind('')

    local ok, err = sock:connect(ctl)
    if not ok then
        return nil, err
    end

    if attach then
        local data
        local cnt = 2

        log.info('ATTACH', ctl)

        repeat
            sock:send('ATTACH')
            data, err = sock:recv(512, 1)
            if not data then
                cnt = cnt - 1
                time.sleep(1)
            end
        until data or cnt == 0

        if not data then
            return nil, 'ATTACH fail: ' .. err
        end

        if data ~= 'OK\n' then
            log.err('ATTACH fail:', data)
            return nil, 'ATTACH fail:' .. data
        end
    end

    return sock
end

local function notify_event(cond, ctx, ev)
    for _, key in ipairs({ 'key', 'disconnected', 'connected' }) do
        ctx[key] = ev[key]
    end

    if ev.connected then
        local path = 'network.interface.' .. ctx.network
        local s = ubus.call(path, 'status')
        local dev = ctx.dev
        local iface = dev.iface

        time.at(2, function()
            M.bypass_cac(dev)
        end)
        if not s.device then
            log.info('add device', iface, 'to', path)
            ubus.call(path, 'add_device', { name = iface })
        else
            local net = ctx.net
            log.info('ifname', iface, 'mtu', net.mtu)
            link.set(iface, { mtu = net.mtu or 1500 })
        end
    end

    cond:signal()
end

local function wpa_get_status(dev)
    local sock, err = wpactl_connect(dev)
    if not sock then
        log.err('wpactl_connect fail:', err)
        return nil
    end

    sock:send('IFNAME=' .. dev.iface .. ' STATUS')

    local data, err = sock:recv(512, 0.5)
    if not data then
        sock:close()
        log.err('wait STATUS response fail:', err)
        return nil
    end

    sock:close()

    local status = {}
    for line in data:gmatch('[^\n]+') do
        local name, value = line:match('(.+)=(.+)')
        if name and value then
            status[name] = value
        end
    end

    return status
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

local function iface_type(iface)
    local dir = io.popen("iw "..iface.." info")
    local num
    if not dir then return "" end
    for line in dir:lines() do
        if string.find(line, 'wiphy') then
            num = string.match(line, "%d")
        end
    end
    return "phy"..num
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
    for _, ifname in ipairs({ 'apcli0', 'apclii0', 'apclix0'}) do
        if ifname ~= current.ifname and file.access('/sys/class/net/' .. ifname) then
            if get_iface_mac(ifname) == current.mac then
                set_iface_mac(ifname, generate_random_macaddr())
                log.info('update macaddr of', ifname, 'to avoid conflict')
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
    update_macaddr_other({ ifname = iface, mac = mac })

    return true
end

function restore_channel(dev)
    local c = uci.cursor()
    local channel = c:get('wireless', dev.sid, 'channel')
    local htmode = c:get('wireless', dev.sid, 'htmode')
    local acs_alg = c:get('wireless', dev.sid, 'acs_alg')
    c:close()

    local ifnames
    local _, bw = string.match(htmode, "^([%a]+)%s*(%d+)$")

    if dev.band == '5g' then
        ifnames = { 'rai0', 'rai1' }
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
                    if acs_alg == nil then
                        acs_alg = '3'
                    end
                    cmd = ifname..' acs trigger='..acs_alg
                elseif s.channel ~= tonumber(channel) then
                    cmd = 'phy ' .. iface_type(ifname) .. ' set channel num=' .. channel
                end

                if cmd then
                    sys.sh("mwctl "..cmd)
                    if bw then
                        sys.sh("mwctl ".."phy "..iface_type(ifname).." set channel bw="..bw)
                        log.info(ifname, "restore bw to", bw)
                    end
                    log.info(ifname, 'restore channel to', channel, 'cmd:', cmd)
                end
                break
            end
        end
    end
end

local function has_auth(sec, name)
    return sec and sec.auth_suites and sec.auth_suites[name]
end

local function has_eap_auth(sec)
    return has_auth(sec, '802.1X') or
        has_auth(sec, '802.1X-SHA256') or
        has_auth(sec, '802.1X/SUITE-B-192') or
        has_auth(sec, 'FT-802.1X') or
        has_auth(sec, 'FT-802.1X/SUITE-B-192')
end

function M.bss_has_eap(bss)
    return has_eap_auth(bss.wpa) or has_eap_auth(bss.rsn)
end

local function select_encryption_from_bss(net, bss)
    local has_identity = net.identity ~= nil and net.identity ~= ''
    local wpa = bss.wpa
    local rsn = bss.rsn
    local eap = false
    local owe = false
    local ft = false
    local encryption = 'none'

    if has_identity then
        if has_auth(rsn, 'FT-802.1X/SUITE-B-192') then
            encryption = 'wpa3-192'
            eap = true
            ft = true
        elseif has_auth(rsn, '802.1X/SUITE-B-192') then
            encryption = 'wpa3-192'
            eap = true
        elseif has_auth(rsn, '802.1X-SHA256') then
            encryption = 'wpa3'
            eap = true
        elseif has_auth(rsn, 'FT-802.1X') then
            encryption = 'wpa2+ccmp'
            eap = true
            ft = true
        elseif has_auth(rsn, '802.1X') then
            encryption = 'wpa2+ccmp'
            eap = true
        elseif has_auth(wpa, '802.1X') then
            encryption = 'wpa'
            eap = true
        end
    else
        if has_auth(rsn, 'FT-SAE') then
            encryption = 'sae+ccmp'
            ft = true
        elseif has_auth(rsn, 'SAE') then
            encryption = 'sae+ccmp'
        elseif has_auth(rsn, 'FT-PSK') then
            encryption = 'psk2+ccmp'
            ft = true
        elseif has_auth(rsn, 'PSK') then
            encryption = 'psk2+ccmp'
        elseif has_auth(rsn, 'OWE') then
            encryption = 'owe+ccmp'
            owe = true
        elseif has_auth(wpa, 'PSK') then
            encryption = 'psk+ccmp'
        end
    end

    if has_auth(rsn, 'FT-802.1X') then
        ft = true
    end

    return encryption, eap, owe, ft
end

function M.connect_bss(net, bss, encryption, macaddr, self)
    local dev = net.dev
    local network = net.network
    local eap
    local owe
    local ft
    local c = uci.cursor()

    if c:get('repeater', '@main[0]', 'disabled') == '1' then
        return
    end

    encryption, eap, owe, ft = select_encryption_from_bss(net, bss)

    update_macaddr(dev, macaddr)

    c:delete('wireless', 'sta')
    c:set('wireless', 'sta', 'wifi-iface')
    c:set('wireless', 'sta', 'mode', 'sta')
    c:set('wireless', 'sta', 'ifname', dev.iface)
    c:set('wireless', 'sta', 'device', dev.sid)
    c:set('wireless', 'sta', 'network', network)
    c:set('wireless', 'sta', 'ssid', bss.ssid)
    c:set('wireless', 'sta', 'bssid', bss.bssid)
    c:set('wireless', 'sta', 'wds', net.wds and 1 or '')
    if net.wds then
        c:set('wireless', 'sta', 'mwds', 1)
    end
    c:set('wireless', 'sta', 'macaddr', macaddr or '')
    c:set('wireless', 'sta', 'ieee80211r', ft and 1 or '')

    if eap then
        c:set('wireless', 'sta', 'eap_type', 'peap')
        c:set('wireless', 'sta', 'identity', net.identity)
        c:set('wireless', 'sta', 'password', net.key)
    elseif not owe and (bss.wpa or bss.rsn) then
        c:set('wireless', 'sta', 'key', net.key)
    end

    c:set('wireless', 'sta', 'encryption', encryption)

    commit_sync(c, 'wireless')

    sys.exec("/sbin/wifi", 'reload'):wait()

    if self.pending then
        log.info('aborted')
        return
    end

    time.sleep(2)

    if self.pending then
        log.info('aborted')
        return
    end

    if not net.wds then
        ubus.call('network.interface.' .. net.network, 'add_device', { name = dev.iface, ['link-ext'] = false })
    end

    time.sleep(1)

    return true
end

local function wait_scan_done(dev)
    local sock, err = create_event_sock()
    if not sock then
        return false, err
    end

    local IW_SCAN_COMPLETED_EVENT_FLAG = 0x0211

    local deadtime = sys.uptime() + 25

    while sys.uptime() < deadtime do
        local msg, err = sock:recv(nil, 3.0)
        if not msg then
            log.err('recv fail:', err)
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

local BSS_INFRA = 1

local AKM = {
    OPEN = 0,
    SHARED = 1,
    AUTOSWITCH = 2,
    WPA1 = 3,
    WPA1PSK = 4,
    WPANone = 5,
    WPA2 = 6,
    WPA2PSK = 7,
    FT_WPA2 = 8,
    FT_WPA2PSK = 9,
    WPA2_SHA256 = 10,
    WPA2PSK_SHA256 = 11,
    TDLS = 12,
    SAE_SHA256 = 13,
    FT_SAE_SHA256 = 14,
    SUITEB_SHA256 = 15,
    SUITEB_SHA384 = 16,
    FT_WPA2_SHA384 = 17,
    WAICERT = 18,
    WAIPSK = 19,
    OWE = 20,
    FILS_SHA256 = 21,
    FILS_SHA384 = 22,
    WPA3 = 23,
    OSEN = 24,
    DPP = 25,
    PASN = 26,
    SAE_EXT = 27,
    FT_SAE_EXT = 28,
}

local CIPHER = {
    NONE = 0,
    WEP40 = 1,
    WEP104 = 2,
    WEP128 = 3,
    TKIP = 4,
    CCMP128 = 5,
    CCMP256 = 6,
    GCMP128 = 7,
    GCMP256 = 8,
    BIP_CMAC128 = 9,
    BIP_CMAC256 = 10,
    BIP_GMAC128 = 11,
    BIP_GMAC256 = 12,
    WPI_SMS4 = 13,
    GTK_NOT_USED = 14,
}

local function hasbit(v, bit)
    return v and ((v & (1 << bit)) ~= 0)
end

local function parse_uint_field(field)
    local value

    if not field or field == '' then
        return nil
    end

    value = tonumber(field)
    if value == nil and field:match("^0[xX]") then
        value = tonumber(field:sub(3), 16)
    end

    return value
end

local function scan_dumpall_parse_record(buf, pos)
    local sep1, sep2, sep3, sep4, sep5, sep6, line_end
    local ssid_len, ssid_start, ssid_end

    sep1 = buf:find('|', pos, true)
    if not sep1 then return nil end
    sep2 = buf:find('|', sep1 + 1, true)
    if not sep2 then return nil end

    ssid_len = parse_uint_field(buf:sub(sep1 + 1, sep2 - 1))
    if not ssid_len then return nil end

    ssid_start = sep2 + 1
    ssid_end = ssid_start + ssid_len - 1
    if ssid_end + 1 > #buf or buf:sub(ssid_end + 1, ssid_end + 1) ~= '|' then
        return nil
    end

    sep3 = buf:find('|', ssid_end + 2, true)
    if not sep3 then return nil end
    sep4 = buf:find('|', sep3 + 1, true)
    if not sep4 then return nil end
    sep5 = buf:find('|', sep4 + 1, true)
    if not sep5 then return nil end
    sep6 = buf:find('|', sep5 + 1, true)
    if not sep6 then return nil end

    line_end = buf:find('\n', sep6 + 1, true)
    if not line_end then
        line_end = #buf + 1
    end

    return {
        bssid = string.upper(buf:sub(pos, sep1 - 1)),
        ssid_len = ssid_len,
        ssid = buf:sub(ssid_start, ssid_end),
        channel = tonumber(buf:sub(ssid_end + 2, sep3 - 1)),
        rssi = tonumber(buf:sub(sep3 + 1, sep4 - 1)),
        akm = parse_uint_field(buf:sub(sep4 + 1, sep5 - 1)),
        pairwise = parse_uint_field(buf:sub(sep5 + 1, sep6 - 1)),
        bss_type = parse_uint_field(buf:sub(sep6 + 1, line_end - 1)),
    }, line_end + 1
end

local function build_security_from_raw(akm, pairwise)
    local caps = { ['ESS'] = true }
    local wpa, rsn
    local is_open = hasbit(akm, AKM.OPEN) and hasbit(pairwise, CIPHER.NONE)
    local has_sae_ext = hasbit(akm, AKM.SAE_EXT)
    local has_ft_sae_ext = hasbit(akm, AKM.FT_SAE_EXT)

    if not is_open then
        caps['PRIVACY'] = true
    end

    if hasbit(akm, AKM.WPA1) or hasbit(akm, AKM.WPA1PSK) then
        wpa = { auth_suites = {} }
        if hasbit(akm, AKM.WPA1PSK) then
            wpa.auth_suites['PSK'] = true
        end
        if hasbit(akm, AKM.WPA1) then
            wpa.auth_suites['802.1X'] = true
            caps['EAP'] = true
        end
    end

    if hasbit(akm, AKM.WPA2) or hasbit(akm, AKM.FT_WPA2) or
        hasbit(akm, AKM.WPA3) or hasbit(akm, AKM.SUITEB_SHA384) or
        hasbit(akm, AKM.FT_WPA2_SHA384) or hasbit(akm, AKM.WPA2PSK) or
        hasbit(akm, AKM.FT_WPA2PSK) or hasbit(akm, AKM.WPA2_SHA256) or
        hasbit(akm, AKM.WPA2PSK_SHA256) or hasbit(akm, AKM.SAE_SHA256) or
        hasbit(akm, AKM.FT_SAE_SHA256) or has_sae_ext or has_ft_sae_ext or
        hasbit(akm, AKM.OWE) then
        rsn = { auth_suites = {} }

        if hasbit(akm, AKM.WPA2) then
            rsn.auth_suites['802.1X'] = true
            caps['EAP'] = true
        end

        if hasbit(akm, AKM.FT_WPA2) then
            rsn.auth_suites['FT-802.1X'] = true
            caps['EAP'] = true
            caps['FT'] = true
        end

        if hasbit(akm, AKM.WPA2_SHA256) or hasbit(akm, AKM.WPA3) then
            rsn.auth_suites['802.1X-SHA256'] = true
            caps['EAP'] = true
        end

        if hasbit(akm, AKM.SUITEB_SHA384) then
            rsn.auth_suites['802.1X/SUITE-B-192'] = true
            caps['EAP'] = true
        end

        if hasbit(akm, AKM.FT_WPA2_SHA384) then
            rsn.auth_suites['FT-802.1X/SUITE-B-192'] = true
            caps['EAP'] = true
            caps['FT'] = true
        end

        if hasbit(akm, AKM.WPA2PSK) or hasbit(akm, AKM.WPA2PSK_SHA256) then
            rsn.auth_suites['PSK'] = true
        end

        if hasbit(akm, AKM.FT_WPA2PSK) then
            rsn.auth_suites['FT-PSK'] = true
            caps['FT'] = true
        end

        if hasbit(akm, AKM.SAE_SHA256) or has_sae_ext then
            rsn.auth_suites['SAE'] = true
        end

        if hasbit(akm, AKM.FT_SAE_SHA256) or has_ft_sae_ext then
            rsn.auth_suites['FT-SAE'] = true
            caps['FT'] = true
        end

        if hasbit(akm, AKM.OWE) then
            rsn.auth_suites['OWE'] = true
            caps['OWE'] = true
        end
    end

    return caps, wpa, rsn
end

local function scan_dump(ifname)
    local survey = {}
    local fp = io.popen("mwctl "..ifname.." scan dumpall")
    local scan_result = fp:read("*all")
    local pos = 1

    fp:close()

    while pos <= #scan_result do
        local rec, next_pos = scan_dumpall_parse_record(scan_result, pos)
        local caps, wpa, rsn

        if not rec then
            log.err(ifname, ': scan dumpall raw parse fail at offset ', pos)
            break
        end

        pos = next_pos

        if rec.bss_type ~= BSS_INFRA or not rec.channel or not rec.rssi or
            rec.akm == nil or rec.pairwise == nil then
            goto continue
        end

        caps, wpa, rsn = build_security_from_raw(rec.akm, rec.pairwise)
        if not caps then
            goto continue
        end

        survey[#survey + 1] = {
            bssid = rec.bssid,
            ssid = rec.ssid,
            channel = rec.channel,
            signal = rec.rssi,
            caps = caps,
            wpa = wpa,
            rsn = rsn,
        }

        ::continue::
    end

    log.info(ifname, ': scan dumpall total:', #survey)
    return survey
end

function M.scan(dev, ssid)
    local ifname = dev.phy

    log.info('mtkwifi begin scan, ifname: '..ifname..(ssid and (', ssid: ' .. ssid) or ''))

    ifname = ap_is_disabled(dev, ifname) or ifname
    M.bypass_cac(dev)

    sys.sh("mwctl "..ifname.." scan clear")
    sys.sh("mwctl "..ifname.." set PartialScanNumOfCh=5")
    if ssid then
        sys.sh("mwctl "..ifname.." scan type=partial ssid=\"".. escape_special_chars(ssid, hostapd_special_chars) .. "\"")
    else
        sys.sh("mwctl "..ifname.." scan type=partial")
    end

    local ok, err = wait_scan_done(dev)
    if not ok then
        log.err(ifname, ': wait scan fail:', err)
        return nil
    end

    local survey = scan_dump(ifname)

    return survey
end

function M.listen_event(ctx, cond)
    local dev = ctx.dev
    local bss = ctx.bss

    local sock, err = wpactl_connect(dev, true)
    if not sock then
        log.err('wpactl_connect fail:', err)
        return false
    end
    ctx.sock = sock

    local WLAN_REASON_4WAY_HANDSHAKE_TIMEOUT = 15


    sock:send('IFNAME=' .. dev.iface .. ' STATUS')

    local wpa_pid = sys.sh({'pidof', 'wpa_supplicant'})
    if wpa_pid then
        wpa_pid = wpa_pid:match('(%d+)')
    end

    eco.run(function()
        while true do
            local data, err = sock:recv(512, 1)

            if ctx.exit then break end

            if not data then
                if err ~= 'timeout' then
                    log.err('recv wpa_supplicant event fail:', err)
                    notify_event(cond, ctx, { disconnected = true})
                    break
                end

                local cur_pid = sys.sh({'pidof', 'wpa_supplicant'})
                if cur_pid then
                    cur_pid = cur_pid:match('(%d+)')
                end
                if cur_pid ~= wpa_pid then
                    log.err('wpa_supplicant pid changed: ' .. (wpa_pid or 'nil') .. ' -> ' .. (cur_pid or 'nil'))
                    notify_event(cond, ctx, { disconnected = true })
                    break
                end
            else
                if data:match('CTRL%-EVENT%-DISCONNECTED') then
                    local bssid = data:match('bssid=(%S+)')
                    if string.upper(bssid) == string.upper(bss.bssid) then
                        local reason = data:match('reason=(%d+)')
                        if reason then
                            reason = tonumber(reason)

                            if reason == 4 then
                                log.err('disconnected due to inactivity')
                            end

                            if reason == WLAN_REASON_4WAY_HANDSHAKE_TIMEOUT then
                                notify_event(cond, ctx, { key = true })
                                break
                            end
                        end

                       notify_event(cond, ctx, { disconnected = true})
                    end
                elseif data:match('4%-Way Handshake failed') then
                    notify_event(cond, ctx, { key = true })
                    break
                elseif data:match('CTRL%-EVENT%-AUTH%-REJECT') then
                    notify_event(cond, ctx, { key = true })
                    break
                elseif data:match('CTRL%-EVENT%-CONNECTED') then
                    notify_event(cond, ctx, { connected = true })
                elseif data:match('wpa_state=COMPLETED') then
                    notify_event(cond, ctx, { connected = true })
                end
            end
        end

        sock:close()

        log.info('listen event exited')
    end)

    return true
end

function M.get_status(dev)
    local s = {}

    local ws = wpa_get_status(dev)
    if not ws then
        return nil
    end

    s.wpa_status = ws
    s.macaddr = ws.address
    s.bssid = ws.bssid

    local info = iwinfo.info(dev.iface)
    if not info then
        log.err('iwinfo: get info of', dev.iface, 'fail')
        return s
    end

    s.band = dev.band
    s.ssid = info.ssid
    s.channel = info.channel
    s.signal = info.signal
    s.htmode = info.htmode

    return s
end

function M.lookup_phy(s)
    local sid = s['.name']

    if sid == 'MT7992_1_1' or sid == 'MT7993_1_1' or sid == 'MT7990_1_1' then
        return 'ra0', 'apcli0'
    elseif sid == 'MT7992_1_2' or sid == 'MT7993_1_2' or sid == 'MT7990_1_2' then
        return 'rai0', 'apclii0'
    elseif sid == 'MT7990_1_3' then
        return 'rax0', 'apclix0'
    else
        return nil
    end
end

function M.bypass_cac(dev)
    if dev.band == '5g' then
        sys.sh("mwctl rai15 set bypasscac=1")
    end
end

function M.delete_net_device(network)
    remove_net_dev(network)
end

function M.disconnect(net, self)
    remove_net_dev(net.network)

    sys.exec("/sbin/wifi", 'reload'):wait()
end

function M.get_timeout_period()
    local timeout = 45

    return timeout
end

return M
