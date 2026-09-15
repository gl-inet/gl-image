-- Author: Jianhui Zhao <jianhui.zhao@gl-inet.com>

local link = require 'eco.ip'.link
local ubus = require 'eco.ubus'
local time = require 'eco.time'
local file = require 'eco.file'
local sync = require 'eco.sync'
local log = require 'eco.log'
local sys = require 'eco.sys'

local iwinfo = require 'iwinfo'
local uci = require 'uci'

local vendor = require 'gl.repeater-vendor'
local portal = require 'gl.repeater-portal'

local M = {}

local states = {
    DISCONNECTED = 0,
    DISABLED = 1,
    CONNECTING = 2,
    CONNECTED = 3,
    WAN_ENABLE = 4
}

local reconn_mode = {
    NONE       = 0,  -- No switching
    NORMAL     = 1,  -- default behavior( every 30 seconds )
    EXTENDED   = 2   -- Extended interval( every 5 minutes after 5 failed reconnect)
}

local BLACKLIST_TIMEOUT = 300  -- default blacklist expiry in seconds (5 minutes)

local function commit_sync(c, ...)
    for _, config in ipairs({...}) do
        c:commit(config)
    end

    eco.run(file.sync)
end

local function bss_to_dev(devices, bss)
    for _, dev in ipairs(devices) do
        if dev.sid == bss.device then
            return dev
        end
    end
end

local function bss_has_eap(bss)
    if vendor.bss_has_eap then
        local eap = vendor.bss_has_eap(bss)
        if eap ~= nil then
            return eap
        end
    end

    local wpa = bss.wpa
    local rsn = bss.rsn

    if wpa and wpa.auth_suites['802.1X'] then
        return true
    end

    if rsn and rsn.auth_suites['802.1X'] then
        return true
    end

    return false
end

local function matched_bss(self, bss, net)
    local cfg = self.cfg
    local caps = bss.caps

    local blacklist_ts = self.disabled_bss[bss.bssid]
    if blacklist_ts then
        local timeout = cfg.blacklist_timeout or BLACKLIST_TIMEOUT
        if sys.uptime() - blacklist_ts >= timeout then
            log.info('blacklist expired for bss', bss.bssid)
            self.disabled_bss[bss.bssid] = nil
        else
            log.info('skip disabled bss', bss.bssid)
            return false
        end
    end

    if net.disabled_bss and net.disabled_bss[bss.bssid] then
        log.info('skip disabled bss', bss.bssid)
        return false
    end

    if not caps['ESS'] then
        return false
    end

    if bss.ssid ~= net.ssid then
        return false
    end

    if net.bssid and bss.bssid ~= net.bssid then
        return false
    end

    if cfg.lock_band and bss.band ~= cfg.lock_band then
        return false
    end

    if not cfg.dfs and bss.dfs then
        log.err('skip bss with dfs channel: ', bss.ssid, bss.bssid, bss.channel)
        return false
    end

    local wpa = bss.wpa
    local rsn = bss.rsn

    if net.key then
        if not wpa and not rsn then
            return false
        end

        local eap = bss_has_eap(bss)

        if net.identity and not eap then return false end
        if not net.identity and eap then return false end
    elseif caps['PRIVACY'] then
        if rsn and rsn.auth_suites['OWE'] then
            return true
        else
            return false
        end
    end

    return true
end

local function find_best_bss(self, net)
    local matched = {}

    for _, dev in ipairs(self.devices) do
        for _, bss in ipairs(self.surveys[dev.sid] or {}) do
            if matched_bss(self, bss, net) then
                local signal = bss.signal
                local level = 5

                if signal >= -50 then
                    level = 1
                elseif signal >= -67 then
                    level = 2
                elseif signal >= -70 then
                    level = 3
                elseif signal >= -80 then
                    level = 4
                end

                if not matched[level] then
                    matched[level] = {}
                end

                matched[level][#matched[level] + 1] = bss
            end
        end
    end

    for level = 1, 5 do
        if matched[level] then
            local best

            for _, bss in ipairs(matched[level]) do
                if not best then
                    best = bss
                elseif bss.band == best.band then
                    if bss.signal > best.signal then
                        best = bss
                    end
                elseif bss.band == '6g' then
                    best = bss
                elseif bss.band == '5g' and best.band ~= '6g' then
                    best = bss
                end
            end

            return best
        end
    end

    return nil
end

local function collect_matched_bsses(self)
    local cfg = self.cfg
    local bsses = {}
    local cur_bss_num = 0

    for _, net in ipairs(cfg.nets) do
        if net.temporary_disabled then
            net.temporary_disabled = nil
        else
            local bss = find_best_bss(self, net)
            if bss then
                local signal = bss.signal
                local level = 5

                if signal >= -50 then
                    level = 1
                elseif signal >= -67 then
                    level = 2
                elseif signal >= -70 then
                    level = 3
                elseif signal >= -80 then
                    level = 4
                end

                bsses[level] = bsses[level] or {}

                cur_bss_num = cur_bss_num + 1
                bsses[level][#bsses[level] + 1] = { bss, net }
            end
        end
    end

    return bsses, cur_bss_num
end

local function find_best_bss_any(self)
    local bsses, cur_bss_num = collect_matched_bsses(self)

    if cur_bss_num < 1 then
        log.debug("release disabled BSS")
        self.disabled_bss = {}
        bsses = collect_matched_bsses(self)
    end

    for level = 1, 5 do
        if bsses[level] then
            local best

            for _, item in ipairs(bsses[level]) do
                local bss = item[1]

                if not best then
                    best = item
                elseif bss.band == best[1].band then
                    if bss.signal > best[1].signal then
                        best = item
                    end
                elseif bss.band == '5g' then
                    best = item
                end
            end
            return best[2], best[1]
        end
    end
end

local methods = {}

local function channel_is_dfs(ch)
    return ch > 48 and ch < 149
end

local function check_has_ath11ko()
    return file.access('/lib/modules/4.4.60/ath11k.ko') or file.access('/lib/modules/5.4.164/ath11k.ko')
end

local function enable_wifi(up, wifi_type)
    local c = uci.cursor()
    local ifname

    c:foreach('wireless', 'wifi-iface', function(s)
        if s.mode == 'ap' and s.disabled ~= '1' then
            if wifi_type == 'guest' and s.guest then
                ifname = s.ifname
            elseif wifi_type == 'iot' and s.iot then
                ifname = s.ifname
            end
        end
    end)

    if ifname then
        local action_text = up and 're-enable' or 'disable'
        local wifi_type_text = wifi_type == 'guest' and 'guest' or 'iot'
        log.info(action_text, wifi_type_text .. ' wifi due to scan on ath11k')
        sys.exec('/sbin/ifconfig', ifname, up and 'up' or 'down'):wait()
    end
end

local function enable_guest_wifi(up)
    enable_wifi(up, 'guest')
end

local function enable_iot_wifi(up)
    enable_wifi(up, 'iot')
end

local function do_scan(self, dev, wg)
    local cnt = 15
    local survey

    repeat
        survey = vendor.scan(dev, self.cfg.scan_ssid, self.cfg.extra_scan_ssid)
        if not survey then
            cnt = cnt - 1
            time.sleep(5)
        end
    until survey or cnt == 0

    if cnt == 0 then
        log.err(dev.iface, 'scan failed after multiple attempts')
    else
        for _, bss in ipairs(survey) do
            bss.band = dev.band
            bss.device = dev.sid
            if bss.band == '5g' then
                bss.dfs = channel_is_dfs(bss.channel)
            else
                bss.dfs = false
            end
        end

        self.surveys[dev.sid] = survey

        log.info(dev.iface .. ': found ' .. #survey .. ' networks')
    end

    if wg then
        wg:done()
    end
end

--[[
    Failover Mode:
    - If Ethernet is available, Repeater will not reconnect.
    - If Cellular/Tethering is available and its priority is HIGHER than Repeater:
        Same handling as Ethernet - Repeater will not reconnect.
    - If Cellular/Tethering is available but its priority is LOWER than Repeater:
        Repeater attempts reconnect every 30s. After 5 consecutive failures,
        reconnect interval increases to 5 minutes.

    Balance Mode:
    - If Ethernet is available, Repeater will not reconnect.
    - If Cellular/Tethering is available:
        Repeater attempts reconnect every 30s. After 5 consecutive failures,
        reconnect interval increases to 5 minutes.
    return:
    NONE - not reconnect
    NORMAL - reconnect every 30s(default.)
    EXTENDED - reconnect every 30s. After 5 consecutive failures, reconnect every 5 minutes.
--]]
local function check_reconnect_mode()
    local c = uci.cursor()
    local kmwan_mode = c:get('kmwan', 'global', 'mode')
    local repeater_metric = tonumber(c:get('kmwan', 'wwan', 'metric') or 0)

    local kmwan_cfg = {
        wan = nil,
        secondwan = nil,
        usbwan = nil,
        tethering = nil,
        modems = {}
    }

    local content = file.readfile('/proc/gl-kmwan/config')
    if not content then
        log.error('Failed to open /proc/gl-kmwan/config')
        return reconn_mode.NORMAL
    end

    for key, value in content:gmatch('(%S+):(%S+)') do
        if key == 'wan' or key == 'secondwan' or key == 'usbwan' or key == 'tethering' then
            kmwan_cfg[key] = {
                online = (value == 'online'),
                metric = tonumber(c:get('kmwan', key, 'metric') or 6)
            }
        elseif key:sub(1, 5) == 'modem' then
            -- preserve full modem name, push to array
            table.insert(kmwan_cfg.modems, {
                name = key,
                online = (value == 'online'),
                metric = tonumber(c:get('kmwan', key, 'metric') or 6)
            })
        end
    end

    --find modem online
    local function find_modem_online()
        for _, m in ipairs(kmwan_cfg.modems) do
            if m.online then return m end
        end
        return false
    end

    -- check metric
    local function check_metric(metric, repeater_metric, description)
        if (metric or 6) > repeater_metric then
            log.debug(description .. ' has lower priority than repeater network, return EXTENDED mode')
            return reconn_mode.EXTENDED
        else
            log.debug(description .. ' has higher priority than repeater network, return NONE mode')
            return reconn_mode.NONE
        end
    end
    -- Ethernet online -> do NOT switch
    if (kmwan_cfg.wan and kmwan_cfg.wan.online) or(kmwan_cfg.secondwan and kmwan_cfg.secondwan.online) or
        (kmwan_cfg.usbwan and kmwan_cfg.usbwan.online) then
        log.info('wan/secondwan/usbwan online')
        return reconn_mode.NONE
    end

    -- mode logic
    if kmwan_mode == 'balance' then
        -- balance behavior: if tethering or any modem online -> EXTENDED
        if (kmwan_cfg.tethering and kmwan_cfg.tethering.online) or find_modem_online() then
            log.info('in balance, tethering/modem online -> EXTENDED')
            return reconn_mode.EXTENDED
        end
    elseif kmwan_mode == 'failover' then
        local tmp_tether
        if kmwan_cfg.tethering and kmwan_cfg.tethering.online then
            tmp_tether = check_metric(kmwan_cfg.tethering.metric, repeater_metric, 'tethering network')
        end
        local modem = find_modem_online()
        local tmp_modem
        if modem then
            tmp_modem = check_metric(modem.metric, repeater_metric, 'cellular network')
        end
        -- failover behavior: if tethering or any modem online and priority > repeater -> NONE
        if tmp_tether == reconn_mode.NONE or tmp_modem == reconn_mode.NONE then
            return reconn_mode.NONE
        elseif tmp_tether == reconn_mode.EXTENDED or tmp_modem == reconn_mode.EXTENDED then
            return reconn_mode.EXTENDED
        end
    end

    log.debug('No matching conditions found, return NORMAL mode')
    return reconn_mode.NORMAL
end

function methods:scan(active, refresh)
    if self.scanning then
        log.err('already scanning')
        return false
    end

    if self.last_scan then
        local interval = 30

        if active then
            interval = 10
        end

        if refresh then
            interval = 3
        end

        local elapsed = sys.uptime() - self.last_scan
        if elapsed < interval then
            log.err('skip scan due to too frequent, use cache in', interval - elapsed .. ' seconds')
            return true
        end
    end

    local devices = self.devices

    log.info('scanning...', self.cfg.scan_ssid or '')

    if self.cfg.extra_scan_ssid then
        log.info('extra scan ssid: ', self.cfg.extra_scan_ssid)
    end

    self.scanning = true

    if check_has_ath11ko() then
        enable_guest_wifi(false)
        enable_iot_wifi(false)
    end

    local first_dev = devices[1]
    --the original mtk factory recommends scanning separately to avoid unexpected problems.
    if first_dev.type == 'mtk' or first_dev.type == 'qcacld32' then
        for _, dev in ipairs(devices) do
            if not self.cfg.lock_band or self.cfg.lock_band == dev.band then
                do_scan(self, dev)
            end
        end
    else
        local wg = sync.waitgroup()
        if self.cfg.lock_band then
            wg:add(1)
        else
            wg:add(#devices)
        end
        for _, dev in ipairs(devices) do
            if not self.cfg.lock_band or self.cfg.lock_band == dev.band then
                eco.run(do_scan, self, dev, wg)
            end
        end
        wg:wait()
    end

    if check_has_ath11ko() then
        enable_guest_wifi(true)
        enable_iot_wifi(true)
    end

    self.scanning = false
    self.last_scan = sys.uptime()

    return true
end

function methods:disable_net(net)
    local c = uci.cursor()

    c:foreach('repeater', 'network', function(s)
        if s.ssid == net.ssid then
            c:set('repeater', s['.name'], 'disabled', 1)
        end
    end)

    commit_sync(c, 'repeater')
    c:close()

    local nets = self.cfg.nets

    for i, n in ipairs(nets) do
        if n.ssid == net.ssid then
            table.remove(nets, i)
        end
    end
end

local function select_next_bss(self, net)
    return find_best_bss(self, net)
end

local function try_next_bss(self, bss, net)
    if not net.disabled_bss then
        net.disabled_bss = {}
    end

    net.disabled_bss[bss.bssid] = true
    self.disabled_bss[bss.bssid] = sys.uptime()

    local try_bss = select_next_bss(self, net)
    if try_bss then
        return try_bss
    end

    net.temporary_disabled = true
    net.disabled_bss = nil
end

function methods:on_disconnect(bss, net, ctx)
    if not net then
        return
    end

    -- connect timeout requires re-scanning
    if self.fail_type == 'timeout' then
        self.fail_type = ''
        self.disabled_bss[bss.bssid] = sys.uptime()
    else
        local try_bss = try_next_bss(self, bss, net)
        if try_bss then
            self.state = states.CONNECTING
            ctx.try_bss = try_bss
            self.cond_status:signal()
            return
        end
    end

    if #self.cfg.nets == 1 then
        net.temporary_disabled = false
    end

    self:disconnect()
    self.cond_status:signal()
    self:switch(self.cfg.switch_interval)
    self:notify_status()
end

function methods:check_status(ctx)
    local timeout_period = 25
    if vendor.get_timeout_period then
        timeout_period = vendor.get_timeout_period()
    end

    local tmr_timeout = time.at(timeout_period, function()
        log.err('connect timeout')
        self.fail_type = 'timeout'
        self:on_disconnect(ctx.bss, self.net, ctx)
    end)

    local tmr_disconnect
    local net = self.net

    while self.cond_status:wait() do
        if tmr_disconnect then tmr_disconnect:cancel() end

        tmr_timeout:cancel()

        if self.state < states.CONNECTING then break end

        if self.pending then break end

        if ctx.try_bss then
            break
        end

        if ctx.key then
            local bss = ctx.bss

            log.err('disconnect from', net.ssid, bss.bssid, 'due to key error')

            if bss.key_err then
                log.err('disable bss', bss.bssid, 'due to key error 2 times')
                    self.disabled_bss[bss.bssid] = sys.uptime()
                    bss = select_next_bss(self, net)
            else
                bss.key_err = true
            end

            if bss then
                ctx.try_bss = bss
                break
            end

            self.fail_type = 'key'
            self:disconnect()
            self:notify_status()
            self:switch(self.cfg.switch_interval)
            break
        end

        if ctx.connected then
            self.state = states.CONNECTED
            self.connected = time.now()

            local network = net.network
            local dev = net.dev

            net.key_err = nil
            net.disabled_bss = nil

            if net.wds then
                log.info('add_device', dev.iface, 'to interface', network)
                link.set(dev.iface, { master = 'br-lan' })
            end

            self:update_selected(net)

            log.info('connected to', net.ssid, ctx.bss.bssid)
            self.connected_bssid = ctx.bss.bssid
            self.signal_monitor_tmr:set(3.0)

            self:notify_status()

            if network == 'wwan' then
                local delay = tonumber(self.cfg.ip_wait_time) or 15
                if delay < 1 then
                    delay = 1
                end
                log.info('[no-ip] schedule recover check delay:', delay, 'ip_wait_time:', self.cfg.ip_wait_time)
                self:schedule_no_ip_recover_check(delay)
                local s = ubus.call('network.interface.wwan', 'status')
                if s and s.up then
                    self:on_interface_up(network)
                end
            else
                self.switch_fail_cnt = 0
            end

        elseif ctx.disconnected then
            local bss = ctx.bss

            log.err('disconnected from', net.ssid, bss.bssid)

            tmr_disconnect = time.at(5, function()
                log.info('disconnected from', net.ssid, bss.bssid, 'confirmed')
                self:on_disconnect(bss, net, ctx)
            end)
        end

        if self.pending then break end
    end

    tmr_timeout:cancel()

    if tmr_disconnect then tmr_disconnect:cancel() end

    if ctx.sock then
        ctx.exit = true
        ctx.sock:close()
    end

    log.info('check status exited')

    local try_bss = ctx.try_bss
    if try_bss then
        ctx.try_bss = nil
        self:connect_bss(net, try_bss)
    end
end

local function valid_psk(key)
    local len = #key

    if len == 64 and key:match('^%x+$') then
        return true
    end

    return len > 7 and len < 64
end

local function set_nft_ttl(chain, ttl, hl, ifname)
    local dir = '/usr/share/nftables.d/chain-pre/mangle_' .. chain
    local path = dir .. '/01-set-ttl-portal.nft'

    if not file.access(dir) then
        os.execute('mkdir -p ' .. dir)
    end

    os.remove(path)

    if not ttl and not hl then
        return
    end

    local f, err = io.open(path, 'w')
    if not f then
        log.err('open', path, 'fail:', err)
        return
    end

    local inout = chain == 'prerouting' and 'i' or 'o'

    if ttl then
        f:write(string.format(inout .. 'ifname %s counter ip ttl set %d\n', ifname, ttl))
    end

    if hl then
        f:write(string.format(inout .. 'ifname %s counter ip6 hoplimit != 255 ip6 hoplimit set %d\n', ifname, hl))
    end

    f:close()
end

local function set_iptables_ttl_init(ttl, hl)
    local f, err = io.open('/etc/firewall-portal.user', 'w')
    if not f then
        log.err('open /etc/firewall-portal.user fail:', err)
        return false
    end

    local script = [[
clear_rules() {
    local ipt=$1
    local chain=$2
    local nums cnt

    nums=$($ipt -n -t mangle -L $chain --line-numbers | grep PORTAL_TTL | awk '{print $1}')
    cnt=$(echo $nums | wc -w)

    while [ $cnt -gt 0 ];
    do
        $ipt -t mangle -D $chain $cnt
        let cnt=cnt-1
    done
}

clear_rules iptables POSTROUTING
clear_rules iptables PREROUTING

iptables -t mangle -N PORTAL_TTL 2>/dev/null
iptables -t mangle -F PORTAL_TTL

[ -f /usr/sbin/ip6tables ] && {
    clear_rules ip6tables POSTROUTING
    clear_rules ip6tables PREROUTING

    ip6tables -t mangle -N PORTAL_TTL 2>/dev/null
    ip6tables -t mangle -F PORTAL_TTL
}
]]

    f:write(script)

    if ttl then
        f:write(string.format('iptables -t mangle -A PORTAL_TTL -j TTL --ttl-set %d\n', ttl))
    end

    if file.access('/usr/sbin/ip6tables') and hl then
        f:write(string.format('ip6tables -t mangle -A PORTAL_TTL -m hl ! --hl-eq 255 -j HL --hl-set %d\n', hl))
    end

    f:close()

    local c = uci.cursor()

    if not c:get('firewall', 'portal_ttl') then
        c:set('firewall', 'portal_ttl', 'include')
        c:set('firewall', 'portal_ttl', 'path', '/etc/firewall-portal.user')
        c:set('firewall', 'portal_ttl', 'reload', 1)
        commit_sync(c, 'firewall')
    end

    c:close()

    return true
end

local function set_iptables_ttl(chain, ttl, hl, ifname)
    local f, err = io.open('/etc/firewall-portal.user', 'a')
    if not f then
        log.err('open /etc/firewall-portal.user fail:', err)
        return
    end

    local inout = chain == 'PREROUTING' and 'i' or 'o'

    if ttl then
        f:write(string.format('iptables -t mangle -A %s -%s %s -j PORTAL_TTL\n', chain, inout, ifname))
    end

    if file.access('/usr/sbin/ip6tables') and hl then
        f:write(string.format('ip6tables -t mangle -A %s -%s %s -j PORTAL_TTL\n', chain, inout, ifname))
    end

    f:close()
end

local function set_ttl(ifname, net)
    local ttl = net.ttl
    local hl = net.hl

    if file.access('/sbin/fw4') then
        os.remove('/usr/share/nftables.d/chain-pre/mangle_prerouting/01-set-ttl-portal.nft')
        os.remove('/usr/share/nftables.d/chain-pre/mangle_postrouting/01-set-ttl-portal.nft')
    else
        set_iptables_ttl_init(ttl, hl)
    end

    file.writefile('/proc/gl-repeater/config', 'ifname=' .. (net.disguise and ifname or ''))

    if not net.disguise then
        if file.access('/sbin/fw4') then
            set_nft_ttl('postrouting', ttl, hl, ifname)
        else
            set_iptables_ttl('POSTROUTING', ttl, hl, ifname)
        end
    end

    sys.exec('/etc/init.d/firewall', 'reload')
end

local function ntp_is_valid()
    return file.access('/var/state/ntp-valid')
end

local function random_mac_expired(macaddr)
    if not ntp_is_valid() then
        return false
    end

    if type(macaddr.last_update_mac) ~= 'number' then
        return false
    end

    return os.time() - macaddr.last_update_mac > tonumber(macaddr.period) * 60
end

local function format_seconds(s)
    local day = math.floor(s / (24 * 3600))
    local hour = math.floor(s % (24 * 3600) / 3600)
    local min = math.floor(s % 3600 / 60)
    local sec = math.floor(s % 60)

    local fields = {}

    if day > 0 then
        fields[#fields + 1] = day .. 'd'
    end

    if hour > 0 then
        fields[#fields + 1] = hour .. 'h'
    end

    if min > 0 then
        fields[#fields + 1] = min .. 'm'
    end

    if sec > 0 then
        fields[#fields + 1] = sec .. 's'
    end

    return table.concat(fields, ',')
end

local function uci_set_net(net, conf)
    local c = uci.cursor()
    for s in c:each('repeater', 'network') do
        if s.ssid == net.ssid then
            local sid = s['.name']
            for k, v in pairs(conf) do
                c:set('repeater', sid, k, v)
            end
            break
        end
    end

    commit_sync(c, 'repeater')
    c:close()
end

local function net_macaddr_expired_tmr(_, net)
    local macaddr = net.macaddr

    macaddr.need_update_mac = true

    uci_set_net(net, { need_update_mac = 1 })

    log.info(net.ssid, macaddr.macaddr, 'expired, update it in next connection')
end

local function start_macaddr_expire_tmr(net)
    local macaddr = net.macaddr

    if type(macaddr.last_update_mac) == 'number' then
        local left = macaddr.period * 60 -  (os.time() - macaddr.last_update_mac)
        if left > 0 then
            log.info(net.ssid, macaddr.macaddr, 'will expire after', format_seconds(left))
            if net.tmr then net.tmr:cancel() end
            net.tmr = time.on(time.now() + left, net_macaddr_expired_tmr, net)
        end
    end
end

local function set_last_update_mac(net, write_uci)
    local macaddr = net.macaddr
    local mode = macaddr.mode
    local update = macaddr.update

    if mode ~= 'random' or update ~= 'time' then
        return
    end

    if ntp_is_valid() then
        macaddr.last_update_mac = os.time()
        start_macaddr_expire_tmr(net)
        log.info(net.ssid, net.macaddr, 'update "last_update_mac" to', macaddr.last_update_mac)
    else
        macaddr.last_update_mac = 'ntp'
        log.info(net.ssid, net.macaddr, 'update "last_update_mac" to ntp')
    end

    if write_uci then
        local conf = {
            need_update_mac = '',
            macaddr = 'r' .. macaddr.period .. ',' .. macaddr.macaddr
        }

        if type(macaddr.last_update_mac) == 'number' then
            conf.last_update_mac = macaddr.last_update_mac
        end

        uci_set_net(net, conf)
    end
end

function methods:connect_bss(net, bss)
    self.fail_type = ''

    net.dev = bss_to_dev(self.devices, bss)

    self:notify_status()

    local c = uci.cursor()

    local network = net.network

    if not net.wds then
        network = network or 'wwan'

        local proto = net.proto or 'dhcp'

        if vendor.delete_net_device then
            vendor.delete_net_device(network)
        end

        c:delete('network', network)

        c:set('network', network, 'interface')
        c:set('network', network, 'proto', proto)
        c:set('network', network, 'classlessroute', '0')

        local metric = c:get('kmwan', network, 'metric')
        c:set('network', network, 'metric', metric or '')

        if proto == 'static' then
            c:set('network', network, 'ipaddr', net.ipaddr)
            c:set('network', network, 'netmask', net.netmask)
            c:set('network', network, 'gateway', net.gateway)

            if net.dns  then
                c:set('network', network, 'dns', net.dns)
            end
        else
            c:set('network', network, 'hostname', net.hostname or '')
        end

        c:set('network', network, 'mtu', net.mtu or 1500)
    else
        network = network or 'lan'
    end

    net.network = network

    commit_sync(c, 'network')

    local dev = net.dev

    set_ttl(dev.iface, net)

    ubus.call('network', 'reload')

    if self.pending then
        log.info('aborted')
        return
    end

    local encryption = 'none'
    local wpa = bss.wpa
    local rsn = bss.rsn
    local eap = false
    local owe = false

    if rsn then
        local auth_suites = rsn.auth_suites

        if auth_suites['802.1X/SUITE-B-192'] then
            encryption = 'wpa3'
            eap = true
        elseif auth_suites['802.1X'] then
            encryption = 'wpa2'
            eap = true
        elseif auth_suites['SAE'] then
            encryption = 'sae'
        elseif auth_suites['PSK'] then
            encryption = 'psk2'
        elseif auth_suites['OWE'] then
            encryption = 'owe'
            owe = true
        end
    elseif wpa then
        local auth_suites = wpa.auth_suites
        if auth_suites['PSK'] then
            encryption = 'psk'
        elseif auth_suites['802.1X'] then
            encryption = 'wpa'
            eap = true
        end
    end
    eap = eap or bss_has_eap(bss)

    if not eap and not owe and (rsn or wpa) then
        if not valid_psk(net.key) then
            log.err('connecting to', net.ssid, 'fail, invalid psk')
            self.state = states.DISCONNECTED
            self.fail_type = 'key'
            self:disable_net(net)
            self:switch(self.cfg.switch_interval)
            return
        end
    end

    local macaddr = net.macaddr

    if type(macaddr) == 'table' then
        local expired = false

        if macaddr.need_update_mac then
            net.need_update_mac = nil
            expired = true
        elseif random_mac_expired(macaddr) then
            expired = true
        end

        if expired then
            macaddr.macaddr = M.generate_random_macaddr(6)
            log.info(net.ssid, 'macaddr changed to "' .. macaddr.macaddr .. '" due to',
                    net.need_update_mac and 'last' or 'this', 'expired')
            net.need_update_mac = nil
        end

        if macaddr.mode == 'random' and macaddr.update == 'time' then
            if expired or not macaddr.last_update_mac then
                set_last_update_mac(net, true)
            end
        end
    else
        macaddr = net.macaddr
    end

    if type(macaddr) == 'table' then
        macaddr = macaddr.macaddr
    end

    macaddr = macaddr or self.cfg.macaddr

    log.info('connecting to bss:', bss.ssid, bss.bssid, bss.channel, encryption)

    if vendor.connect_bss then
        if not vendor.connect_bss(net, bss, encryption, macaddr, self) then
            if self.pending then
                log.info('aborted')
                return
            end
            self:disconnect()
            self:switch(self.cfg.switch_interval)
            return
        end
    else
        if file.access('/lib/wifi/qcacld32.sh') then
            local staname = c:get('wireless', 'sta', 'ifname')
            local devicename = c:get('wireless', 'sta', 'device')
            if staname and devicename then
                local cmd = string.format('/sbin/wifi multi_down %s %s', devicename, staname)
                sys.sh(cmd)
            end
        end
        c:delete('wireless', 'sta')
        c:set('wireless', 'sta', 'wifi-iface')
        c:set('wireless', 'sta', 'mode', 'sta')
        c:set('wireless', 'sta', 'ifname', dev.iface)
        c:set('wireless', 'sta', 'device', dev.sid)
        c:set('wireless', 'sta', 'network', network)
        c:set('wireless', 'sta', 'ssid', bss.ssid)
        c:set('wireless', 'sta', 'bssid', bss.bssid)
        c:set('wireless', 'sta', 'wds', net.wds and 1 or '')
        c:set('wireless', 'sta', 'macaddr', macaddr or '')
        if file.access('/lib/wifi/qcacld32.sh') then
            c:set('wireless', 'sta', 'extap', 1)
        end
        if eap then
            c:set('wireless', 'sta', 'eap_type', 'peap')
            c:set('wireless', 'sta', 'identity', net.identity)
            c:set('wireless', 'sta', 'password', net.key)
        elseif not owe and (rsn or wpa) then
            c:set('wireless', 'sta', 'key', net.key)
        end

        if encryption == 'sae' and (dev.type == 'qcawificfg80211' or dev.type == 'qcacld32') then
            c:set('wireless', 'sta', 'encryption', 'ccmp')
            c:set('wireless', 'sta', 'sae', '1')
        elseif encryption == 'owe' then
            c:set('wireless', 'sta', 'encryption', 'ccmp')
            c:set('wireless', 'sta', 'owe', '1')
        else
            c:set('wireless', 'sta', 'encryption', encryption)
        end

        commit_sync(c, 'wireless')

        if file.access('/lib/wifi/qcacld32.sh') then
            if vendor.switch_mutex_band then
                vendor.switch_mutex_band(c, dev)
            end
            -- up repeater sta interface
            local cmd = string.format('/sbin/wifi multi_up %s %s', dev.sid, dev.iface)
            sys.sh(cmd)
            -- for E5800,need to wake up wifi
            ubus.call("lpm", "change_wifi_config", {})
        end
        ubus.call('network', 'reload')

        if self.pending then
            log.info('aborted')
            return
        end

        time.sleep(2)

        if self.pending then
            log.info('aborted')
            return
        end

        vendor.bypass_cac(dev)
    end

    c:close()

    local ctx = { dev = dev, network = network, bss = bss, net = net}

    if not vendor.listen_event(ctx, self.cond_status) then
        if self.pending then
            log.info('aborted')
            return
        end

        log.err('disconnect from', net.ssid, bss.bssid, 'due to connection failure')
        -- Connection failed twice in a row. Temporarily disable this BSSID.
        if self.last_failed_bssid == bss.bssid then
            log.err('disable bss', bss.bssid, 'due to connect bss fails 2 times')
            self.disabled_bss[bss.bssid] = sys.uptime()
        end
        self.last_failed_bssid = bss.bssid

        self:disconnect()
        self:switch(self.cfg.switch_interval)
        return
    end

    self.last_failed_bssid = nil
    self:check_status(ctx)
end

function methods:switch_to_net(net)
    log.info('switch to', net.ssid)

    if self.cfg.auto then self.selected_net = nil end

    self.net = net
    self.pending = nil

    local bss = find_best_bss(self, net)
    if not bss then
        log.info('not found matched bss for', net.ssid)
        self.fail_type = 'not-found'
        self:disconnect()
        self.disabled_bss = {}
        self:switch(self.cfg.switch_interval)
        return
    end

    self:connect_bss(net, bss)
end

function methods:switch_to_any()
    log.info('switch to any...')

    local net, bss = find_best_bss_any(self)
    if not net then
        self.fail_type = 'not-found'
        self.state = states.DISCONNECTED
        log.info('not found matched bss')
        self.disabled_bss = {}
        self:switch(self.cfg.switch_interval)
        return
    end

    self.net = net

    self:connect_bss(net, bss)
end

function methods:find_bss_by_net(net)
    for _, dev in ipairs(self.devices) do
        for _, bss in ipairs(self.surveys[dev.sid] or {}) do
            if bss.ssid == net.ssid then
                return bss
            end
        end
    end
end

function methods:update_selected(net)
    local c = uci.cursor()

    c:foreach('repeater', 'network', function(s)
        if s.ssid == net.ssid then
            c:set('repeater', s['.name'], 'selected', 1)
        else
            c:delete('repeater', s['.name'], 'selected')
        end
    end)

    commit_sync(c, 'repeater')
    c:close()
end

function methods:save_config(net)
    net = net or self.net

    if not net then
        return
    end

    local nets = self.cfg.nets
    local c = uci.cursor()

    for i, n in ipairs(nets) do
        if n.ssid == net.ssid then
            if n.tmr then n.tmr:cancel() end
            table.remove(nets, i)
            break
        end
    end

    local portal_fields = {}
    local portal_keys = {
        'portal_auth_mode',
        'portal_password',
        'portal_username',
        'portal_voucher',
        'portal_one_click'
    }
    for s in c:each('repeater', 'network') do
        if s.ssid == net.ssid then
            for _, key in ipairs(portal_keys) do
                portal_fields[key] = s[key] or ''
            end
            c:delete('repeater', s['.name'])
        end
    end

    local sid = c:add('repeater', 'network')
    c:set('repeater', sid, 'ssid', net.ssid)
    c:set('repeater', sid, 'identity', net.identity or '')
    c:set('repeater', sid, 'key', net.key or '')
    c:set('repeater', sid, 'proto', net.proto or '')
    c:set('repeater', sid, 'ipaddr', net.ipaddr or '')
    c:set('repeater', sid, 'netmask', net.netmask or '')
    c:set('repeater', sid, 'gateway', net.gateway or '')
    c:set('repeater', sid, 'dns', net.dns or '')
    c:set('repeater', sid, 'mtu', net.mtu or '')
    c:set('repeater', sid, 'ttl', net.ttl or '')
    c:set('repeater', sid, 'hl', net.hl or '')
    c:set('repeater', sid, 'network', net.network or '')
    c:set('repeater', sid, 'bssid', net.bssid or '')
    c:set('repeater', sid, 'wds', net.wds and 1 or '')
    c:set('repeater', sid, 'auto_portal', net.auto_portal and 1 or 0)
    c:set('repeater', sid, 'disguise', net.disguise and 1 or 0)
    c:set('repeater', sid, 'hostname', net.hostname or '')

    local macaddr = net.macaddr or ''

    if type(macaddr) == 'table' then
        local prefix

        if macaddr.mode == 'clone' then
            prefix = 'c'
        elseif macaddr.mode == 'random' then
            prefix = 'r'
            if macaddr.update == 'reboot' then
                prefix = prefix .. 'r'
            elseif macaddr.update == 'time' then
                prefix = prefix .. macaddr.period
                c:set('repeater', sid, 'last_update_mac', macaddr.last_update_mac or '')
            end
        end

        if prefix then
            macaddr = prefix .. ',' .. macaddr.macaddr
        else
            macaddr = ''
        end
    end

    c:set('repeater', sid, 'macaddr', macaddr or '')

    for _, key in ipairs(portal_keys) do
        if portal_fields[key] and portal_fields[key] ~= '' then
            c:set('repeater', sid, key, portal_fields[key])
        end
    end

    nets[#nets + 1] = net

    local scan_ssid = self.cfg.scan_ssid

    for s in c:each('repeater', 'network') do
        if s.ssid == scan_ssid then
            c:set('repeater', s['.name'], 'scan', 1)
        else
            c:delete('repeater', s['.name'], 'scan')
        end
    end

    commit_sync(c, 'repeater')
    c:close()

    self:update_selected(net)
end

function M.generate_random_macaddr(cnt)
    local nums = {}

    cnt = cnt or 6

    for i = 1, cnt do
        local n = math.random(1, 255)

        -- the 2th byte should be 0x2, 0x6, 0xa, 0xe
        if i == 1 then
            n = n & 0xfe | 0x2
        end

        nums[i] = string.format('%02X', n)
    end

    return table.concat(nums, ':')
end

local function random_mac_changed(self, ssid, macaddr)
    local nets = self.cfg.nets

    for _, net in ipairs(nets) do
        if net.ssid == ssid then

            if type(net.macaddr) ~= 'table' then
                return true
            end

            if net.macaddr.mode ~= macaddr.mode or net.macaddr.update ~= macaddr.update then
                return true
            end

            local changed = net.macaddr.macaddr ~= macaddr.macaddr

            if not changed then
                macaddr.need_update_mac = net.macaddr.need_update_mac
            end

            return changed
        end
    end
end

function methods:on_ntp_valid(init)
    local nets = self.cfg.nets

    log.info('ntp been valid')

    local c = uci.cursor()

    for _, net in ipairs(nets) do
        local macaddr = net.macaddr

        if type(macaddr) == 'table' and macaddr.mode == 'random' and macaddr.update == 'time' then
            if not macaddr.need_update_mac then
                if macaddr.last_update_mac == 'ntp' or not macaddr.last_update_mac then
                    set_last_update_mac(net)
                else
                    if init then
                        start_macaddr_expire_tmr(net)
                    elseif random_mac_expired(macaddr) then
                        macaddr.need_update_mac = true
                        log.info(net.ssid, macaddr.macaddr, 'expired, update it in next connection')
                    end
                end
            end

            uci_set_net(net, {
                last_update_mac = macaddr.last_update_mac or '',
                need_update_mac = macaddr.need_update_mac and '1' or ''
            })
        end
    end

    commit_sync(c, 'repeater')
    c:close()
end

function methods:switch_to_new(net)
    net.macaddr = net.macaddr or { mode = 'default' }

    local cfg = self.cfg
    for _, n in ipairs(cfg.nets) do
        n.disabled_bss = nil
        n.temporary_disabled = nil
    end

    self.selected_net = net
    self.pending = true
    self.last_failed_bssid = nil
    portal.stop_detect()
    portal.exit_bare_mode()

    self.cond_status:signal()

    while self.running do
        time.sleep(0.1)
    end

    self.fail_type = ''
    self.net_bak = self.net
    self.net = nil

    local c = uci.cursor()
    c:set('repeater', '@main[0]', 'disabled', 0)
    commit_sync(c, 'repeater')

    cfg.disabled = false

    if not net.network then
        net.network = net.wds and 'lan' or 'wwan'
    end

    self.disabled_bss = {}

    local found = self:find_bss_by_net(net)

    if not found or net.manual then
        cfg.scan_ssid = net.ssid
    end

    self.skip_scan = nil

    if found then
        self.skip_scan = true
    else
        -- do a force scan
        self.last_scan = nil
    end

    local macaddr = net.macaddr

    if net.disguise then
        net.macaddr = macaddr:sub(1, 9) .. M.generate_random_macaddr(3)
        log.info('using disguise macaddr:', net.macaddr, 'hostname:', net.hostname or '')
    else
        if macaddr.mode == 'clone' then
            log.info('using clone macaddr:', macaddr.macaddr, 'hostname:', net.hostname or '')
        elseif macaddr.mode == 'default' then
            log.info('using default macaddr:', cfg.macaddr)
        else
            if macaddr.update == 'time' then
                if random_mac_changed(self, net.ssid, macaddr) then
                    log.info('random macaddr changed by user')
                    set_last_update_mac(net)
                end
            end

            log.info('using random macaddr:', macaddr.macaddr)
         end
    end

    if net.remember then
        self:save_config(net)
    end

    self:switch(1)
end

function methods:disconnect()
    if self.no_ip_check_tmr then
        self.no_ip_check_tmr:cancel()
    end

    self.state = states.DISCONNECTED
    self.disconnecting = true
    self.skip_scan = nil
    self.pending = nil

    -- use cache for scan during disconnecting
    time.at(5, function() self.disconnecting = nil end)

    portal.stop_detect()
    portal.exit_bare_mode()

    local c = uci.cursor()

    c:set('wireless', 'sta', 'disabled', 1)
    commit_sync(c, 'wireless')

    local mode = c:get('glconfig', 'general', 'mode')

    if mode == 'wds' or mode == 'relay' then
        if self.connected then
            self.connected = nil
            sys.exec('/etc/init.d/network', 'restart'):wait()
        else
            ubus.call('network', 'reload')
        end
    else
        ubus.call('network', 'reload')
    end

    local device = c:get('wireless', 'sta', 'device')
    local device_type = device and c:get('wireless', device, 'type') or ""
    local net = self.net

    if device_type == "mtkwifi" then
        net = self.net and self.net or self.net_bak
    end

    if net then
        net.disabled_bss = nil
        net.temporary_disabled = nil
        if vendor.disconnect then
            vendor.disconnect(net, self)
        end
        self.net_bak = nil
    end

    c:close()
end

function methods:disable()
    local c = uci.cursor()
    c:set('repeater', '@main[0]', 'disabled', 1)
    commit_sync(c, 'repeater')

    self.cfg.disabled = true

    self:disconnect()
    self.cond_status:signal()
    if self.state == states.DISCONNECTED then
        self.state = states.DISABLED
        self.net = nil

        ubus.call('gl-session', 'notify', { name = 'repeater.status', data = { state = 0 } })
        log.info('disabled by user')
    end

    c:close()
end

function methods:enable()
    local c = uci.cursor()
    c:set('repeater', '@main[0]', 'disabled', 0)
    commit_sync(c, 'repeater')
    c:close()

    self.cfg.disabled = false
    self:switch(3)
end

function methods:__run()
    local cfg = self.cfg

    if cfg.disabled then
        self.state = states.DISABLED
        return log.info('disabled')
    end

    if not self.selected_net then
        if cfg.auto then
            if #cfg.nets == 0 then
                log.info('no saved network')
                self:disconnect()
                self.state = states.DISABLED
                return
            end
        else
            log.info('no selected net and auto switch disabled, exit switch')
            self:disconnect()
            self.state = states.DISABLED
            return
        end
    end

    if self.cfg.smart_reconnect then
        --[[repeater_boot_reconn_flag:
            After a reboot, if the repeater was enable before reboot,
            it must perform one repeater operation upon restart.
        ]]--
        if not self.pending and file.access("/tmp/repeater_boot_reconn_flag") then
            self.reconn_mode = check_reconnect_mode()
            if self.reconn_mode == reconn_mode.NONE then
                self.state = states.WAN_ENABLE
                return log.info('temporarily stop switching due to WAN/modem/tethering is online.')
            end
        end
        if not file.access("/tmp/repeater_boot_reconn_flag") then
            os.execute("touch /tmp/repeater_boot_reconn_flag")
        end
    end

    self.state = states.CONNECTING

    self:notify_status()

    if not self.skip_scan then
        if not self:scan() then
            return self:switch(3)
        end
    end

    self.skip_scan = nil

    if self.state == states.DISABLED then
        return log.info('switch breaked')
    end

    if self.scanning then
        log.info('wait scan done...')
        return self:switch(2)
    end

    -- last connected before reboot or new to connect
    if self.selected_net then
        self:switch_to_net(self.selected_net)
    elseif cfg.auto then
        self:switch_to_any()
    end
end

function methods:run()
    while true do
        self.running = false
        self:notify_status()
        self.cond_switch:wait()
        self.running = true
        self:__run()
    end
end

function methods:switch(delay)
    delay = delay or 0

    -- Enable Smart Reconnect and switch to interval usage with the default configuration interval of 30 seconds.
    if self.cfg.smart_reconnect and delay > 5 then
        self.switch_fail_cnt = self.switch_fail_cnt + 1
        log.debug('switch count:', self.switch_fail_cnt)
        if self.reconn_mode == reconn_mode.EXTENDED and self.switch_fail_cnt >= 5 then
            log.debug('switch delay changed to 5 minutes')
            delay = 300
        end
    end
    log.info('switch in', delay, 'seconds...')

    self.switch_tmr:set(delay)
end

function methods:start()
    local cfg = self.cfg

    self:notify_status(true)

    self.switch_fail_cnt = 0
    if #cfg.nets == 0 then
        self.reconn_mode = reconn_mode.NORMAL
        log.info('no saved network')
        return
    end

    self:switch()
end

function methods:should_use_cache_for_scan()
    return self.state == states.CONNECTING or self.disconnecting
end

function methods:update_config(cfg)
    if not cfg.auto then
        self.selected_net = cfg.selected_net
    end

    local nets = self.cfg.nets

    for _, n in ipairs(nets) do
        if n.tmr then
            n.tmr:cancel()
        end
    end

    self.cfg = cfg
end

function methods:get_network()
    local net = self.net

    if not net then return nil end

    local network = net.network
    if not network then
        network = net.wds and 'lan' or 'wwan'
    end
    return network
end

local function has_ipv4(interface)
    local status = ubus.call('network.interface.' .. interface, 'status')
    if not status or not status.up then
        return false
    end

    local ipv4 = status['ipv4-address']
    return ipv4 and ipv4[1] and ipv4[1].address
end

local function no_ip_get_recover_target(self, source)
    if self.state ~= states.CONNECTED then
        log.debug('skip ' .. source .. ', not connected')
        return nil
    end

    local current = self:get_network()
    if not current then
        log.debug('skip ' .. source .. ', network unavailable')
        return nil
    end

    if has_ipv4(current) then
        log.debug('[no-ip] skip ' .. source .. ', ipv4 already acquired on', current)
        self.switch_fail_cnt = 0
        return nil
    end

    return current
end

local function no_ip_trigger_recover(self, current)
    log.err('[no-ip] no ipv4 address on', current, 'reconnect to known bss')
    self:recover_from_no_ip()
end

function methods:schedule_no_ip_recover_check(delay)
    local check_delay = delay or 15
    log.debug('[no-ip] schedule recover check, delay:', check_delay)

    if not self.no_ip_check_tmr then
        self.no_ip_check_tmr = time.timer(function()
            log.debug('[no-ip] recover check fired, state:', self.state)

            local recover_target = no_ip_get_recover_target(self, 'no-ip recover check timer')
            if not recover_target then
                return
            end
            no_ip_trigger_recover(self, recover_target)
        end)
    end

    self.no_ip_check_tmr:set(check_delay)
end

local function build_ip_status(res)
    if res.state ~= 2 then return end

    local status = ubus.call('network.interface.' .. (res.network or 'wwan'), 'status')
    if not status or not status.up then return end

    res.ipv4 = {
        ip = status['ipv4-address'][1].address .. '/' .. status['ipv4-address'][1].mask,
        dns = status['dns-server']
    }

    for _, v in ipairs(status.route) do
        if v.target == '0.0.0.0' then
            res.ipv4.gateway = v.nexthop
            break
        end
    end

    local c = uci.cursor()
    local wwan6

    c:foreach('network', 'interface', function (s)
        if s.ifname == '@wwan' or s.device == '@wwan' then
            wwan6 = s['.name']
        end
    end)

    if not wwan6 then return end

    status = ubus.call('network.interface.' .. wwan6, 'status')
    if not status or not status.up then return end

    local ipaddrs = {}

    for _, v in ipairs(status['ipv6-address']) do
        ipaddrs[#ipaddrs + 1] = string.format('%s/%d', v.address, v.mask)
    end

    res.ipv6 = {
        ip = ipaddrs,
        dns = status['dns-server']
    }

    for _, v in ipairs(status.route) do
        if v.target == '::' then
            res.ipv6.gateway = v.nexthop
            break
        end
    end
end

function methods:is_connected()
    return not self.cfg.disabled and self.state == states.CONNECTED
end

local function detect_wifi_gen(s)
    s = string.upper(s or "")

    if s:find("^HE") then
        return "6"
    elseif s:find("^EHT") then
        return "7"
    elseif s:find("^VHT") then
        return "5"
    elseif s:find("^HT") then
        return "4"
    end

    return "Legacy"
end

function methods:get_status()
    if self.cfg.disabled then
        return { state = 0 }
    end

    local state_idle = 0
    local state_connecting = 1
    local state_connected = 2
    local state_failed = 3
    local wan_enable = 4

    local net = self.net

    local s = { fail_type = self.fail_type, running = self.running }

    if self.pending then
        s.state = state_connecting
        net = self.selected_net
    elseif self.state == states.DISABLED then
        s.state = state_idle
        return s
    elseif self.state == states.CONNECTED then
        s.state = state_connected
    elseif self.state == states.CONNECTING then
        s.state = state_connecting
    elseif self.state == states.WAN_ENABLE then
        s.state = wan_enable
    else
        s.state = state_failed
    end

    if not net then return s end

    local config = {
        remember = net.remember,
        ssid = net.ssid,
        bssid = net.bssid,
        key = net.key,
        identity = net.identity,
        protocol = net.proto,
        ip = net.ipaddr,
        netmask = net.netmask,
        gateway = net.gateway,
        dns = net.dns,
        mtu = net.mtu,
        ttl = net.ttl,
        ttl_ipv6 = net.hl,
        manual = net.manual or net.scan == '1',
        auto_portal = net.auto_portal,
        disguise = net.disguise
    }

    if not net.disguise then
        local macaddr = { mode = net.macaddr.mode or 'default' }
        local mode = macaddr.mode

        if mode ~= 'default' then
            macaddr.update = net.macaddr.update
            macaddr.macaddr = net.macaddr.macaddr

            if mode == 'random' and macaddr.update == 'time' then
                -- minute to hour
                macaddr.period = net.macaddr.period / 60
            end
        end

        config.macaddr = macaddr
    end

    s.config = config

    if self.pending or self.state ~= states.CONNECTED then
        return s
    end

    local dev = net.dev

    if not dev then return s end

    s.network = self:get_network()
    s.device = dev.sid

    local portal_status = portal.get_status()
    s.portal = portal_status.detected
    s.portal_detecting = portal_status.detecting
    s.portal_url = portal_status.portal_url
    s.bare_mode = portal_status.bare_mode

    local vs = vendor.get_status(dev)
    if not vs then
        s.state = state_connecting
        return s
    end

    for k, v in pairs(vs) do
        s[k] = v
    end

    local htmode = vs.htmode or ""
    local wifi_generation = (vs.wpa_status or {}).wifi_generation
    -- wpa_status Existing Wi-Fi generation Using Wi-Fi generation from wpa_status.
    if wifi_generation then
        s.wifi_generation = wifi_generation
    else
        s.wifi_generation = detect_wifi_gen(htmode)
    end

    if s.channel and dev.band == '5g' then
        s.dfs = channel_is_dfs(s.channel)
    else
        s.dfs = false
    end

    local connected = time.now() - self.connected
    if connected < 0 then
        connected = 0
    end

    s.connected = format_seconds(connected)

    build_ip_status(s)

    return s
end

function methods:get_portal_info()
    local c = uci.cursor()
    local res = {
        auth_mode = 0,
        password = '',
        username = '',
        voucher = ''
    }

    local net = self.net
    if not net or not net.ssid then
        c:close()
        return res
    end

    local sid
    c:foreach('repeater', 'network', function(section)
        if section.ssid == net.ssid then
            if net.bssid and net.bssid ~= '' and section.bssid and section.bssid ~= '' and section.bssid ~= net.bssid then
                return
            end
            sid = section['.name']
            return false
        end
    end)

    if not sid then
        c:close()
        return res
    end

    local auth_mode_raw = c:get('repeater', sid, 'portal_auth_mode')
    if auth_mode_raw and auth_mode_raw ~= '' then
        res.auth_mode = tonumber(auth_mode_raw) or 0
    end

    res.password = c:get('repeater', sid, 'portal_password') or ''
    res.username = c:get('repeater', sid, 'portal_username') or ''
    res.voucher = c:get('repeater', sid, 'portal_voucher') or ''

    local one_click = c:get('repeater', sid, 'portal_one_click')
    if one_click == '1' then
        res.one_click = true
    elseif one_click == '0' then
        res.one_click = false
    end

    c:close()
    return res
end

function methods:recover_from_no_ip()
    self.fail_type = 'no-ip'
    local bssid = self.connected_bssid

    log.debug('enter recover_from_no_ip, net:', self.net and self.net.ssid, 'bssid:', bssid)

    if bssid and self.cfg.auto then
        self.disabled_bss[bssid] = sys.uptime()
        log.info('[no-ip] disable current bss', bssid)
    end

    if self.cfg.scan_ssid then
        log.info('[no-ip] clear scan_ssid for recovery', self.cfg.scan_ssid)
        self.cfg.scan_ssid = nil
    end

    self:disconnect()
    self.cond_status:signal()
    self:switch(self.cfg.switch_interval)
    self:notify_status()
end

function methods:on_interface_up(name)
    if self.state ~= states.CONNECTED then
        return
    end

    local network = self:get_network()
    if not network then
        return
    end

    local network6 = network .. 6

    if network ~= name and network6 ~= name then
        return
    end

    self:notify_status()
    log.info('interface "' .. name .. '" up')

    if name ~= 'wwan' then
        return
    end

    if self.network_uping[name] then
        return
    end

    local net = self.net
    if not net then return end

    local dev = net.dev
    if not dev then return end

    local s = ubus.call('network.interface.wwan', 'status')
    if not s then
        return
    end

    self.network_uping[name] = true

    time.at(2, function()
        self.network_uping[name] = nil
    end)

    portal.detect(self, dev.iface, s['dns-server'], net.auto_portal)
end

function methods:on_kmwan_status_changed(interface, status, failover_chg)
    portal.on_kmwan_wwan_event(interface)

    if self.cfg.disabled or not self.cfg.smart_reconnect then
        return
    end

    if failover_chg then
        log.info('failover is changed. ')
        self:switch(3)
        return
    end

    if interface and interface ~= 'wan' and interface ~= 'secondwan' and
        interface ~= 'usbwan' and interface ~= 'tethering' and not string.find(interface, '^modem') then
        return
    end

    if self.state == states.CONNECTED then
        return
    end

    log.info('interface', interface, status)

    -- Only trigger reconnect when the interface transitions from online to offline,
    -- and the repeater is currently in WAN_ENABLE state.
    if status == 'offline' and self.state == states.WAN_ENABLE then
        self:switch(3)
    end
end

function methods:on_interface_down(name)
    if self.cfg.disabled then
        return
    end

    if name ~= 'wan' and name ~= 'secondwan' and name ~= 'usbwan' and name ~= 'tethering' and not string.find(name, '^modem') then
        return
    end

    if self.state == states.CONNECTED then
        return
    end

    log.info('interface', name, 'down')

    self:switch(3)
end

function methods:notify_status(bypass_disabled)
    if self.cfg.disabled and not bypass_disabled then
        return
    end

    ubus.call('gl-session', 'notify', { name = 'repeater.status', data = self:get_status() })
end

function methods:enter_bare_mode(client_macaddr)
    portal.enter_bare_mode(client_macaddr)
    self:notify_status()
end

function methods:exit_bare_mode()
    portal.exit_bare_mode()
    self:notify_status()
end

function M.new(cfg, devices)
    local o = {
        cfg = cfg,
        fail_type = '',
        running = false,
        devices = devices,
        surveys = {},
        disabled_bss = {},
        network_uping = {},
        selected_net = cfg.selected_net,
        state = states.DISABLED,
        cond_switch = sync.cond(),
        cond_status = sync.cond(),
        states = states
    }

    o.switch_tmr = time.timer(function()
        local ok
        repeat
            ok = o.cond_switch:signal()
            if not ok then
                time.sleep(0.2)
            end
        until ok
    end)

    o.signal_monitor_tmr = time.timer(function()
        if o.state ~= states.CONNECTED then return end

        local net = o.net
        if not net then return end

        local dev = net.dev
        if not dev then return end

        local info = iwinfo.info(dev.iface)
        local signal = info and info.signal

        if o.last_signal ~= signal then
            o.last_signal = signal
            o:notify_status()
        end
    end)

    setmetatable(o, { __index = methods })

    if ntp_is_valid() then
        o:on_ntp_valid(true)
    end

    return o
end
return setmetatable(M, { __index = vendor })
