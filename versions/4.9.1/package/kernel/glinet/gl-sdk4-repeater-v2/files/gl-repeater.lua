#!/usr/bin/eco

-- Author: Jianhui Zhao <jianhui.zhao@gl-inet.com>

local time = require 'eco.time'
local ubus = require 'eco.ubus'
local file = require 'eco.file'
local log = require 'eco.log'
local sys = require 'eco.sys'
local uci = require 'uci'

local vendor = require 'gl.repeater-vendor'
local repeater = require 'gl.repeater'

local function load_config()
    log.info('load repeater config...')

    local c = uci.cursor()

    local cfg = {}

    c:foreach('repeater', 'main', function(s)
        cfg.disabled = s.disabled == '1'
        cfg.auto = s.auto == '1'
        cfg.smart_reconnect = s.smart_reconnect == '1'
        cfg.switch_interval = tonumber(s.switch_interval or 30)
        cfg.ip_wait_time = tonumber(s.ip_wait_time or 15)
        cfg.lock_band = s.lock_band

        local blacklist_timeout = tonumber(s.blacklist_timeout or 300)
        if blacklist_timeout < 300 then blacklist_timeout = 300 end
        if blacklist_timeout > 3600 then blacklist_timeout = 3600 end
        cfg.blacklist_timeout = blacklist_timeout
        cfg.macaddr = s.macaddr

        local level = s.level
        if level then
            if level:match('%d+') ~= level then
                if type(log[level]) ~= 'number' then
                    log.err('invalid log level:', level)
                    level = log['INFO']
                end
            end

            log.set_level(tonumber(level))
        end

        if s.log_path then
            log.set_path(s.log_path)
        end
    end)

    local nets = {}

    c:foreach('repeater', 'network', function(s)
        if s.disabled == '1' then
            return
        end

        local disguise = s.disguise == '1'
        local macaddr = s.macaddr

        if not disguise then
            if macaddr then
                local prefix, real_macaddr = macaddr:match('^(%w+),(.+)')
                if prefix then
                    macaddr = {}

                    if prefix == 'c' then
                        macaddr.mode = 'clone'
                    else
                        macaddr.mode = 'random'

                        prefix = prefix:sub(2)

                        if prefix == 'r' then
                            macaddr.update = 'reboot'
                        elseif prefix == '' then
                            macaddr.update = 'none'
                        else
                            macaddr.update = 'time'
                            macaddr.period = tonumber(prefix)
                        end
                    end

                    macaddr.macaddr = real_macaddr

                    if macaddr.update == 'reboot' then
                        macaddr.macaddr = repeater.generate_random_macaddr(6)
                        c:set('repeater', s['.name'], 'macaddr', 'rr,' .. macaddr.macaddr)
                    end

                    local last_update_mac = s.last_update_mac

                    if last_update_mac and last_update_mac ~= 'ntp' then
                        last_update_mac = tonumber(last_update_mac)
                    end

                    macaddr.last_update_mac = last_update_mac

                    macaddr.need_update_mac = s.need_update_mac == '1'
                end
            end

            if not macaddr then
                macaddr = { mode = 'default' }
            end
        end

        local net = {
            remember = true,
            ssid = s.ssid,
            bssid = s.bssid,
            identity = s.identity,
            key = s.key,
            proto = s.proto,
            ipaddr = s.ipaddr,
            netmask = s.netmask,
            gateway = s.gateway,
            dns = s.dns,
            mtu = tonumber(s.mtu),
            ttl = tonumber(s.ttl),
            hl =  tonumber(s.hl),
            network = s.network,
            wds = s.wds == '1',
            auto_portal = s.auto_portal == '1',
            disguise = disguise,
            hostname = s.hostname,
            macaddr = macaddr
        }

        nets[#nets + 1] = net

        if s.scan then
            cfg.scan_ssid = net.ssid
        end

        if s.selected then
            cfg.selected_net = net
        end
    end)

    c:commit('repeater')
    c:close()
    file.sync()

    cfg.nets = nets
    cfg.dfs = file.access('/proc/gl-hw-info/dfs')

    return cfg
end

local function wait_wifi_radio_config_ready(c, radio, deadtime)
    log.info(radio, 'wait wifi config ready...')

    while not c:get('wireless', radio, 'band') do
        log.debug('wait wifi', radio, 'config ready...')

        if sys.uptime() > deadtime then
            return false
        end

        time.sleep(3)

        c:load('wireless')
    end

    return true
end

local function wait_wifi_config_ready(timeout)
    local deadtime = sys.uptime() + timeout

    log.info('wait wifi config ready...')

    local c = uci.cursor()

    for name in file.dir('/usr/share/') do
        local radio = name:match('wifi%-features%-([%w_]+)')
        if radio and not wait_wifi_radio_config_ready(c, radio, deadtime) then
            return false
        end
    end

    c:close()

    return true
end

local function load_wifi_config()
    if not wait_wifi_config_ready(100) then
        return nil
    end

    if vendor.init then
        vendor.init()
    end

    log.info('load wifi config...')

    local c = uci.cursor()

    local devices = {}

    for s in c:each('wireless', 'wifi-device') do
        local sid = s['.name']

        if s.disabled ~= '1' then
            local phy, iface = repeater.lookup_phy(s)
            if not phy then
                log.err('Not support:', sid)
            else
                devices[#devices + 1] = {
                    type = s.type,
                    sid = sid,
                    band = s.band,
                    phy = phy,
                    iface = iface
                }

                log.info('found radio', sid)
            end
        end
    end

    c:close()

    return devices
end

local function trace_hook(event)
    local info3 = debug.getinfo(3, 'Sl')

    if not info3 then
        return
    end

    local info2 = debug.getinfo(2, 'nf')

    local src = info3.short_src

    if info3.currentline ~= -1 then
        src = src .. ':' .. info3.currentline
    end

    log.info(event:upper(), info2.name or info2.func, src)
end

local function ubus_init(R)
    local con = ubus.connect()

    con:add('repeater', {
        log = {
            function(_, msg)
                local level = msg.level
                local path = msg.path

                if type(level) == 'string' and type(log[level]) == 'number' then
                    log.set_level(log[level])
                end

                if type(path) == 'string' then
                    log.set_path(path)
                end
            end,
            { level = ubus.STRING }
        },
        devices = {
            function(req)
                con:reply(req, { devices = R.devices })
            end
        },
        status = {
            function(req)
                local s = R:get_status()
                local state_string = {'idle', 'connecting', 'connected', 'failed'}
                s.state_s = state_string[s.state + 1]

                s.portal_info = R:get_portal_info()

                con:reply(req, s)
            end
        },
        get_portal_info = {
            function(req)
                con:reply(req, R:get_portal_info())
            end
        },
        surveys = {
            function(req)
                con:reply(req, { surveys = R.surveys })
            end
        },
        scan = {
            function(req, msg)
                if file.access('/usr/bin/atp') then
                    sys.sh({'atp', 'online'}, 3)
                end

                if not R:should_use_cache_for_scan() then
                    if not R:scan(true, msg.refresh) then
                        while R.scanning do time.sleep(1) end
                    end
                end

                local survey = {}

                for _, dev in ipairs(R.devices) do
                    if not R.cfg.lock_band or R.cfg.lock_band == dev.band then
                        for _, bss in ipairs(R.surveys[dev.sid] or {}) do
                            if R.cfg.dfs or not bss.dfs then
                                survey[#survey + 1] = bss
                            end
                        end
                    end
                end

                con:reply(req, { survey = survey })
            end,
            { cached = ubus.BOOLEAN }
        },
        connect = {
            function(req, msg)
                R:switch_to_new(msg)
            end,
            {
                ssid = ubus.STRING,
                bssid = ubus.STRING,
                identity = ubus.STRING,
                key = ubus.STRING,
                wds = ubus.BOOLEAN,
                network = ubus.STRING,
                protocol = ubus.STRING,
                ip = ubus.STRING,
                netmask = ubus.STRING,
                gateway = ubus.STRING,
                dns = ubus.ARRAY,
                mtu = ubus.INT32,
                ttl = ubus.INT32,
                hl = ubus.INT32,
                remember = ubus.BOOLEAN,
                manual = ubus.BOOLEAN,
                auto_portal = ubus.BOOLEAN,
                disguise = ubus.BOOLEAN,
                macaddr = ubus.STRING,
                hostname = ubus.STRING
            }
        },
        disconnect = {
            function()
                eco.run(function()
                    while R.configuring do time.sleep(1) end
                    R:disable()
                end)
            end
        },
        reload = {
            function()
                local cfg = load_config()

                R:update_config(cfg)

                local s = R:get_status()

                if s.state == 0 then
                    return
                end

                if not R.scanning and not s.channel then
                    return
                end

                local band
                if s.band then
                    band = s.band
                else
                    band = '2g'
                    if s.channel ~= nil and s.channel > 14 then
                        band = '5g'
                    end
                end

                if not cfg.lock_band or cfg.lock_band == band then
                    return
                end

                eco.run(function()
                    R:disable()
                    R:enable()
                end)
            end
        },
        save_config = {
            function()
                R:save_config()
            end
        },
        enter_bare_mode = {
            function(req, msg)
                R:enter_bare_mode(msg.client_macaddr)
            end, {
                client_macaddr = ubus.STRING
            }
        },
        exit_bare_mode = {
            function()
                R:exit_bare_mode()
            end
        },
        set_exit = {
            function(_, msg)
                -- exit without disconnect
                R.exit = msg.exit
            end,
            { exit = ubus.BOOLEAN }
        },
        trace = {
            function()
                for _, co in ipairs(eco.all()) do
                    debug.sethook(co, trace_hook, 'rc')
                end
            end
        },
        disabled_bss = {
            function(req)
                local now = sys.uptime()
                local timeout = R.cfg.blacklist_timeout or 300
                local list = {}
                for bssid, ts in pairs(R.disabled_bss) do
                    local elapsed = now - ts
                    list[#list + 1] = {
                        bssid = bssid,
                        added_at = ts,
                        elapsed = elapsed,
                        remaining = math.max(timeout - elapsed, 0),
                        expired = elapsed >= timeout
                    }
                end
                con:reply(req, {
                    blacklist_timeout = timeout,
                    uptime = now,
                    count = #list,
                    entries = list
                })
            end
        },
        set_extra_scan_ssid = {
            function(_, msg)
                R.cfg.extra_scan_ssid = msg.ssid
            end,
            { ssid = ubus.STRING }
        }
    })

    con:listen('network.interface', function(ev, params)
        if params.action == 'ifup' then
            R:on_interface_up(params.interface)
        elseif params.action == 'ifdown' then
            R:on_interface_down(params.interface)
        end
    end)

    con:listen('kmwan.status', function(ev, params)
        R:on_kmwan_status_changed(params.interface, params.status, params.failover_chg)
    end)

    con:listen('ntp.valid', function()
        R:on_ntp_valid()
    end)

    while true do
        time.sleep(1000)
    end
end

local function quit(R)
    if not R.exit then
        R:disconnect()
    end

    eco.unloop()
end

local function main()
    if arg[1] == '-trace' then
        debug.sethook(trace_hook, 'rc')
    end

    if eco.VERSION_MAJOR > 3 then
        eco.set_panic_hook(function(traceback1, traceback2)
            log.err(traceback1)
            log.err(traceback2)
        end)
    else
        eco.panic_hook = function(err)
            log.err(err)
        end
    end

    local sig = sys.signal(sys.SIGINT, function()
        log.info('\nGot SIGINT, now quit')
        eco.unloop()
    end)

    log.set_flags(log.FLAG_LF | log.FLAG_FILE)
    log.set_ident('gl-repeater')

    log.info('lua-eco version:', eco.VERSION)

    local devices = load_wifi_config()
    if not devices then
        return
    end

    local cfg = load_config()

    if eco.VERSION_MAJOR > 3 then
        sig:close()
    else
        sig:cancel()
    end

    local R = repeater.new(cfg, devices)

    sys.signal(sys.SIGINT, function()
        log.info('\nGot SIGINT, now quit')
        quit(R)
    end)

    sys.signal(sys.SIGTERM, function()
        log.info('\nGot SIGTERM, now quit')
        quit(R)
    end)

    eco.run(ubus_init, R)

    time.at(1, function() R:start() end)

    R:run()
end

main()
