--[[
    @object-name: kmwan
    @object-desc: kmwan api
]]

local uci = require "uci"
local fs  = require "oui.fs"
local utils = require "oui.utils"
local cjson = require "cjson"
local kmwan = require "gl.kmwan"
local ubus = require "oui.ubus"

local M = {}

local function add_ip_info(cmd, ip)
    if ip then
        for i = 1, #ip do
            ip[i] = cmd .. "," .. ip[i]
        end
    end
end

--[[
    @method-type: call
    @method-name: set_interface
    @method-desc: set interface info

    @in string     interface       detetction interface name
    @in bool       enable_check    enable detection
    @in number     track_mode      Internet status check mode,which takes effect when enbale_check is true[0:passive; 1:force; 2:strict]
    @in number     track_proto     detection protocol [0:ipv4 only; 1:ipv6 only; 2:both ipv4 & ipv6]
    @in number     track_method    detection way [0:ping; 1:httping, not implemented]
    @in bool       ?enable_ssl     Whether ssl is enabled in httping mode, not use
    @in array      track_ipv4      IPv4 address detected
    @in array      track_ipv6      IPv6 address detected

    @out number   ?err_code        error code
    @out string   ?err_msg         error message

    @in-example:  {"jsonrpc":"2.0","method":"call","params":["","kmwan","set_interface",{"interface":"wan","enable_check":true,"track_mode":1,"track_proto": 0,"track_method":0,"ipv4":["8.8.4.4","8.8.8.8","208.67.222.222","208.67.220.220"],"ipv6":["2001:4860:4860::8844","2001:4860:4860::8888","2620:0:ccd::2","2620:0:ccc::2"]}],"id":1}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": null}
]]
function M.set_interface(params)
    local interface = params.interface
    local enable_check = params.enable_check
    local track_mode = params.track_mode
    local track_proto = params.track_proto
    local track_method = params.track_method
    local enable_ssl = params.enable_ssl
    local track_ipv4 = params.track_ipv4
    local track_ipv6 = params.track_ipv6

    local iface6 = interface .. 6
    if string.find(interface, "modem") then
        iface6 = interface .. "_6"
    end

    local c = uci.cursor()
    if enable_check then

        c:set("kmwan", interface, "check", 1)
        --Track Mode
        if track_mode == 0 then
            c:set("kmwan", interface, "track_mode", "passive")
            c:set("kmwan", iface6, "track_mode", "passive")
        elseif track_mode == 1 then
            c:set("kmwan", interface, "track_mode", "force")
            c:set("kmwan", iface6, "track_mode", "force")
        else
            c:set("kmwan", interface, "track_mode", "strict")
            c:set("kmwan", iface6, "track_mode", "strict")
        end

        local track_cmd
        if track_method == 1 then
            track_cmd = "httping"
            c:set("kmwan", interface, "enable_ssl", enable_ssl and 1 or 0)
            c:set("kmwan", iface6, "enable_ssl", enable_ssl and 1 or 0)
        else
            track_cmd = "ping"
            c:delete("kmwan", interface, "enable_ssl")
            c:delete("kmwan", iface6, "enable_ssl")
        end

        --Track proto and Track ip
        if track_proto == 0 then --ipv4
            c:set("kmwan", interface, "disabled", 0)
            c:set("kmwan", iface6, "disabled", 1)
            add_ip_info(track_cmd, track_ipv4)
            c:set("kmwan", interface, "tracks", track_ipv4)
        elseif track_proto == 1 then --ipv6
            c:set("kmwan", interface, "disabled", 1)
            c:set("kmwan", iface6, "disabled", 0)
            add_ip_info(track_cmd, track_ipv6)
            c:set("kmwan", iface6, "tracks", track_ipv6)
        else --ipv4 and ipv6
            c:set("kmwan", interface, "disabled", 0)
            c:set("kmwan", iface6, "disabled", 0)
            add_ip_info(track_cmd, track_ipv4)
            add_ip_info(track_cmd, track_ipv6)
            c:set("kmwan", interface, "tracks", track_ipv4)
            c:set("kmwan", iface6, "tracks", track_ipv6)
        end
    else
        c:set("kmwan", interface, "check", 0)
        c:set("kmwan", interface, "disabled", 1)
        c:set("kmwan", iface6, "disabled", 1)
    end

    c:commit("kmwan")

    --接口配置有变动是改变发射功率
    os.execute("/usr/bin/atp interface_changed &")

    fs.sync()
    ngx.pipe.spawn({"/etc/init.d/kmwan","restart"})
    --接口配置有变动是改变发射功率
    ngx.pipe.spawn({"/usr/bin/atp", "interface_changed"})
end

--[[
    @method-type: call
    @method-name: set_sensitivity
    @method-desc: set sensitivity

    @in string  sensitivity.level  Set sensitivity level[low; medium; high; custom]
    @in number  ?sensitivity.val   Set detection time interval(unit:s)

    @out number   ?err_code        error code
    @out string   ?err_msg         error message

    @in-example:  {"jsonrpc":"2.0","method":"call","params":["","kmwan","set_sensitivity",{"sensitivity":{"level":"custom","val":5}}],"id":1}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": null}
--]]
function M.set_sensitivity(params)
    local c = uci.cursor()
    local sensitivity = params.sensitivity
    local level = sensitivity.level or "medium"

    local val
    if level == "low" then
        val = 5000
    elseif level == "medium" then
        val = 3000
    elseif level == "high" then
        val = 1000
    else
        val = sensitivity.val * 1000
    end

    local mode = c:get("kmwan", "global", "mode")
    local rtmode = c:get("glconfig", "general", "mode") == "passthrough" and 1 or 0

    c:set("kmwan", "global", "level", level)
    c:set("kmwan", "global", "sensitivity", val)
    c:commit("kmwan")

    local str = {op = 0, data = {sensitivity = val, mode = mode, rtmode = rtmode}}
    local json_data = cjson.encode(str)
    utils.writefile("/proc/gl-kmwan/config", json_data)
    fs.sync()
end


--[[
    @method-type: call
    @method-name: get_sensitivity
    @method-desc: get kmwan sensitivity

    @out string   sensitivity.level              sensitivity level[low; medium; high; custom]
    @out number   ?sensitivity.val               sensitivity val(unit:s)

    @out number   ?err_code                      error code
    @out string   ?err_msg                       error message

    @in-example:  {"jsonrpc":"2.0", "method":"call", "params":["","kmwan","get_sensitivity",{}],"id":1}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result":{"sensitivity":{"level":"custom", "val":5}}}
--]]
function M.get_sensitivity()
    local c = uci.cursor()
    local val = c:get("kmwan", "global", "sensitivity") or "3000"
    local level = c:get("kmwan", "global", "level") or "medium"

    local sensitivity = {}
    sensitivity.level = level
    sensitivity.val = tonumber(val) / 1000

    return {sensitivity = sensitivity}
end

--[[
    @method-type: call
    @method-name: set_config
    @method-desc: set kmwan Settings

    @in string     mode                    mode setting[0:failover; 1:load balance]
    @in array      interfaces              interface setting
    @in string     interfaces.interface    interface name
    @in number     ?interfaces.metric      link metric [for failover mode]
    @in number     ?interfaces.weight      load weight [for load balance mode, ranges: 0-10]

    @out number   ?err_code                error code
    @out string   ?err_msg                 error message

    @in-example:  {"jsonrpc":"2.0","method":"call","params":["","kmwan","set_config",{"mode":0,"interfaces":[{"interfacd":"wan","metric":10},{"interface":"wwan","metric":20}]}],"id":1}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": null}
]]
function M.set_config(params)
    local mode = params.mode
    local args = params.interfaces

    local c = uci.cursor()
    local flag = false
    local iface6
    local cfg_changes

    if mode == 0 then
        for i = 1, #args do
            local metric = args[i].metric
            if type(metric) ~= "number" or metric < 0 or metric > #args then
                return {
                    err_code = -1,
                    err_msg = "invalid metric"
                }
            end
        end

        c:set("kmwan", "global", "mode", "failover")
        for i = 1, #args do
            if string.find(args[i].interface, "modem") then
                iface6 = args[i].interface .. "_6"
                c:foreach("glmodem", "network", function(s)
                    c:set("glmodem", s['.name'], "metric", args[i].metric)
                    flag = true
                end)
            else
                iface6 = args[i].interface .. 6
            end
            c:set("kmwan", args[i].interface, "metric", args[i].metric)
            c:set("network", args[i].interface, "metric", args[i].metric)
            c:set("kmwan", iface6, "metric", args[i].metric)
        end
        cfg_changes = c:changes()
        if next(cfg_changes) then
            ubus.send('kmwan.status', {failover_chg = true})
        end
    else
        for i = 1, #args do
            local weight = args[i].weight
            if type(weight) ~= "number" or weight < 0 or weight > 10 then
                return {
                    err_code = -1,
                    err_msg = "invalid weight"
                }
            end
        end

        c:set("kmwan", "global", "mode", "balance")
        cfg_changes = c:changes()
        if next(cfg_changes) then
            ubus.send('kmwan.status', {failover_chg = true})
        end
        for i = 1, #args do
            if string.find(args[i].interface, "modem") then
                iface6 = args[i].interface .. "_6"
            else
                iface6 = args[i].interface .. 6
            end

            c:set("kmwan", args[i].interface, "weight", args[i].weight)
            c:set("kmwan", iface6, "weight", args[i].weight)
        end
    end
    c:commit("kmwan")
    c:commit("network")
    if flag then
        c:commit("glmodem")
    end
    fs.sync()
    ubus.call("network", "reload")
    ngx.pipe.spawn(". /lib/functions/kmwan.sh;sync_route_netcell"):wait()

    if fs.access("/usr/bin/gen_weight_route") then
        ngx.pipe.spawn({"/usr/bin/gen_weight_route"})
    end

    -- 用户更改故障转移/负载均衡模式，通知atp
    ngx.pipe.spawn({"/usr/bin/atp", "interface_changed"})
end

--[[
    @method-type: call
    @method-name: get_config
    @method-desc: get kmwan Settings

    @out number   mode                           mode setting[0:failover; 1:load balance]
    @out number   sensitivity                    detection time interval(unit:s)
    @out array    interfaces                     interface setting
    @out string   interfaces.interface           interface name
    @out number   ?interfaces.metric             link metric [for failover mode]
    @out number   ?interfaces.weight             load weight [for load balance mode, ranges: 0-10]
    @out bool     interfaces.enable_check        enable detection
    @out number   interfaces.track_mode          Internet status check mode
    @out number   interfaces.track_proto         detection protocol [0:ipv4 only; 1:ipv6 only; 2:both ipv4 & ipv6]
    @out number   interfaces.track_method        detection way [0: ping; 1: httping]
    @out bool     ?enable_ssl                    Whether to enable ssl in http mode
    @out array    interfaces.track_ipv4          detection ipv4 address [array of string]
    @out array    interfaces.track_ipv6          detection ipv6 address [array of string]

    @out number   ?err_code                      error code
    @out string   ?err_msg                       error message

    @in-example:  {"jsonrpc":"2.0", "method":"call", "params":["","kmwan","get_config",{}],"id":1}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result":{"mode":1, "sensitivity":5, "interfaces":[{"interface":"wan", "weight":1, "metric":10, "enable_check":true, "track_mode":1, "track_proto":0, "track_method":0, "track_ipv4":["8.8.4.4","8.8.8.8","208.67.222.222","208.67.220.220"]},{"interface":"wwan", "weight":2, "metric":20, "enable_check":true, "track_mode":0, "track_proto":0, "track_method":0, "track_ipv4":["8.8.4.4","8.8.8.8","208.67.222.222","208.67.220.220"]}]}}
--]]
function M.get_config()
    local c = uci.cursor()
    local mode = (c:get("kmwan", "global", "mode") == "failover") and 0 or 1
    local ipv6_enable = c:get("glipv6", "globals", "enabled")

    local interfaces = {}
    local res = {}
    c:foreach("kmwan", "member", function(s)
        if string.find(s[".name"], "6") == nil then
            local iface6 = s[".name"] .."6"
            if string.find(s[".name"], "modem") then
                iface6 = s[".name"] .. "_6"
            end

            local metric = tonumber(s.metric)
            local weight = tonumber(s.weight)

            local tracks = s.tracks or {}
            local track_ipv4 = {}
            for _, v in pairs(tracks) do
                local index = string.find(v, ",")
                track_ipv4[#track_ipv4+1] = string.sub(v, index + 1)
            end

            local tracks6 = c:get("kmwan", iface6, "tracks") or {}
            local track_ipv6 = {}
            for _, v in pairs(tracks6) do
                local index = string.find(v, ",")
                track_ipv6[#track_ipv6+1] = string.sub(v, index + 1)
            end

            local track_method = 1
            if tracks[1] then
                track_method = string.match(tracks[1], "%w+") == "ping" and 0 or 1
            end

            local enable_ssl = c:get("kmwan", s[".name"], "enable_ssl") == "1"
            res = {
                interface = s[".name"],
                metric = metric,
                weight = weight,
                track_mode = s.track_mode == "force" and 1 or s.track_mode == "passive" and 0 or 2,
                track_method = track_method,
                enable_ssl = track_method == 1 and enable_ssl,
                track_ipv4 = track_ipv4,
                track_ipv6 = track_ipv6
            }

            local enable_check = s.disabled == "0"
            local enable_check6 = false
            if ipv6_enable == "1" then
                enable_check6 = c:get("kmwan", iface6, "disabled") == "0"
            end

            res.enable_check = enable_check or enable_check6

            if enable_check and not enable_check6 then
                res.track_proto = 0
            elseif not enable_check and enable_check6 then
                res.track_proto = 1
            elseif enable_check and enable_check6 then
                res.track_proto = 2
            end
            interfaces[#interfaces+1] = res
        end
    end)

    return {
        mode = mode,
        interfaces = interfaces
    }
end

--[[
    @method-type: call
    @method-name: get_status
    @method-desc: get status

    @out array    interfaces                     interface setting info
    @out string   interfaces.interface           interface name.
    @out number   interfaces.status_v4           interface ipv4 status [0:online; 1:offline]
    @out number   interfaces.status_v6           interface ipv6 status [0:online; 1:offline]

    @out number   ?err_code                      Error code
    @out string   ?err_msg                       Error messag
    @in-example:  {"jsonrpc":"2.0","method":"call","params":["","kmwan","get_status",{}],"id":1}
    @out-example: {"id":1,"jsonrpc":"2.0","result":{"interfaces":[{"status_v6":0,"status_v4":0,"interface":"wan"},{"status_v6":1,"status_v4":1,"interface":"wwan"},{"status_v6":1,"status_v4":1,"interface":"tethering"},{"status_v6":1,"status_v4":0,"interface":"modem_1_1_2"},{"status_v6":1,"status_v4":0,"interface":"modem_1_1_3"}]}}
--]]
function M.get_status()
    local interfaces = {}
    local c = uci.cursor()
    local check_array = {}

    c:foreach("kmwan", "member", function(s)
        local array = {}
        local iface = s[".name"]
        if string.find(iface, "6") == nil then
            local iface6 = iface .. "6"
            if string.find(iface, "modem") then
                iface6 = iface .. "_6"
            end
            if c:get("kmwan", iface, "disabled") == "0" then
                array.interface = iface
                array.status_v4 = kmwan.get_ifstatus(iface) == "online" and 0 or 1
                array.status_v6 = kmwan.get_ifstatus(iface6) == "online" and 0 or 1
                interfaces[#interfaces+1] = array
            else
                array.interface = iface
                array.interface6 = iface6
                check_array[#check_array+1] = array
            end
        end
    end)

    for i=1, #check_array do
        local array = {}
        local iface_status = (ubus.call("network.interface." .. check_array[i].interface, "status") or {}).up or false
        local iface6_status = (ubus.call("network.interface." .. check_array[i].interface6, "status") or {}).up or false
        array.interface = check_array[i].interface
        array.status_v4 = iface_status and 0 or 1
        array.status_v6 = iface6_status and 0 or 1
        interfaces[#interfaces+1] = array
    end

    return {interfaces = interfaces}
end

return M
