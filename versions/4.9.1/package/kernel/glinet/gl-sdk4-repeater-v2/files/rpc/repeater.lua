--[[
    @object-name: repeater
    @object-desc: WiFi repeater
--]]

local M = {}

local rpc = require "oui.rpc"
local ubus = require "oui.ubus"
local uci = require "uci"
local fs = require "oui.fs"
local validator = require "gl.validator"

--[[
    @method-type: call
    @method-name: set_config
    @method-desc: Setup config

    @in bool   auto           Setup the enable swith to other saved networks (with this option enabled, router will try to connect to other saved network when the current WiFi doesn't have the Internet access)
    @in bool smart_reconnect  Setup the smart switching logic to enable or disable
    @in string ?lock_band     Setup the repeater band (2g or 5g, null refers to auto)

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","set_config", {"auto": true, "lock_band": "2g"}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": null}
--]]
M.set_config = function(params)
    local c = uci.cursor()

    local auto = params.auto
    local smart_reconnect = params.smart_reconnect
    local lock_band = params.lock_band

    if not validator.base(auto, "boolean", true) then
        return rpc.ERROR_CODE_INVALID_PARAMS, 'param "auto" must be a boolean value'
    end

    -- default is true
    if smart_reconnect == nil then
        smart_reconnect = true
    end
    if not validator.base(smart_reconnect, "boolean", true) then
        return rpc.ERROR_CODE_INVALID_PARAMS, 'param "smart_reconnect" must be a boolean value'
    end

    if lock_band ~= nil and lock_band ~= '' and lock_band ~= '2g' and lock_band ~= '5g' and lock_band ~= '6g' then
        return rpc.ERROR_CODE_INVALID_PARAMS, 'param "lock_band" must be a string value'
    end

    c:set("repeater", "@main[0]", "auto", auto and "1" or "0")
    c:set("repeater", "@main[0]", "smart_reconnect", smart_reconnect and "1" or "0")
    c:set("repeater", "@main[0]", "lock_band", lock_band or "")

    c:commit("repeater")
    fs.sync()

    ubus.call("repeater", "reload")
end

--[[
    @method-type: call
    @method-name: get_config
    @method-desc: Get the confirguations

    @out bool   dfs_support       Get whether DFS is supported or not
    @out bool   auto              Get whether it is supported to switch to other saved networks or not
    @out bool    smart_reconnect  Get the smart switching logic state
    @out string lock_band         Get the repeater band (2g or 5g, null refers to auto)
    @out string macaddr           Get the default macaddr

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","get_config"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"auto": true, "lock_band": "2g"}}
--]]
M.get_config = function()
    local c = uci.cursor()
    local res = {}

    c:foreach("repeater", "main", function(s)
        res.auto = s.auto == "1"
        res.smart_reconnect = s.smart_reconnect == "1"
        res.lock_band = s.lock_band
        res.macaddr = s.macaddr
    end)

    res.dfs_support = fs.access("/proc/gl-hw-info/dfs")

    return res
end

--[[
    @method-type: call
    @method-name: get_channel_prompt
    @method-desc: Get the confirguations of channel prompt

    @out bool chan_prompt_en Get whether the channel prompt has been enabled or not
    @out bool popup_prompt_en Get whether the pop-up prompt has been enabled or not

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","get_channel_prompt"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"chan_prompt_en": true, "popup_prompt_en": true}}
--]]
M.get_channel_prompt = function()
    local res = {}
    local chan_flag_file = "/tmp/chan_prompt_flag"
    local popup_flag_file = "/tmp/popup_prompt_flag"

    res.chan_prompt_en = not fs.access(chan_flag_file)
    res.popup_prompt_en = not fs.access(popup_flag_file)
    return res
end

--[[
    @method-type: call
    @method-name: set_channel_prompt
    @method-desc: Setup channel prompt

    @in bool   ?chan_prompt_en       Setup the channel prompt
    @in bool   ?popup_prompt_en       Setup the pop-up prompt

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","set_channel_prompt", {"chan_prompt_en": true, "popup_prompt_en": true}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": null}
--]]
M.set_channel_prompt = function(params)
    local chan_prompt_en = params.chan_prompt_en
    local popup_prompt_en = params.popup_prompt_en

    if chan_prompt_en ~= nil and not validator.base(chan_prompt_en, "boolean", true) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid chan_prompt_en"
    end

    if popup_prompt_en ~= nil and not validator.base(popup_prompt_en, "boolean", true) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid chan_prompt_en"
    end

    if chan_prompt_en ~= nil then
        if chan_prompt_en then
            os.remove("/tmp/chan_prompt_flag")
        else
            os.execute("touch /tmp/chan_prompt_flag")
        end
    end

    if popup_prompt_en ~= nil then
        if popup_prompt_en then
            os.remove("/tmp/popup_prompt_flag")
        else
            os.execute("touch /tmp/popup_prompt_flag")
        end
    end
end

local function get_current_repeater_network(c)
    local status = ubus.call("repeater", "status")
    if not status or not status.config or not status.config.ssid then
        return nil
    end

    local current = status.config
    local sid

    c:foreach("repeater", "network", function(s)
        if s.ssid ~= current.ssid then
            return
        end

        if current.bssid and current.bssid ~= '' and s.bssid and s.bssid ~= '' and s.bssid ~= current.bssid then
            return
        end

        sid = s[".name"]
        return false
    end)

    return sid
end

--[[
    @method-type: call
    @method-name: get_repeater_portal
    @method-desc: Get portal auth mode and saved auth info for current repeater WiFi

    @out number auth_mode   Auth mode [1: one-click; 2: password; 3: username/password; 4: voucher]
    @out string ?username   Portal username, only for auth_mode=3 and exists
    @out string ?password   Portal password, only for auth_mode=2/3 and exists
    @out string ?voucher    Portal voucher, only for auth_mode=4 and exists
    @out bool   ?one_click  Portal one-click flag, only for auth_mode=1 and exists

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","get_repeater_portal"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"auth_mode": 3, "username": "user", "password": "pass"}}
--]]
M.get_repeater_portal = function()
    local c = uci.cursor()
    local sid = get_current_repeater_network(c)

    if not sid then
        return rpc.ERROR_CODE_NOT_FOUND, "current repeater wifi not found"
    end

    local auth_mode_raw = c:get("repeater", sid, "portal_auth_mode")
    if not auth_mode_raw or auth_mode_raw == '' then
        return rpc.ERROR_CODE_INVALID_PARAMS, "portal_auth_mode not found"
    end

    local auth_mode = tonumber(auth_mode_raw)

    if not auth_mode or auth_mode < 1 or auth_mode > 4 then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid portal_auth_mode"
    end

    local res = {
        auth_mode = auth_mode,
    }

    if auth_mode == 1 then
        local one_click = c:get("repeater", sid, "portal_one_click")
        if one_click == '1' then
            res.one_click = true
        elseif one_click == '0' then
            res.one_click = false
        end
    elseif auth_mode == 2 then
        local password = c:get("repeater", sid, "portal_password")
        if password and password ~= '' then
            res.password = password
        end
    elseif auth_mode == 3 then
        local username = c:get("repeater", sid, "portal_username")
        local password = c:get("repeater", sid, "portal_password")

        if username and username ~= '' then
            res.username = username
        end

        if password and password ~= '' then
            res.password = password
        end
    elseif auth_mode == 4 then
        local voucher = c:get("repeater", sid, "portal_voucher")
        if voucher and voucher ~= '' then
            res.voucher = voucher
        end
    end

    return res
end

--[[
    @method-type: call
    @method-name: set_repeater_portal
    @method-desc: Setup portal auth mode and auth info for current repeater WiFi

    @in number auth_mode              Auth mode [1: one-click; 2: password; 3: username/password; 4: voucher]
    @in bool   save_config=false      Whether save auth config
    @in string ?username              Portal username, required when auth_mode is 3
    @in string ?password              Portal password, required when auth_mode is 2 or 3
    @in string ?voucher               Portal voucher, required when auth_mode is 4
    @in bool   ?one_click             Portal one-click flag, required when auth_mode is 1

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","set_repeater_portal",{"auth_mode":3,"save_config":true,"username":"user","password":"pass"}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": null}
--]]
M.set_repeater_portal = function(params)
    params = params or {}

    local auth_mode = params.auth_mode
    local save_config = params.save_config
    local username
    local password
    local voucher
    local one_click

    if save_config == nil then
        save_config = false
    end

    if not validator.base(save_config, "boolean", true) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid save_config"
    end

    if save_config then
        if not validator.base(auth_mode, "integer", false, 1, 4) then
            return rpc.ERROR_CODE_INVALID_PARAMS, "invalid auth_mode"
        end

        if auth_mode == 1 then
            one_click = params.one_click

            if not validator.base(one_click, "boolean", false) then
                return rpc.ERROR_CODE_INVALID_PARAMS, "invalid one_click"
            end
        elseif auth_mode == 2 then
            password = params.password
            if password == '' then password = nil end

            if not validator.base(password, "string", false, 1, 256) then
                return rpc.ERROR_CODE_INVALID_PARAMS, "invalid password"
            end
        elseif auth_mode == 3 then
            username = params.username or params.usrname
            password = params.password
            if username == '' then username = nil end
            if password == '' then password = nil end

            if not validator.base(username, "string", false, 1, 256) then
                return rpc.ERROR_CODE_INVALID_PARAMS, "invalid username"
            end

            if not validator.base(password, "string", false, 1, 256) then
                return rpc.ERROR_CODE_INVALID_PARAMS, "invalid password"
            end
        elseif auth_mode == 4 then
            voucher = params.voucher
            if voucher == '' then voucher = nil end

            if not validator.base(voucher, "string", false, 1, 1024) then
                return rpc.ERROR_CODE_INVALID_PARAMS, "invalid voucher"
            end
        end
    end

    local c = uci.cursor()
    local sid = get_current_repeater_network(c)

    if not sid then
        return rpc.ERROR_CODE_NOT_FOUND, "current repeater wifi not found"
    end

    c:delete("repeater", sid, "portal_auth_mode")
    c:delete("repeater", sid, "portal_one_click")
    c:delete("repeater", sid, "portal_username")
    c:delete("repeater", sid, "portal_password")
    c:delete("repeater", sid, "portal_voucher")

    if save_config then
        c:set("repeater", sid, "portal_auth_mode", auth_mode)

        if auth_mode == 1 then
            if one_click ~= nil then
                c:set("repeater", sid, "portal_one_click", one_click and "1" or "0")
            end
        elseif auth_mode == 2 then
            if password ~= nil then
                c:set("repeater", sid, "portal_password", password)
            end
        elseif auth_mode == 3 then
            if username ~= nil then
                c:set("repeater", sid, "portal_username", username)
            end

            if password ~= nil then
                c:set("repeater", sid, "portal_password", password)
            end
        elseif auth_mode == 4 then
            if voucher ~= nil then
                c:set("repeater", sid, "portal_voucher", voucher)
            end
        end
    else
        local screen_c = uci.cursor()
        local auth_status = screen_c:get("gl_screen", "portal", "auth_status")
        if auth_status == "re_fail" then
            screen_c:set("gl_screen", "portal", "auth_status", "waiting")
            screen_c:commit("gl_screen")
        end
        screen_c:close()
    end

    c:commit("repeater")
    fs.sync()

    local status = ubus.call("repeater", "status")
    if status then
        ubus.call('gl-session', 'notify', { name = 'repeater.status', data = status })
    end
end

local function is_valid_mac(mac)
    if not validator.macaddr(mac) then
        return false
    end

    local n = mac:sub(1, 2)

    return tonumber(n, 16) % 2 == 0
end

local function generate_random_macaddr(cnt)
    local nums = {}

    cnt = cnt or 6

    for i = 1, cnt do
        local n = math.random(1, 255)

        if i == 1 then
            n = n - (n % 2)
        end

        nums[i] = string.format('%02X', n)
    end

    return table.concat(nums, ':')
end

local function get_client_macaddr()
    local remote_ip = ngx.var.remote_addr

    for l in io.lines("/proc/net/arp") do
        if l:match(remote_ip) then
            return l:match('%S+%s+%S+%s+%S+%s+(%S+)'):upper()
        end
    end

    return generate_random_macaddr(6)
end

local function find_dhcp_hostname(mac)
    local c = uci.cursor()
    local leasefile = c:get('dhcp', '@dnsmasq[0]', 'leasefile')

    for line in io.lines(leasefile) do
        local hostname = line:match(mac:lower() .. ' %d+%.%d+%.%d+%.%d+% (%S+)')
        if hostname then
            return hostname
        end
    end

    return nil
end

--[[
    @method-type: call
    @method-name: connect
    @method-desc: Connect an AP

    @in string  ssid              SSID
    @in string  ?identity         Setup the EAP identity (required when connect to WPA Enterprise)
    @in string  ?key              Set up the password (required when connect to excrypted WiFi)
    @in bool    ?remember=false   Setup whether save the network or not
    @in string  ?bssid            Setup whether lock the BSSID or not (null refers to not lock)
    @in bool    ?manual           Setup connect by manual
    @in string  ?protocol=dhcp    Setup the Internet connection [DHCP ; static]
    @in string  ?ip               Setup the IP address (required when set as static)
    @in string  ?netmask          Setup the netmask (required when set as static)
    @in string  ?gateway          Setup the gateway (required when set as static)
    @in array   ?dns              Setup the DNS (required when set as static)
    @in number  ?mtu              Setup the MTU of the device
    @in number  ?ttl              Setup the TTL output via the device
    @in number  ?ttl_ipv6         Setup the HL output via the device
    @in bool    disguise          Setup enabling disguises
    @in object  ?macaddr          Setup macaddr and mode
    @in string  ?macaddr.mode     macaddr mode: ['default'; 'clone'; 'random']
    @in string  ?macaddr.macaddr  macaddr value: required if macaddr.mode != 'default'
    @in string  ?macaddr.update   random macaddr update policy: ['none'; 'reboot'; 'time']
    @in number  ?macaddr.period   required if macaddr.update is 'time', the value must be great than 0, the time unit is hour
    @in bool    ?auto_portal      Setup whether to enter the portal authentication mode automatically

    @out number ?err_code         Error code
    @out number ?err_msg          Error data

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","connect",{"ssid":"test","key":"goodlife"}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": null}
--]]
M.connect = function(params)
    local ssid = params.ssid
    local identity = params.identity
    local key = params.key
    local remember = params.remember
    local manual = params.manual
    local network = params.network
    local protocol = params.protocol
    local ip = params.ip
    local netmask = params.netmask
    local gateway = params.gateway
    local dns = params.dns
    local mtu = params.mtu
    local ttl = params.ttl
    local ttl_ipv6 = params.ttl_ipv6
    local auto_portal = params.auto_portal
    local bssid = params.bssid
    local disguise = params.disguise
    local macaddr

    if not validator.base(disguise, "boolean", true) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid disguise"
    end

    if not disguise then
        macaddr = params.macaddr or { mode = "default" }

        if type(macaddr) ~= "table" then
            return rpc.ERROR_CODE_INVALID_PARAMS, "invalid macaddr mode"
        end
    end

    if not validator.base(ssid, "string", false, 1, 32) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid ssid"
    end

    if bssid ~= nil and not is_valid_mac(bssid) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid bssid"
    end

    if not validator.base(identity, "string", true, 1, 256) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid identity"
    end

    if not validator.base(key, "string", true, 1, 256) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid key"
    end

    if not validator.base(remember, "boolean", true) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid remember"
    end

    if not validator.base(manual, "boolean", true) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid manual"
    end

    if not validator.base(auto_portal, "boolean", true) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid auto_portal"
    end

    if not validator.base(network, "string", true, 1, 256) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid network"
    end

    if not validator.base(mtu, "integer", true, 576, 9200) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid mtu"
    end

    if not validator.base(ttl, "integer", true, 1, 255) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid ttl"
    end

    if not validator.base(ttl_ipv6, "integer", true, 1, 255) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid ttl_ipv6"
    end

    if protocol ~= nil then
        if protocol ~= "dhcp" and protocol ~= "static" then
            return rpc.ERROR_CODE_INVALID_PARAMS, "invalid protocol"
        end

        if protocol == "static" then
            if not validator.ip4addr(ip) then
                return rpc.ERROR_CODE_INVALID_PARAMS, "invalid ip"
            end

            if not validator.netmask4(netmask) then
                return rpc.ERROR_CODE_INVALID_PARAMS, "invalid netmask"
            end

            if not validator.ip4addr(gateway) then
                return rpc.ERROR_CODE_INVALID_PARAMS, "invalid gateway"
            end

            if dns ~= nil then
                if type(dns) ~= 'table' then
                    return rpc.ERROR_CODE_INVALID_PARAMS, "invalid dns"
                end

                for _, d in ipairs(dns) do
                    if not validator.ip4addr(d) then
                        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid dns"
                    end
                end
            end
        end
    end

    local hostname
    local client_macaddr = get_client_macaddr()

    if disguise then
        macaddr = client_macaddr
        hostname = find_dhcp_hostname(macaddr) or '*'
    else
        local mode = macaddr.mode

        if mode ~= "default" and mode ~= "clone" and mode ~= "random" then
            return rpc.ERROR_CODE_INVALID_PARAMS, "invalid macaddr mode"
        end

        if mode ~= "default" then
            if not is_valid_mac(macaddr.macaddr) then
                return rpc.ERROR_CODE_INVALID_PARAMS, "invalid macaddr"
            end

            hostname = '*'
        end

        if mode == "random" then
            local update = macaddr.update or "none"

            if update ~= "none" and update ~= "reboot" and update ~= "time" then
                return rpc.ERROR_CODE_INVALID_PARAMS, "invalid macaddr update mode"
            end

            if update == "time" then
                if type(macaddr.period) ~= 'number' or macaddr.period < 1 then
                    return rpc.ERROR_CODE_INVALID_PARAMS, "invalid macaddr update period"
                end

                -- hour to minute
                macaddr.period = macaddr.period * 60
            end
        end
    end

    ubus.call("repeater", "connect", {
        remember = remember,
        ssid = ssid,
        bssid = bssid,
        identity = identity,
        key = key,
        manual = manual,
        network = network or 'wwan',
        proto = protocol,
        ipaddr = ip,
        netmask = netmask,
        gateway = gateway,
        dns = dns,
        mtu = mtu,
        ttl = ttl,
        hl = ttl_ipv6,
        auto_portal = auto_portal,
        disguise = disguise,
        macaddr = macaddr,
        hostname = hostname
    })
end

--[[
    @method-type: call
    @method-name: get_saved_ap_list
    @method-desc: Get the saved networks data

    @out array   res           Get the list of saved networks
    @out string  res.ssid      SSID
    @out string  ?res.bssid    Get whether the BSSID is locked or not (null refers to not locked)
    @out string  ?res.identity Get the EAP identify (required for EAP enterprise)
    @out string  ?res.key      Get the password
    @out string  ?res.protocol=dhcp  Get the Internet connection type [DHCP ; static]
    @out string  ?res.ip       Get the IP address (static IP)
    @out string  ?res.netmask  Get the netmask (static IP)
    @out string  ?res.gateway  Get the gateway (static IP)
    @out array   ?res.dns      Get the DNS (static IP)
    @out number  ?res.mtu          Get the MTU of the device
    @out number  ?res.ttl          Get the TTL output via the device
    @out number  ?res.ttl_ipv6     Get the HL output via the device
    @out bool    ?res.manual   Get whether it is added by manual
    @out object  ?macaddr          Setup macaddr and mode
    @out string  ?macaddr.mode     macaddr mode: ['default'; 'clone'; 'random']
    @out string  ?macaddr.macaddr  macaddr value
    @out string  ?macaddr.update   random macaddr update policy: ['none'; 'reboot'; 'time']
    @out number  ?macaddr.period   required if macaddr.update is 'time', the value must be great than 0, the time unit is hour
    @out bool    ?res.auto_portal Get Whether to enter the portal authentication mode automatically
    @out object  ?res.portal_info              Get the portal authentication info (only for saved networks with portal)
    @out number  res.portal_info.auth_mode     Portal auth mode [1: one-click; 2: password; 3: username/password; 4: voucher]
    @out string  ?res.portal_info.one_click    Portal one-click flag (only for auth_mode=1)
    @out string  ?res.portal_info.password     Portal password (only for auth_mode=2/3)
    @out string  ?res.portal_info.username     Portal username (only for auth_mode=3)
    @out string  ?res.portal_info.voucher      Portal voucher (only for auth_mode=4)

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","get_saved_ap_list"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"res": [{"ssid":"test", "key": "12132432423"}, {"ssid":"x", "key": "sefrsedtfsde"}]}}
--]]
M.get_saved_ap_list = function()
    local c = uci.cursor()
    local res = {}

    c:foreach("repeater", "network", function(s)
        local entry = {
            ssid = s.ssid,
            bssid = s.bssid,
            key = s.key,
            identity = s.identity,
            protocol = s.proto,
            ip = s.ipaddr,
            netmask = s.netmask,
            gateway = s.gateway,
            dns = s.dns,
            mtu = tonumber(s.mtu),
            ttl = tonumber(s.ttl),
            ttl_ipv6 = tonumber(s.hl),
            manual = s.scan == "1",
            auto_portal = s.auto_portal == '1',
            disguise = s.disguise == "1"
        }

        if not entry.disguise then
            local macaddr = s.macaddr
            local mode = "default"
            local update = "none"
            local period

            if macaddr then
                local prefix

                prefix, macaddr = macaddr:match("(%w+),(.+)")

                if prefix == "c" then
                    mode = "clone"
                else
                    mode = "random"

                    prefix = prefix:sub(2)

                    if prefix == "r" then
                        update = "reboot"
                    elseif #prefix > 0 then
                        update = "time"
                        -- minute to hour
                        period = tonumber(prefix) / 60
                    end
                end
            end

            entry.macaddr = {
                mode = mode,
                macaddr = macaddr,
                update = update,
                period = period
            }
        end

        -- Add portal info if configured
        local portal_auth_mode = tonumber(s.portal_auth_mode) or 0
        if portal_auth_mode > 0 then
            local portal_fields = {
                [1] = { 'one_click' },
                [2] = { 'password' },
                [3] = { 'username', 'password' },
                [4] = { 'voucher' }
            }

            entry.portal_info = {
                auth_mode = portal_auth_mode
            }

            for _, field in ipairs(portal_fields[portal_auth_mode] or {}) do
                local value = s['portal_' .. field]
                if value and value ~= '' then
                    entry.portal_info[field] = value
                end
            end
        end

        res[#res + 1] = entry
    end)

    return {res = res}
end

--[[
    @method-type: call
    @method-name: remove_saved_ap
    @method-desc: Remove the saved network

    @in string  ssid    SSID

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","remove_saved_ap",{"ssid":"test"}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": null}
--]]
M.remove_saved_ap = function(params)
    local c = uci.cursor()
    local ssid = params.ssid
    local deleted_had_scan = false
    local selected_sid
    local remain_sids = {}

    if not validator.base(ssid, "string", false, 1, 32) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid ssid"
    end

    c:foreach("repeater", "network", function(s)
        if s.ssid == ssid then
            if s.scan then
                deleted_had_scan = true
            end
            c:delete("repeater", s[".name"])
        else
            local sid = s[".name"]
            table.insert(remain_sids, sid)
            if s.selected == "1" then
                selected_sid = sid
            end
        end
    end)

    if deleted_had_scan then
        local target = selected_sid or remain_sids[1]

        if target then
            for _, sid in ipairs(remain_sids) do
                if sid == target then
                    c:set("repeater", sid, "scan", 1)
                else
                    c:delete("repeater", sid, "scan")
                end
            end
        end
    end

    c:commit("repeater")
    fs.sync()

    ubus.call("repeater", "reload")
end

--[[
    @method-type: call
    @method-name: disconnect
    @method-desc: Disconnect the connected WiFi

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","disconnect"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": null}
--]]
M.disconnect = function()
    ubus.call("repeater", "disconnect")
end

--[[
    @method-type: call
    @method-name: get_status
    @method-desc: Get the connection status

    @out number  state             Status [-1: initializing ;0: not used; 1: connecting; 2: connected; 3: connect failed; 4: wan available]
    @out string  ?fail_type        Get the failed type [not-found; key]
    @out bool    eap               Get whether it is EAP enterprise encryption
    @out string  ?wifi_generation  Get Wi-Fi protocol version [4: wifi4 5: wifi5 6: wifi6 7: wifi7]
    @out string  ?device           Get the WiFi device name
    @out string  ?bssid            Get the connected WiFi BSSID
    @out number  ?channel          Get the connected WiFi channel
    @out number  ?signal           Get the connected WiFi signal strength(dBm)
    @out object  ?ipv4             Get the IPv4 info
    @out string  ipv4.ip           Get the IPv4 address (CIDR)
    @out array   ipv4.dns          IPv4 DNS
    @out string  ipv4.gateway      Get the IPv4 gateway
    @out object  ?ipv6             Get the IPv6 info
    @out array   ipv6.ip           Get the IPv6 address (CIDR)
    @out array   ipv6.dns          IPv6 DNS
    @out string  ipv6.gateway      Get the IPv6 gateway
    @out bool    portal            Whether portal is detected
    @out string  portal_url        Get the portal url
    @out bool    bare_mode         Get the bare status
    @out object  config            Current profile
    @out string  config.ssid             SSID
    @out string  config.identity         The EAP identity (required when connect to WPA Enterprise)
    @out string  config.key              The password (required when connect to excrypted WiFi)
    @out bool    config.remember         Whether save the network or not
    @out string  config.bssid            Whether lock the BSSID or not (null refers to not lock)
    @out bool    config.manual           Connect by manual
    @out string  config.protocol=dhcp    Internet connection [DHCP ; static]
    @out string  config.ip               IP address (required when set as static)
    @out string  config.netmask          Netmask (required when set as static)
    @out string  config.gateway          Gateway (required when set as static)
    @out array   config.dns              DNS (required when set as static)
    @out string  config.portal='manual'  How to connect a WiFi with portal('manual' or 'auto')
    @out bool    config.disguise         Enabling disguises
    @out object  config.macaddr          macaddr and mode
    @out string  config.macaddr.mode     macaddr mode: ['default'; 'clone'; 'random']
    @out string  config.macaddr.macaddr  macaddr value
    @out string  config.macaddr.update   random macaddr update policy: ['none'; 'reboot'; 'time']
    @out number  config.macaddr.period   required if macaddr.update is 'time', the value must be great than 0, the time unit is hour
    @out bool    config.auto_portal      Whether to enter the portal authentication mode automatically

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","get_status"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"state": 3, "ssid": "test", "bssid": "11:22:33:44:55:66", "channel": 12, "signal": 20, "ipv4": {"ip":"192.168.8.12/24", "gateway": "192.168.1.1"}, "ipv6": {"ip": ["fd4d:7210:ad91:10::/64"], "dns": ["fd5c:d00d:fa8e::/64"], "gateway":"fd5c:d00d:fa8e::/64"}}}
--]]
M.get_status = function()
    local initializing = -1
    return ubus.call("repeater", "status") or { state = initializing }
end

--[[
    @method-type: call
    @method-name: scan
    @method-desc: Scan the APs

    @in bool    refresh         Perform a real scan, even if the scan interval has not expired

    @out array  res             Get the AP list
    @out string res.ssid        SSID
    @out string res.bssid       BSSID
    @out number res.signal      Get the signal strength
    @out number res.channel     Get the channel
    @out bool   res.dfs         Channel is dfs
    @out number res.band        Get the band (2g or 5g)
    @out bool   res.saved       Get whether to save it or not
    @out string res.device      Get the WiFi device (refer to 'wifi-device' in /etc/config/wireless)
    @out object res.encryption                Get the security info
    @out bool   res.encryption.enabled        Get whether it is encrypted
    @out string res.encryption.description    Security description
    @out object ?res.portal_info              Get the portal authentication info (only for saved networks with portal)
    @out number res.portal_info.auth_mode     Portal auth mode [1: one-click; 2: password; 3: username/password; 4: voucher]
    @out string ?res.portal_info.one_click    Portal one-click flag (only for auth_mode=1)
    @out string ?res.portal_info.password     Portal password (only for auth_mode=2/3)
    @out string ?res.portal_info.username     Portal username (only for auth_mode=3)
    @out string ?res.portal_info.voucher      Portal voucher (only for auth_mode=4)

    @out number ?err_code         Error code: -1: in dfs cac state, -2: interface not ready
    @out number ?err_msg          Error data

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","scan"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"res": [{"ssid":"GL-AR750S-4e2-5G","band":"5g","encryption":{"enabled":true,"description":"WPA2 PSK (CCMP)","auth_suites":["PSK"],"wpa":2,"pair_ciphers":["CCMP"], "uci": "psk2"},"bssid":"94:83:C4:0C:54:E3","channel":36,"signal":-60}]}}
 --]]
function M.scan(params)
    local c = uci.cursor()
    local survey = {}

    params = params or {}

    if not validator.base(params.refresh, "boolean", true) then
        return rpc.ERROR_CODE_INVALID_PARAMS, "invalid refresh"
    end

    local mod = dofile("/usr/lib/oui-httpd/rpc/wifi")
    local status = mod.get_status().res
    local lock_band = c:get('repeater', '@main[0]', 'lock_band')
    local device_type = c:get('wireless', '@wifi-device[0]', 'type')

    for _, s in ipairs(status) do
        if device_type == 'qcawificfg80211' then
            if s.band == "5g" and (lock_band == '5g' or lock_band == nil) then
                if s.state == "cac" then
                    return {
                        err_code = -1,
                        err_msg = "in dfs cac state"
                    }
                elseif s.state ~= "ready" then
                    return {
                        err_code = -2,
                        err_msg = "interface not ready"
                    }
                end
            elseif s.band == "6g" and lock_band ~= '2g' and lock_band ~= '5g' then
                if s.state ~= "ready" then
                    return {
                        err_code = -2,
                        err_msg = "interface not ready"
                    }
                end
            end
        elseif device_type == 'qcacld32' then
            if s.state ~= "ready" then
                return {
                    err_code = -1,
                    err_msg = "in dfs cac state"
                }
            end
        else
            if s.band == "5g" and (lock_band == '5g' or lock_band == nil) then
                if s.state ~= "ready" then
                    return {
                        err_code = -1,
                        err_msg = "in dfs cac state"
                    }
                end
            elseif s.band == "6g" and lock_band ~= '2g' and lock_band ~= '5g' then
                if s.state ~= "ready" then
                    return {
                        err_code = -2,
                        err_msg = "interface not ready"
                    }
                end
            end
        end
    end

    local saved_networks = {}
    c:foreach("repeater", "network", function(s)
        saved_networks[s.ssid] = {
            portal_auth_mode = tonumber(s.portal_auth_mode) or 0,
            portal_password = s.portal_password or '',
            portal_username = s.portal_username or '',
            portal_voucher = s.portal_voucher or '',
            portal_one_click = s.portal_one_click == '1' and true or false
        }
    end)

    local function add(info)
        local caps = info.caps

        if not caps['ESS'] then
            return
        end

        if not info.ssid or not info.ssid:match('%C+') then
            return
        end

        local encryption = {}

        -- skip wep
        if caps['PRIVACY'] then
            if not info.rsn and not info.wpa then
                return
            end
        end

        local auth_suites = {}

        local rsn = info.rsn
        local wpa = info.wpa

        if rsn then
            for k in pairs(rsn.auth_suites) do
                auth_suites[k] = true
            end
            if rsn.auth_suites['OWE'] then
                encryption.enabled = false
            else
                encryption.enabled = true
            end
        end

        if wpa then
            for k in pairs(wpa.auth_suites) do
                auth_suites[k] = true
            end
            encryption.enabled = true
        end

        local description = {}

        for k in pairs(auth_suites) do
            description[#description + 1] = k
        end

        encryption.description = table.concat(description, ',')

        local saved_net = saved_networks[info.ssid]
        local saved = saved_net ~= nil

        local result = {
            ssid = info.ssid,
            encryption = encryption,
            bssid = info.bssid,
            signal = info.signal,
            channel = info.channel,
            dfs = info.dfs,
            band = info.band,
            device = info.device,
            saved = saved
        }

        if saved and saved_net.portal_auth_mode and saved_net.portal_auth_mode > 0 then
            local portal_fields = {
                [1] = { 'one_click' },
                [2] = { 'password' },
                [3] = { 'username', 'password' },
                [4] = { 'voucher' }
            }

            result.portal_info = {
                auth_mode = saved_net.portal_auth_mode
            }

            for _, field in ipairs(portal_fields[saved_net.portal_auth_mode] or {}) do
                local value = saved_net['portal_' .. field]
                if value and value ~= '' then
                    result.portal_info[field] = value
                end
            end
        end

        survey[#survey + 1] = result

        return true
    end

    local res

    -- maybe the repeater not running, wait it to be running
    local deadtime = ngx.now() + 40

    while ngx.now() < deadtime do
        res = ubus.call("repeater", "scan", { refresh = params.refresh })
        if res then break end
        ngx.sleep(1)
    end

    if res then
        for _, s in ipairs(res.survey) do
            add(s)
        end
    end

    return { res = survey }
end

--[[
    @method-type: call
    @method-name: enter_bare_mode
    @method-desc: Enter bare mode

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","enter_bare_mode"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": null}
 --]]
function M.enter_bare_mode()
    ubus.call('repeater', 'enter_bare_mode', { client_macaddr = get_client_macaddr() })
end

--[[
    @method-type: call
    @method-name: exit_bare_mode
    @method-desc: Exit bare mode

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","repeater","exit_bare_mode"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": null}
 --]]
 function M.exit_bare_mode()
    ubus.call('repeater', 'exit_bare_mode')
 end

return M
