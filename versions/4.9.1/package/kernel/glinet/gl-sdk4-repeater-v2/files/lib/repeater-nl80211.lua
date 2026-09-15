-- Author: Jianhui Zhao <jianhui.zhao@gl-inet.com>

local nl80211 = require 'eco.nl80211'
local socket = require 'eco.socket'
local link = require 'eco.ip'.link
local time = require 'eco.time'
local ubus = require 'eco.ubus'
local file = require 'eco.file'
local log = require 'eco.log'
local nl = require 'eco.nl'
local sys = require 'eco.sys'

local iwinfo = require 'iwinfo'
local uci = require 'uci'

local M = {}

local function is_sft_model()
    local model = file.readfile('/proc/gl-hw-info/model', 'l');

    if type(model) ~= "string" then
        return false
    end
    local sft_models = {
        sft1200 = true,
        sft3000 = true,
        -- 可添加更多型号
    }

    return sft_models[model] or false
end

local function is_ath9k_driver()
    return file.access('/sys/module/ath9k')
end

local function generate_random_mac()
    local s = {}

    for i = 1, 6 do
        s[i] = math.random(16)
    end

    if s[1] % 2 == 1 then
        s[1] = s[1] - 1
    end

    for i = 1, 6 do
        s[i] = string.format('%02x', s[i])
    end

    return table.concat(s, ':')
end

local function create_iface(dev)
    local iface = dev.iface

    if file.access('/sys/class/net/' .. iface) then
        return true
    end

    if dev.type == 'qcacld32' then
        -- For qcacld32, wlan0 is can used for create sta iface
        local cmd = string.format('iw wlan0 interface add %s type managed', iface)
        os.execute(cmd)
    else
        local mac = generate_random_mac()
        local ok, err = nl80211.add_interface(dev.phy, iface, { type = nl80211.IFTYPE_STATION, mac = mac })
        if not ok then
            log.err('add interface:', iface, err)
            local res = link.get(iface)
            if res and not res.up then
                link.set(iface, { up = true })
            end
            return false
        end
    end

    link.set(iface, { up = true })
    return true
end

local function wpactl_connect(dev, attach)
    local ctl

    if dev.type == 'qcawificfg80211' then
        ctl = '/var/run/wpa_supplicantglobal'
    elseif is_sft_model() then
        ctl = '/var/run/wpa_supplicant/' .. dev.iface
    elseif dev.type == 'qcacld32' then
        ctl = '/data/vendor/wifi/wpa_supplicant/' .. dev.iface
    else
        ctl = '/var/run/wpa_supplicant/global'
    end

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

local function hapdctl_connect(dev)
    local ctl

    if dev.type == 'qcawificfg80211' then
        local dir = '/var/run/hostapd-' .. dev.sid
        for name in file.dir(dir) do
            if name ~= '.' and name ~= '..' then
                ctl = dir .. '/' .. name
                break
            end
        end
    else
        local phyconf = '/var/run/hostapd-' .. dev.phy .. '.conf'

        if not file.access(phyconf) then
            return nil, phyconf .. '" not exist'
        end

        local ifname = file.readfile(phyconf):match('interface=(%C+)')
        ctl = '/var/run/hostapd/' .. ifname
    end

    if not ctl then
        return nil, 'not found ap ctl'
    end

    local sock = socket.unix_dgram()

    sock:bind('')

    local ok, err = sock:connect(ctl)
    if not ok then
        return nil, err
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

    if is_sft_model() or dev.type == 'qcacld32' then
        sock:send('STATUS')
    else
        sock:send('IFNAME=' .. dev.iface .. ' STATUS')
    end

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

function M.disconnect(net)
    local c = uci.cursor()
    local staname
    local wifiname
    local mode = c:get('glconfig', 'general', 'mode')
    local ifname

    if not file.access('/lib/wifi/qcacld32.sh') then
        return
    end

    if net and net.dev and net.dev.iface then
        staname = net.dev.iface
    end

    if net and net.dev and net.dev.sid then
        wifiname = net.dev.sid
    end

    -- when repeater disconnect,disable sta interface
    if mode ~= 'wds' and mode ~= 'relay' then
        if wifiname and staname then
            log.info('multi_down staname', wifiname, staname)
            local cmd = string.format('/sbin/wifi multi_down %s %s', wifiname, staname)
            sys.sh(cmd)
        end
    end

    -- because the guest network and sta network share a common ifname,so delete it
    c:delete('wireless', 'sta', 'ifname')

    -- reset guest ap interface name
    for s in c:each('wireless', 'wifi-iface') do
        if s.network == 'guest' then
            if s.device == 'wifi0' then
                -- do nothing for main ap interface
                c:set("wireless", s['.name'], "ifname", "wlan3")
                c:set("network", "wlan3", "disabled", "0")
            elseif s.device == 'wifi1' then
                c:set("wireless", s['.name'], "ifname", "wlan4")
                c:set("network", "wlan4", "disabled", "0")
            elseif s.device == 'wifi2' then
                c:set("wireless", s['.name'], "ifname", "wlan5")
                c:set("network", "wlan5", "disabled", "0")
            end
        elseif s.network == 'iot' then
            if s.device == 'wifi0' then
                -- do nothing for main ap interface
                c:set("wireless", s['.name'], "ifname", "wlan6")
                c:set("network", "wlan6", "disabled", "0")
            elseif s.device == 'wifi1' then
                c:set("wireless", s['.name'], "ifname", "wlan7")
                c:set("network", "wlan7", "disabled", "0")
            elseif s.device == 'wifi2' then
                c:set("wireless", s['.name'], "ifname", "wlan8")
                c:set("network", "wlan8", "disabled", "0")
            end
        elseif s.network == 'lan' then
            if s.device == wifiname then
                ifname = s.ifname
            end
        end
    end
    -- when repeater disconnect,restore ap channel
    if mode ~= 'wds' and mode ~= 'relay' then
        if wifiname and ifname then
            local cmd_restore = string.format('/sbin/wifi multi_up %s %s', wifiname, ifname)
            -- fix Realtek RTL8852BE WiFi 6 can't use ssh
            if staname then
                sys.sh({ 'hostapd_cli', '-i', staname, 'disassociate', 'FF:FF:FF:FF:FF:FF' })
            end
            sys.sh(cmd_restore)
        end
    end

    c:commit('wireless')
    c:commit('network')
    c:close()
end

-- Disable guest network ap interface and delete its ifname
local function disabled_no_main_ap(c)
    for s in c:each('wireless', 'wifi-iface') do
        -- disable the guest network ap interface
        if (s.network == "guest" or s.network == "iot") and s.disabled == "0" then
            c:set("wireless", s[".name"], "disabled", "1")
            local cmd = string.format('/sbin/wifi multi_down %s %s', s.device, s.ifname)
            sys.sh(cmd)
        end
        -- delete the ifname of guest network,because the guest network and sta network share a common ifname
        if s.network == "guest" or s.network == "iot" then
            local ifname = c:get("wireless", s[".name"], "ifname")
            if ifname then c:set("network", ifname, "disabled", "1") end
            c:delete("wireless", s[".name"], "ifname")
        end
    end

    c:set("wireless", "autoparam", "use_guestenable", 1)
    c:set("wireless", "autoparam", "use_iotenable", 1)
    local guest_disabled = c:get("network", "guest", "disabled")
    if guest_disabled and guest_disabled == "0" then
        c:set("network", "guest", "disabled", "1")
    end
    local iot_disabled = c:get("network", "iot", "disabled")
    if iot_disabled and iot_disabled == "0" then
        c:set("network", "iot", "disabled", "1")
    end

    c:commit("wireless")
    c:commit("network")
end


-- for qcacld32, if we switch mutex band is 5G or 6G, we need to set autoparam
-- and disable last ap interface, and enable current ap interface according to last ap status
function M.switch_mutex_band(c, dev)
    local old_useband = c:get("wireless", "autoparam", "useband")
    local target_section = nil
    local need_enable_current = false

    -- disable guest network and delete its ifname
    disabled_no_main_ap(c)

    -- if repeat 2g or same band internet, do nothing
    if old_useband == dev.band or dev.band == '2g' then
        return
    end

    c:set("wireless", "autoparam", "useband", dev.band)
    c:set("wireless", "autoparam", "usemode", 'auto')

    for s in c:each("wireless", "wifi-iface") do
        if s.device ~= 'wifi0' and s.mode == "ap" and s.network ~= 'guest' and s.network ~= 'iot' then
            if s.device ~= dev.sid then
                -- Disable other enabled AP interfaces
                if s.disabled == "0" then
                    c:set("wireless", s['.name'], "disabled", 1)
                    local cmd = string.format('/sbin/wifi multi_down %s %s', s.device, s.ifname)
                    sys.sh(cmd)
                    need_enable_current = true
                end
            else
                -- Record the configuration section of the current device
                target_section = s
            end
        end
    end
    -- Enable the current AP interface (if necessary)
    if need_enable_current and target_section then
        c:set("wireless", target_section['.name'], "disabled", 0)
        c:commit('wireless')
        local cmd = string.format('/sbin/wifi multi_up %s %s', target_section.device, target_section.ifname)
        sys.sh(cmd)
    end
    c:commit('wireless')

    return
end

local function is_mt798x_driver()
    return file.access('/sys/module/mt7915e')
end

-- If using EAP authentication, after successful association, check whether the upstream connection is reachable.
-- If not, execute PMKSA_FLUSH to the wpa_supplicant to resolve the issue of unreachability on the relay.
local function eap_network_check(sock, iface)
    local s = ubus.call('repeater', 'status')
    local gw = ((s or {}).ipv4 or {}).gateway
    local wpa_state = ((s or {}).wpa_status or {}).wpa_state
    local key_mgmt = ((s or {}).wpa_status or {}).key_mgmt

    if wpa_state ~= 'COMPLETED' or not key_mgmt:match('EAP') then
        return
    end

    local result = sys.sh({ 'ping', '-c', '3', '-W', '1', gw })
    local recv_count
    if result then
        recv_count = result:match("(%d+) packets received")
    end

    if recv_count and tonumber(recv_count) > 0 then
        log.debug('ping EAP network reachable')
        return
    end

    if socket.connect_tcp(gw, 53) then
        log.debug('DNS network reachable')
        return
    end

    local content = file.readfile('/proc/gl-kmwan/config')
    if not content then
        return
    end
    for key, value in content:gmatch('(%S+):(%S+)') do
        if key == 'wwan' and value == 'online' then
            log.debug('network reachable')
            return
        end
    end

    log.info('EAP network unreachable, flush PMKSA cache')
    sock:send('IFNAME='..iface..' PMKSA_FLUSH')
end

-- Check every 10 seconds
local function fixed_eap_network_issue(sock, dev, ctx)
    ctx.gw_check_tmr = time.at(0, function(tm)
        eap_network_check(sock, dev.iface)
        tm:set(10)
    end)
end

function M.listen_event(ctx, cond)
    local dev = ctx.dev
    local bss = ctx.bss
    local count = 0

    while true do
        if not is_ath9k_driver() then break end

        local status =  wpa_get_status(dev)
        if status and status['bssid'] == bss.bssid then
            break
        end

        time.sleep(1.5)
        count = count + 1
        if count > 12 then
            log.err('Loop to get wpa_get_status too many times')
            return false
        end
    end

    local sock, err = wpactl_connect(dev, true)
    if not sock then
        log.err('wpactl_connect fail:', err)
        return false
    end

    ctx.sock = sock

    local WLAN_REASON_4WAY_HANDSHAKE_TIMEOUT = 15

    if is_sft_model() or dev.type == 'qcacld32' then
        sock:send('STATUS')
    else
        sock:send('IFNAME=' .. dev.iface .. ' STATUS')
    end

    if is_mt798x_driver() then fixed_eap_network_issue(sock, dev, ctx) end

    eco.run(function()
        while true do
            local data, err = sock:recv(512)

            if ctx.exit then break end

            if not data then
                log.err('recv wpa_supplicant event fail:', err)
                notify_event(cond, ctx, { disconnected = true})
                break
            else
                if data:match('CTRL%-EVENT%-DISCONNECTED') then
                    local bssid = data:match('bssid=(%S+)')
                    if bssid == bss.bssid then
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
                elseif data:match('CTRL%-EVENT%-CONNECTED') then
                    notify_event(cond, ctx, { connected = true })
                elseif data:match('wpa_state=COMPLETED') then
                    notify_event(cond, ctx, { connected = true })
                end
            end
        end

        sock:close()

        if ctx.gw_check_tmr then
            ctx.gw_check_tmr:cancel()
        end

        log.info('listen event exited')
    end)

    return true
end

local function get_ap_status(dev)
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

-- Filter scan results by band
local function filter_scanresult(survey, dev)
    local survey_result = {}

    for _, bss in ipairs(survey) do
        if bss.band == 2.4 then
            bss.band = '2g' -- 2.4GHz
            bss.device = 'wifi0'
        elseif bss.band == 5 then
            bss.band = '5g' -- 5GHz
            bss.device = 'wifi1'
        elseif bss.band == 6 then
            bss.band = '6g' -- 6GHz
            bss.device = 'wifi2'
        else
            bss.band = 'unknown'
            bss.device = 'unknown'
        end

        if bss.band == dev.band then
            table.insert(survey_result, bss)
        end
    end

    return survey_result
end

local function bss_is_gl_cntr(bss)
    local vies = bss.elems and bss.elems[nl80211.WLAN_EID_VENDOR_SPECIFIC]
    local GL_OUI = '\x94\x83\xc4'
    local OUI_TYPE_CNTR_PUBKEY = 0x01

    bss.elems = nil

    for _, vie in ipairs(vies or {}) do
        if vie:sub(1, 3) == GL_OUI and vie:byte(4) == OUI_TYPE_CNTR_PUBKEY then
            return true
        end
    end

    return false
end

local function nl80211_get_scanresult(dev, ifname)
    local survey, err = nl80211.scan('dump', { ifname = ifname, keep_elems = true })
    if not survey then
        log.err(ifname .. ': scan dump fail:', err)
        return nil
    end

    for _, bss in ipairs(survey) do
        bss.is_gl_cntr = bss_is_gl_cntr(bss)
    end

    if dev.type == 'qcacld32' then
        return filter_scanresult(survey, dev)
    end

    return survey
end

local function get_freq_list(dev)
    local freqlist = iwinfo.freqlist(dev.iface)
    local freqs = {}

    if freqlist then
        for _, f in ipairs(freqlist) do
            if dev.band == '2g' then
                -- 2.4GHz band
                if f.mhz >= 2412 and f.mhz <= 2472 then
                    freqs[#freqs + 1] = f.mhz
                end
            elseif dev.band == '5g' then
                -- 5GHz band
                if f.mhz >= 5180 and f.mhz <= 5825 then
                    freqs[#freqs + 1] = f.mhz
                end
            elseif dev.band == '6g' then
                -- 6GHz band
                if f.mhz >= 5955 and f.mhz <= 7115 then
                    freqs[#freqs + 1] = f.mhz
                end
            end
        end
    end

    return freqs
end

local function nl80211_scan_trigger(dev, ifname, ssids)
    local ok, err

    if file.access('/lib/wifi/qcacld32.sh') then
        -- For qcacld32, need to set freqs to scan the band
        local freqs = get_freq_list(dev)
        ok, err = nl80211.scan('trigger', { ifname = ifname, ssids = ssids, freqs = freqs })
    else
        ok, err = nl80211.scan('trigger', { ifname = ifname, ssids = ssids })
    end

    if not ok then
        log.err(ifname, ': trigger scan fail:', err)
        return false
    end

    local ifindex = socket.if_nametoindex(ifname)

    ok, err = nl80211.wait_event('scan', 35.0, function(cmd, attrs)
        if cmd == nl80211.CMD_SCAN_ABORTED or cmd == nl80211.CMD_NEW_SCAN_RESULTS then
            if nl.attr_get_u32(attrs[nl80211.ATTR_IFINDEX]) == ifindex then
                if cmd == nl80211.CMD_SCAN_ABORTED then
                    return false, 'aborted'
                end

                if cmd == nl80211.CMD_NEW_SCAN_RESULTS then
                    return true
                end
            end
        end
    end)

    if not ok then
        log.err(ifname, ': wait scan done fail:', err)
        return false
    end

    return true
end

local function is_device_setup(s)
    local sid = s['.name']

    if s.type == 'qcawificfg80211' or s.type == 'qcacld32' then
        if file.access('/sys/class/net/' .. sid .. '/flags') then
            if file.readfile('/sys/class/net/' .. sid .. '/flags', 'n') % 2 ~= 1 then
                return false
            end
        end
    end

    return true
end

local function is_iface_setup(dev, s)
    local is_up

    if dev.type == 'qcawificfg80211' then
        local function get_vif_state(ifname, cmd)
            local p = sys.exec('/bin/sh', '-c', 'cfg80211tool ' .. ifname .. ' ' .. cmd)
            local iface_stat = p:read_stdout('*a')
            return iface_stat:match(cmd .. ':(%d+)')
        end

        if not file.access('/lib/firmware/IPQ5332') then return true end

        is_up = get_vif_state(s.ifname, 'g_is_vdev_up')
        if s.mld then
            local c = uci.cursor()
            for iface in c:each('wireless', 'wifi-iface') do
                if iface.mode == 'ap' and iface.disabled ~= '1' then
                    local info = iwinfo.info(iface.ifname)
                    if info and info.ssid == '' then
                        return false
                    end

                    is_up = get_vif_state(iface.ifname, 'g_is_vdev_up')
                    local acs_runing = get_vif_state(iface.ifname, 'get_acs_state')

                    if acs_runing == '1' or is_up == '0' then
                        return false
                    end
                end
            end
            c:close()
        end
    end

    return is_up == '1'
end

local function vifs_setup(dev)
    local c = uci.cursor()

    local function vif_setup(ifname)
        sys.exec('sh', '-c', 'wpa_cli -g /var/run/hostapd/global raw REMOVE ' .. ifname):wait()
        sys.exec('sh', '-c', 'wpa_cli -g /var/run/hostapd/global raw ADD bss_config=' .. ifname .. ':/var/run/hostapd-' .. ifname .. '.conf'):wait()
    end

    local function skip_cac(band)
        if band == '5g' then
            local c = uci.cursor()
            for s in c:each('wireless', 'wifi-iface') do
                if s.disabled ~= '1' and s.mode == 'ap' and c:get('wireless', s.device, 'band') == band then
                    sys.exec('sh', '-c', 'cfg80211tool ' .. s.ifname .. ' set_cactimeout -2'):wait()
                end
            end
        end
    end

    for s in c:each('wireless', 'wifi-iface') do
        if s.disabled ~= '1' then
            if s.device == dev.sid or s.mld then
                vif_setup(s.ifname)
            end
        end
    end

   c:close()

   time.at(2, function() skip_cac(dev.band) end)
end

local function repeater_iface_delete(dev)
        local guest_disabled = false
        local iot_disabled = false
        local last_repeater_disconnect = true
        local c = uci.cursor()
        -- if guest ap enabled, no need to delete it
        if dev.band == '2g' then
            guest_disabled = c:get('wireless', 'guest2g', 'disabled') ~= '0'
            iot_disabled = c:get('wireless', 'iot2g', 'disabled') ~= '0'
        elseif dev.band == '5g' then
            guest_disabled = c:get('wireless', 'guest5g', 'disabled') ~= '0'
            iot_disabled = c:get('wireless', 'iot5g', 'disabled') ~= '0'
        elseif dev.band == '6g' then
            guest_disabled = c:get('wireless', 'guest6g', 'disabled') ~= '0'
            iot_disabled = c:get('wireless', 'iot6g', 'disabled') ~= '0'
        end
        local rep = ubus.call('repeater', 'status')
        if rep.state ~= 0 and rep.device == dev.sid then
            last_repeater_disconnect = false
        end
        if (guest_disabled or iot_disabled) and last_repeater_disconnect and is_device_setup(dev.iface) then
            sys.exec('ifconfig', dev.iface, 'down'):wait()
            sys.exec('iw', dev.iface, 'del'):wait()
        end
end

function M.scan(dev, ssid, extra_scan_ssid)
    local ifname = dev.iface
    local rd_mode

    --rt1500 use wlan0/wlan1 to scan
    if file.access('/lib/wifi/rtk_wifi.sh') then
        if ifname == 'wlan0-vxd' then
            ifname = 'wlan0'
        elseif ifname == 'wlan1-vxd' then
           ifname = 'wlan1'
        end
    end

    if dev.type == 'qcawificfg80211' then
        local iface_status
        local c = uci.cursor()
        for s in c:each('wireless', 'wifi-iface') do
            if s.device == dev.sid and s.mode == 'ap' and s.disabled ~= '1' then
                iface_status = is_iface_setup(dev, s)
                if not iface_status then return nil end
                if dev.band == '5g' then
                    local p = sys.exec('/bin/sh', '-c', 'cfg80211tool ' .. s.ifname .. ' get_cac_state')
                    local cac_stat = p:read_stdout('*a')
                    rd_mode = cac_stat:match('get_cac_state:(%d+)')
                end

                if iface_status and rd_mode ~= '1' then
                    ifname = s.ifname
                end
            end
        end

        c:close()

        if ifname == dev.iface then
            if not create_iface(dev) then
                return nil
            end
        end
    else
        if not create_iface(dev) then
            return nil
        end
    end

    local ssids = { '' }

    if ssid then
        ssids[#ssids + 1] = ssid
    end

    if extra_scan_ssid then
        ssids[#ssids + 1] = extra_scan_ssid
    end

    if not nl80211_scan_trigger(dev, ifname, ssids) then
        if dev.type == 'qcacld32' then
            repeater_iface_delete(dev)
        end
        return nil
    end

    local survey = nl80211_get_scanresult(dev, ifname)
    if not survey then
        if dev.type == 'qcacld32' then
            repeater_iface_delete(dev)
        end
        return nil
    end

    -- For ath11k, if more vap enabled, need a passive scan to get more complete ap list
    if file.access('/lib/firmware/ath11k') then
        local nap = 0
        local c = uci.cursor()
        c:foreach('wireless', 'wifi-iface', function(s)
            if s.device == dev.sid and s.mode == 'ap' and s.disabled ~= '1' then
                nap = nap + 1
            end
        end)
        c:close()

        if nap < 2 then
            return survey
        end

        log.info(dev.sid, 'do passive scan for ath11k')

        if not nl80211_scan_trigger(dev, ifname, ssids) then
            return nil
        end

        local pasive_survey = nl80211_get_scanresult(dev, ifname)
        if not pasive_survey then
            return survey
        end

        local survey_bssid = {}

        for _, bss in ipairs(survey) do
            survey_bssid[bss.bssid] = true
        end

        for _, bss in ipairs(pasive_survey) do
            if not survey_bssid[bss.bssid] then
                survey[#survey + 1] = bss
            end
        end
    end

    if dev.type == 'qcawificfg80211' and ifname == dev.iface then
        local status =  wpa_get_status(dev)
        if status['wpa_state'] == nil or status['wpa_state'] ~= 'COMPLETED' then
            sys.exec('iw', dev.iface, 'del'):wait()
            -- qsdk wifi driver will down primary iface when sta iface scan
            -- in cac mode and now setup it
            if rd_mode == '1' then vifs_setup(dev) end
        end
    elseif dev.type == 'qcacld32' then
        repeater_iface_delete(dev)
    end

    return survey
end

local function detect_phy_mode(linkinfo)
    local info = linkinfo or {}
    local t = info.tx_rate or info.rx_rate or {}

    if t.eht then
        return "EHT"   -- WiFi 7
    elseif t.he then
        return "HE"    -- WiFi 6/6E
    elseif t.vht then
        return "VHT"   -- WiFi 5
    end

    return nil
end

local function get_sta_interface_info(interface)
    local info = {}
    local bsta_info, err = nl80211.get_interface(interface)

    if not bsta_info then
        log.err("get interface info err:", err)
        return
    end

    if bsta_info.freq then info.channel = nl80211.freq_to_channel(bsta_info.freq) end
    info.ssid = bsta_info.ssid
    if bsta_info.channel_width then info.htmode =  nl80211.width_name(bsta_info.channel_width) end

    local stations, err = nl80211.get_stations(interface)
    if not stations or #stations == 0 then
        log.err("get stations err:", err)
        return
    end
    info.signal = stations[1].signal

    local linkinfo = nl80211.get_link(interface)
    local phy_mode = detect_phy_mode(linkinfo)

    if phy_mode then
        info.htmode = phy_mode..info.htmode
        info.htmode = info.htmode:match("^(%u+%d+)")
    end

    return info
end

function M.get_status(dev)
    local s = {}
    local info

    local ws = wpa_get_status(dev)
    if not ws then
        return nil
    end

    s.wpa_status = ws
    s.macaddr = ws.address
    s.bssid = ws.bssid

    -- For qcacld32, iwinfo takes a long time to obtain STA interface information
    -- so we use iw command to get it
    if file.access('/lib/wifi/qcacld32.sh') then
        info = get_sta_interface_info(dev.iface)
    else
        info = iwinfo.info(dev.iface)
    end
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
    local idx = sid:match('%d')
    local phyname
    local staname
    local n = 0

    if s.type == 'qcacld32' then
        -- For qcacld32, iface name is fixed wlan0-wlan5
        -- so the guest interface and the sta interface share a common interface name as wlan3-wlan5
        if s.band == '2g' then
            return 'wlan0', 'wlan3'
        elseif s.band == '5g' then
            return 'wlan0', 'wlan4'
        elseif s.band == '6g' then
            return 'wlan0', 'wlan5'
        end
    elseif file.access('/lib/wifi/rtk_wifi.sh') then
        if s.band == '2g' then
            return 'phy1', 'wlan0-vxd'
        elseif s.band == '5g' then
            return 'phy0', 'wlan1-vxd'
        end
    else
        staname = 'sta' .. idx
    end

    repeat
        local info, err = iwinfo.info(sid)
        if not info or not is_device_setup(s) then
            log.err('iwinfo: get info of', sid, 'fail:', err)
            n = n + 1
            time.sleep(3)
        else
            phyname = info.phyname
        end
    until phyname or n > 40

    return phyname, staname
end

function M.bypass_cac(dev)
    if dev.band == '5g' then
        if dev.type == 'qcawificfg80211' then
            local c = uci.cursor()

            if not file.access('/lib/firmware/IPQ5332') then return end
            for s in c:each('wireless', 'wifi-iface') do
                if s.disabled ~= '1' and s.mode == 'ap' and c:get('wireless', s.device, 'band') == dev.band then
                    sys.exec('sh', '-c', 'cfg80211tool ' .. s.ifname .. ' set_cactimeout -2'):wait()
                end
            end
        elseif file.access('/lib/wifi/rtk_wifi.sh') then
            local c = uci.cursor()
            for s in c:each('wireless', 'wifi-iface') do
                if s.disabled ~= '1' and s.mode == 'ap' and c:get('wireless', s.device, 'band') == dev.band then
                    local cac_path = '/proc/net/rtk_wifi6/' .. s.ifname .. '/dfs_cac_time'
                    if file.access(cac_path) then
                        log.info('rtk bypass cac for ' .. s.ifname)
                        file.writefile(cac_path, '0')
                    end
                end
            end
            c:close()
        else
            local state = get_ap_status(dev)
            if state == 'DFS' then
                log.info('bypass cac for ' .. dev.phy)
                file.writefile('/sys/kernel/debug/ieee80211/' .. dev.phy .. '/bypass_cac', 'Y')
            end
        end
    end
end

function M.get_timeout_period()
    local timeout = 35

    return timeout
end

return M
