--[[
    @object-name: mptun
    @object-desc: mptun settings
--]]

local uci = require "uci"
local fs = require "oui.fs"
local ubus = require "oui.ubus"
local utils = require "oui.utils"
local cjson = require "cjson"
local http = require "resty.http"

local M = {}

local ERR_PARAMETER = 1

local req_path = {
    ["req_get_identity"] = "/sdwan/sdk-iprb/v1/my/network/simple",
    ["req_get_seconds_list"] = "/sdwan/sdk-iprb/v1/secondary/list",
    ["req_get_access_code"] = "/sdwan/sdk-iprb/v1/device/access/dynamic-code",
    ["req_get_bind_code"] = "/sdwan/sdk-iprb/v1/device/bind/dynamic-code",
    ["req_remove_second"] = "/sdwan/sdk-iprb/v1/device/client/remove",
    ["req_get_bind_info"] = "/sdwan/sdk-iprb/v1/bindInfo",
    ["req_net_ctrl"] = "/sdwan/sdk-iprb/v1/network/status/ctrl",
    ["req_access_by_code"] = "/sdwan/sdk-iprb/v1/device/dynamic-code/access",
    ["req_second_rename"] = "/sdwan/sdk-iprb/v1/network/secondary/rename",
    ["req_use_exit"] = "/sdwan/sdk-iprb/v1/device/useInternetExit",
    ["req_get_pay_url"] = "/sdwan/sdk-iprb/v1/pay/url",
    ["req_reset"] = "/sdwan/sdk-iprb/v1/device/network/reset",
    ["req_order_alerts"] = "/sdwan/sdk-iprb/v1/device/order/alerts",
    ["req_order_pageList"] = "/sdwan/sdk-iprb/v1/device/order/pageList",
    ["req_order_alerts_dismiss"] = "/sdwan/sdk-iprb/v1/device/order/alerts/dismiss",
    ["req_order_alerts_mute"] = "/sdwan/sdk-iprb/v1/network/plan/alerts/mute",
    ["req_order_refund_link"] = "/sdwan/sdk-iprb/v1/device/order/refund-link"
}
local valid_req_path = {
    [req_path["req_get_identity"]] = true,
    [req_path["req_get_seconds_list"]] = true,
    [req_path["req_get_access_code"]] = true,
    [req_path["req_get_bind_code"]] = true,
    [req_path["req_remove_second"]] = true,
    [req_path["req_get_bind_info"]] = true,
    [req_path["req_net_ctrl"]] = true,
    [req_path["req_access_by_code"]] = true,
    [req_path["req_second_rename"]] = true,
    [req_path["req_use_exit"]] = true,
    [req_path["req_get_pay_url"]] = true,
    [req_path["req_reset"]] = true,
    [req_path["req_order_alerts"]] = true,
    [req_path["req_order_pageList"]] = true,
    [req_path["req_order_alerts_dismiss"]] = true,
    [req_path["req_order_alerts_mute"]] = true,
    [req_path["req_order_refund_link"]] = true
}

local valid_req_header = {
    ["content-type"] = true
}

local valid_req_header_value = {
    ["application/json"] = true
}

local valid_req_method = {
    ["GET"] = true,
    ["POST"] = true
}

local valid_query_and_body_params = {
    ["dynamicCode"] = "^%w+$",
    ["networkId"] = "^%w+$",
    ["sort"] = "^[%w%_%-%,]+$",

    ["DEFAULT"] = "^[%w%_%-]+$"
}

local snoop_func = {}
local pre_req_func = {}

local function vpn_is_tap_s2s()
    local rpc_ret = ubus.call("gl-session", "call", { module = "vpn-client", func = "get_status", params = {} })
    if rpc_ret ~= nil and rpc_ret.result ~= nil and type(rpc_ret.result) == 'table' and rpc_ret.result.status_list ~= nil and type(rpc_ret.result.status_list) == 'table' then
        for _, v in pairs(rpc_ret.result.status_list) do
            if v ~= nil and v.type ~= nil and type(v.type) == 'string' and v.type == 'tap-s2s' then
                if v.enabled ~= nil and type(v.enabled) == 'boolean' and v.enabled == true then
                    return true
                end
            end
        end
    end
    return false
end

local function is_valid_netmode()
    local c = uci.cursor()
    return c:get("glconfig", "general", "mode") == 'router'
end

local function __is_meet_startup_conditions()
    if vpn_is_tap_s2s() == true then
        return false
    end
    if is_valid_netmode() == false then
        return false
    end
    return true
end
local function check_req_header(k, v)
    if not k and not v then
        return true
    end
    if v and v ~= '' and not valid_req_header_value[v] then
        return false
    end
    if not k or not valid_req_header[string.lower(tostring(k))] then
        return false
    end
    return true
end

local function check_payload_header(key, value)
    if not key or not value then
        return true
    end
    if not key then
        return false
    end
    if value and value ~= '' then
        local clean_k = key:match("^%s*(.-)%s*$")
        local clean_v = value:match("^%s*(.-)%s*$")
        local pattern = valid_query_and_body_params[key] or valid_query_and_body_params["DEFAULT"]
        return string.match(clean_k, valid_query_and_body_params["DEFAULT"]) == clean_k and
            string.match(clean_v, pattern) == clean_v
    end
    return true
end

-- validate_func: check query params and body params
local function check_params_valid(params, checkfunc)
    local function check(key, value)
        if value == nil then
            return true
        end
        local t = type(value)
        if t == "string" then
            return checkfunc(key, value)
        elseif t == "table" then
            for k, v in pairs(value) do
                local real_k = k
                if type(k) == 'number' then
                    real_k = key
                end
                if check(real_k, v) == false then
                    return false
                end
            end
        end
        return true
    end
    if params == nil then
        return true
    end
    if checkfunc == nil then
        return false
    end
    return check(nil, params)
end

local function set_service_mode(mode)
    local c = uci.cursor()
    if mode ~= nil and (mode == 'IPRB' or mode == 'classic') then
        c:set("mptun", "global", "service_mode", tostring(mode))
        c:commit("mptun")
    else
        return -1
    end
end
local function set_IPRB_option(opt)
    local c = uci.cursor()
    if not opt then
        return -1
    end

    c:set("mptun", "IPRB", "service_instance")
    if type(opt) == 'table' then
        for k, v in pairs(opt) do
            if v and tostring(v) ~= '' then
                local old_v = c:get("mptun", "IPRB", tostring(k)) or " "
                if tostring(old_v) ~= tostring(v) then
                    c:set("mptun", "IPRB", tostring(k), tostring(v))
                end
            end
        end
    end
    c:commit("mptun")
end

local function net_topology_is_exist()
    for _ = 1, 15 do
        if fs.access('/etc/mptun/mpnet_conf') then
            return true
        end
        ngx.sleep(1)
    end
    return false
end

--[[
    @method-type: call
    @method-name: set_config
    @method-desc: set mptun config

    @in array   interfaces Interfaces information that participate in aggregation network.
    @in string  interfaces.name Name of the interface.
    @in bool    interfaces.enable Indicates whether the interface is enabled.
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["","mptun","set_config",{"interfaces":[{"name":"wan","enable":true},{"name":"secondwan","enable":true}, {"name":"wwan","enable":true},{"name":"tethering","enable":true},{"name":"modem_0000_4","enable":false}]}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": []}
--]]
function M.set_config(params)
    local c = uci.cursor()
    local interfaces = params.interfaces
    if not interfaces or type(interfaces) ~= "table" then
        return {
            err_code = -ERR_PARAMETER,
            err_msg = "Input parameter error"
        }
    end

    for i=1, #interfaces do
        local iface = interfaces[i].name
        local enable = interfaces[i].enable
        if string.find(iface, "modem") and not string.find(iface, "_4") then
            iface = iface .. "_4"
        end

        local metric = tonumber(c:get("kmwan", iface, "metric") or -1)
        if metric ~= -1 then
            if enable then
                metric = metric < 100 and metric or metric - 100
            else
                metric = metric < 100 and metric + 100 or metric
            end
            c:set("kmwan", iface, "metric", metric)
        end
        c:set("mptun", iface, "enable", enable and 1 or 0)
    end

    c:commit("kmwan")
    c:commit("mptun")
    fs.sync()

    ngx.timer.at(2.0, function()
        ngx.pipe.spawn({"/etc/init.d/mptun", "restart"}):wait()
        ngx.pipe.spawn({"/etc/init.d/mpifd", "restart"}):wait()
        ngx.pipe.spawn({"/etc/init.d/kmwan", "restart"}):wait()
        ngx.pipe.spawn({"/etc/init.d/mpflow", "restart"})
    end)

end

--[[
    @method-type: call
    @method-name: get_config
    @method-desc: get mptun config

    @out array   interfaces Interfaces information that participate in aggregation network.
    @out string  interfaces.name Name of the interface.
    @out bool    interfaces.enable Indicates whether the interface is enabled.

    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "get_config"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"interfaces":[{"name":"wan","enable":true},{"name":"secondwan","enable":true},{"name":"wwan","enable":false},{"name":"tethering","enable":false}]}}
--]]
function M.get_config()
    local c = uci.cursor()
    local ifaces = {}

    c:foreach("mptun", "interface", function(s)
        local array = {}
        local enable = tonumber(c:get("mptun", s[".name"], "enable") or "0" )
        array.name = s[".name"]
        array.enable = enable == 1
        ifaces[#ifaces+1] = array
    end)

    return {interfaces = ifaces}
end

--[[
    @method-type: call
    @method-name: get_status
    @method-desc: get mptun network status

    @out string name Name of the device in the network
    @out number status Whether the device is in an aggregated network (0:offline 1:online 2:disable)
    @out string network_name The network name field obtained from the cloud platform
    @out number type Native information(0: edge 1:cloud gateway 2: local gateway)
    @out array ?nodes Information about connections to other nodes, If the object does not exist, means the current device does not belong to any network
    @out object nodes.self information of the node
    @out string nodes.self.name Name of the node
    @out number nodes.self.status Indicates whether the node is online.(0:offline 1:online 2:warning)
    @out number nodes.self.type Type of the node(0:edge, 1:cloud gateway 2 local gateway)
    @out number nodes.self.acl_type Type of the access policy(0:none, 1:allow to local 2:allow to subnet)
    @out number nodes.self.bond Indicates whether the connection is bonding.(0:none 1:aggregation 2:s2s 3:aggregation+s2s)
    @out object nodes.peer Information of the peer(device self)
    @out number nodes.peer.acl_type Type of the access policy(0:none, 1:allow to local 2:allow to subnet)
    @out number nodes.peer.bond Indicates whether the connection is bonding.(0:none 1:aggregation 2:s2s 3:aggregation+s2s)
    @out bool nodes.peer.connect Whether the node is connected to other nodes

    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "get_status"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"network_name":"test","type":0,"name":"mt6000","nodes":[{"self":{"acl_type":2,"type":1,"name":"test","status":1,"bond":1},"peer":{"acl_type":0,"bond":0,"connect":true}}],"status":1}}
]]
function M.get_status()
    local c = uci.cursor()
    local cloud_enable = c:get("gl-cloud", "@cloud[0]", "enable") or "0"
    local conf = {}

    if cloud_enable ~= "0" then
        ubus.call('gl-cloud', 'notify', {type = 'astrowarp/get_device_net_topology'})
        local aw_enable = c:get('mptun', 'global', 'enable') or "0"
        if aw_enable == "0" then
            if fs.access('/etc/mptun/mpnet_conf') then
                conf = cjson.decode(utils.readfile("/etc/mptun/mpnet_conf") or "{}")
            else
                return {}
            end
        else
            if net_topology_is_exist() then
                conf = cjson.decode(utils.readfile("/etc/mptun/mpnet_conf") or "{}")
            else
                return {}
            end
        end
    end

    local status = 1
    local loop_cnt = 0

    if not conf.peer or #conf.peer == 0 then
        status = 0
    else
        loop_cnt = #conf.peer
    end

    if conf.dev_status == "offline" then
        status = 0
    elseif conf.dev_status == "disable" then
        status = 2
    end

    local nodes = {}
    local network_name
    local network_status = conf.networkStatus or "disable"
    for i = 1, loop_cnt do
        local node = {}
        local self = {}
        local peer = {}
        self.name = conf.peer[i].name

        self.status = 0
        if conf.peer[i].nodeType == "CLOUD_GATEWAY" then
            self.type = 1
            -- cloud gateway status should be 'enable/disable',
            -- but the old version users 'online'
            if conf.peer[i].status == "enable" or conf.peer[i].status == "online" then
                self.status = network_status ~= "disable" and 1 or 0
            end
        elseif conf.peer[i].edgeType and conf.peer[i].edgeType == "LOCAL_GATEWAY" then
            self.type = 2
            if conf.peer[i].status == "online" then
                self.status = 1
            end
        else
            self.type = 0
            if conf.peer[i].status == "online" then
                self.status = 1
            elseif conf.peer[i].status == "disable" then
                self.status = 2
            end
        end

        peer.acl_type = 0
        self.acl_type = 0
        self.bond = 0
        peer.bond = 0
        peer.connect = true
        if conf.peer[i].connectionType == "aggregation" then
            network_name = conf.peer[i].name
            self.bond = 1
            self.acl_type = 1
        elseif conf.peer[i].connectionType == "none" then
            peer.connect = false
        else
            if conf.peer[i].fromAllowAccess and
               conf.peer[i].fromAllowAccess == 1 then
                peer.acl_type = 2
            end
            if conf.peer[i].toAllowAccess and
               conf.peer[i].toAllowAccess == 1 then
                self.acl_type = 2
            end

            if conf.peer[i].nodeType and conf.peer[i].nodeType ~= "EDGE" then
                peer.bond = 1
            end

            if conf.peer[i].toAsInternetExit and conf.peer[i].toAsInternetExit == 1 then
                self.bond = 1
                if self.acl_type ~= 2 then
                    self.acl_type = 1
                end
            end

            if conf.peer[i].fromAsInternetExit and conf.peer[i].fromAsInternetExit == 1 then
                peer.bond = 1
                if peer.acl_type ~= 2 then
                    peer.acl_type = 1
                end
            end
        end
        node.self = self
        node.peer = peer
        nodes[#nodes+1] = node
    end

    return {
        network_name = conf.networkName or network_name,
        name = conf.name,
        type = conf.type == 0 and 0 or 2,
        status = status,
        nodes = nodes
    }
end

--[[
    @method-type: call
    @method-name: get_traffic
    @method-desc: get traffic information

    @out object path An object that stores all aggregated interface traffic information.
    @out object path.$iface An object that stores traffic information about an aggregate interface. iface:[wan|wwan|secondwan|tethering|modem]
    @out array  ?path.$iface.tx Interface upload traffic
    @out array  ?path.$iface.rx Interface download traffic

    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "get_traffic"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"path":{"wan":{"rx":[13373,15484,13626],"tx":[16677,18612,12387]},"secondwan":{"rx":[19838,20956,11762],"tx":[11486,12092,6762]}}}}
]]
function M.get_traffic()
    ngx.pipe.spawn({"/usr/bin/get_mpflow.sh"}):wait()
    local data = cjson.decode(utils.readfile("/tmp/mpflow.json") or "{}")
    if data.path then
        return {path = data.path}
    else
        return {path = {}}
    end
end

local function wait_token_update()
    for _ = 1, 15 do
        if fs.access('/etc/mptun/mp_token') then
            local data = cjson.decode(utils.readfile('/etc/mptun/mp_token') or "{}")
            if data.awDeviceToken then
                return data.awDeviceToken
            end
        end
        ngx.sleep(1)
    end
end

--[[
    @method-type: call
    @method-name: get_token
    @method-desc: get astrowarp http token

    @out string token  http token.

    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "get_token"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"token":"0ff048cecf3c49b6806b1432e64c8c8a"}}
--]]
function M.get_token()
    local msg = {
        data = {},
        type = 'astrowarp/get_aw_device_token'
    }
    os.remove('/etc/mptun/mp_token')
    ubus.call('gl-cloud', 'notify', msg)
    local res = wait_token_update() or ''

    if res == '' then
        return {err_code = -2, err_msg = 'Failed to obtain.'}
    end

    return {token = res}
end

local function pre_do_request(req_path, params)
    if pre_req_func[req_path] then
        pre_req_func[req_path](params)
    end
end
local function snoop_response(req_path, resp)
    if snoop_func[req_path] then
        snoop_func[req_path](resp)
    end
end

local function __raw_request(params)
    local c = uci.cursor()
    local base_url = c:get("mptun", "global", "base_url") or 'https://api.astrowarp.net'
    if not base_url then
        return { err_code = -6, err_msg = 'invalid base_url.' }
    end

    local cloud_token = c:get("gl-cloud", "@cloud[0]", "token") or nil
    if not cloud_token then
        return { err_code = -1, err_msg = 'invalid device token.' }
    end

    local req_path = params.req_path or nil
    local req_method = string.upper(tostring(params.req_method)) or nil
    local req_body = params.req_body or {}
    local query_params = params.query_params or {}
    local append_header = params.append_header or {}
    if not req_path or not req_method then
        return { err_code = -2, err_msg = 'invalid params.' }
    end
    -- parames safety check.
    local params_invalid = false
    if not valid_req_path[req_path] then
        params_invalid = true
    end
    if not valid_req_method[req_method] then
        params_invalid = true
    end
    if not check_params_valid(query_params, check_payload_header) then
        params_invalid = true
    end
    if not check_params_valid(req_body, check_payload_header) then
        params_invalid = true
    end
    if not check_params_valid(append_header, check_req_header) then
        params_invalid = true
    end
    if params_invalid then
        return { err_code = -2, err_msg = 'invalid params.' }
    end

    pre_do_request(req_path, params)
    local timestamp = os.time() .. '000'
    local mac_file = io.open('/proc/gl-hw-info/device_mac', "r") or nil
    local mac
    if mac_file then
        mac = mac_file:read("*a")
    end
    mac = mac:match("^%s*(.-)%s*$")
    if mac ~= '' then
        mac = mac:gsub(':', '')
    else
        return { err_code = -3, err_msg = 'invalid device mac' }
    end

    local url = string.format('%s%s', base_url, req_path)
    local sign = utils.sha256(tostring(cloud_token) .. tostring(timestamp) .. tostring(req_method) .. tostring(req_path))

    local req_header = {
        mac = mac,
        timestamp = timestamp,
        sign = sign,
        ["content-type"] = 'application/json'
    }
    -- check and set custom headers
    if append_header and type(append_header) == 'table' and #append_header > 0 then
        for k, v in pairs(append_header) do
            if k and type(k) == 'string' and valid_req_header[string.lower(tostring(k))] and v and valid_req_header_value[v] then
                req_header[k] = v
            end
        end
    end

    local opt = {
        headers = req_header,
        ssl_verify = true
    }
    if req_method == 'GET' then
        -- set custom query_params
        local append_url = ''
        if type(query_params) == 'table' then
            for k, v in pairs(query_params) do
                if k and v then
                    if type(v) == 'table' then
                        for per_k, per_v in pairs(v) do
                            if type(per_k) ~= 'number' then
                                return { err_code = -6, err_msg = 'method GET invalid param array.' }
                            end
                            append_url = string.format('%s&%s=%s', append_url, tostring(k), tostring(per_v))
                        end
                    else
                        append_url = string.format('%s&%s=%s', append_url, tostring(k), tostring(v))
                    end
                end
            end
        end
        if append_url ~= '' then
            url = string.format('%s?%s', url, append_url)
        end
        opt.method = "GET"
    elseif req_method == 'POST' then
        -- set custom body
        local body = {}
        if type(req_body) == 'table' then
            for k, v in pairs(req_body) do
                if v ~= nil and type(k) == 'string' then
                    body[k] = v
                end
            end
        end
        opt.method = "POST"
        opt.body = cjson.encode(body)
    end

    local httpc = http.new()
    httpc:set_timeout(15000)

    local resp, err = httpc:request_uri(url, opt)
    local need_debug = c:get("mptun", "global", "debug") == '1'
    if need_debug == true then
        local debug_info = {
            url = url,
            req_body = opt.body,
            req_header = req_header
        }
        local f_deb = io.open("/tmp/http_debug_info.log", "a")
        if resp ~= nil then
            if resp.status ~= nil and resp.body ~= nil then
                debug_info.resp_status = resp.status
                debug_info.resp_body = resp.body
            end
        end
        if f_deb ~= nil then
            f_deb:write(cjson.encode(debug_info) .. '\n')
            f_deb:close()
        end
    end
    if err ~= nil then
        return { err_code = -4, err_msg = tostring(err) }
    end
    if resp == nil or not resp.status == nil or not resp.body == nil then
        return { err_code = -5, err_msg = tostring(err) }
    end
    if resp.status == 200 then
        snoop_response(req_path, resp)
    end

    return { err_code = 0, status_code = resp.status, body = resp.body }
end

--[[
    @method-type: call
    @method-name: raw_request
    @method-desc: The front-end interacts with the cloud through this interface.
    @out number     status_code         http status code
    @out string     body                http response body
    @out number     ?err_code           Error Code
    @out string     ?err_msg            Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "raw_request",{"req_method":"POST","req_path":"/sdwan/sdk-iprb/v1/my/network/simple","query_params": {},"req_body":{}}]}
    @out-example: {"id":1,"jsonrpc":"2.0","result":{"status_code":200,"body":"{\"code\":0,\"msg\":\"Success\",\"info\":{\"networkId\":\"e594229d6ed40a19cc23b81c6da999cb\",\"networkPlan\":\"IPRB_PLUS\",\"planCode\":\"008\",\"primaryDevice\":false,\"networkStatus\":1,\"enableStatus\":true,\"useInternetExit\":true,\"latestAccessTime\":1769677787124}}"}}
--]]
function M.raw_request(raw_params)
    local res = __raw_request(raw_params)
    if res.err_code ~= 0 then
        return res
    else
        return { status_code = res.status_code, body = res.body }
    end
end

-- This function updates /etc/config/mptun, so subsequent processes may require reloading it.
local function get_info_from_cloud()
    local opt = {
        ["req_method"] = "POST",
        ["req_path"] = req_path["req_get_identity"]
    }
    return __raw_request(opt)
end

-- This function updates /etc/config/mptun, so subsequent processes may require reloading it.
local function IPRB_is_enabled()
    local c = uci.cursor()
    local service_mode = c:get("mptun", "global", "service_mode") or ''
    if service_mode == nil or service_mode == '' then
        get_info_from_cloud()
        c:load("mptun")
        service_mode = c:get("mptun", "global", "service_mode") or ''
    end
    local disabled = c:get("mptun", "IPRB", "disabled") or '0'
    local identity = c:get("mptun", "IPRB", "identity") or ''
    if disabled == '1' or service_mode ~= "IPRB" then
        return false
    else
        if identity == 'primary' or identity == 'secondary' then
            return true
        else
            return false
        end
    end
end

local function __get_IPRB_status(params)
    local status = {}

    if IPRB_is_enabled() == true then
        status.enabled = '1'
    else
        status.enabled = '0'
    end

    local c = uci.cursor()
    local identity = c:get("mptun", "IPRB", "identity") or ''
    status.identity = identity
    status.lastConnectedTime = c:get("mptun", "IPRB", "lastConnectedTime") or ''

    status.be_kicked_out = c:get("mptun", "IPRB", "be_kicked_out") or '0'
    status.is_p2p = c:get("mptun", "IPRB", "is_p2p") or '0'
    local state = c:get("mptun", "IPRB", "state") or 'connecting'
    if state ~= 'connected' then
        state = "connecting"
    end
    status.state = state
    if identity == "primary" or identity == "secondary" then
        status.attempt_to_be = identity
    else
        status.attempt_to_be = c:get("mptun", "IPRB", "attempt_to_be") or ''
    end
    return { status = status }
end

local function push_status_to_wb()
    local status = __get_IPRB_status()
    ubus.call('gl-session', 'notify', { name = "mptun.IPRB_status", data = status })
end
--[[
    @method-type: call
    @method-name: push_IPRB_status_to_wb
    @method-desc: push IPRB status to websocket.
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "push_IPRB_status_to_wb",{}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": { }}
--]]
function M.push_IPRB_status_to_wb(params)
    push_status_to_wb()
    return {}
end

--[[
    @method-type: call
    @method-name: get_IPRB_status
    @method-desc: Obtain IPRB mode related status information.
    @out string     status.enabled                  enabled or not
    @out string     status.identity                 identity: primary or secondary or null
    @out string     status.state                    state: connecting or connected
    @out string     status.attempt_to_be            primary or secondary or null. The role temporarily selected by the user on the initial page, will be replaced by the identity field after the actual role is confirmed.
    @out string     status.be_kicked_out            for secondary, be kicked out or not
    @out string     status.is_p2p                   Indicates whether a p2p connection has been established.
    @out string     status.lastConnectedTime        Timestamp of the last successful tunnel establishment.
    @out number     ?err_code   Error Code
    @out string     ?err_msg    Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "get_IPRB_status",{}]}
    @out-example: {"id":15,"jsonrpc":"2.0","result":{"status":{"lastConnectedTime":"1769678361","is_p2p":"0","be_kicked_out":"0","attempt_to_be":"secondary","state":"connected","identity":"secondary","enabled":"1"}}}
--]]
function M.get_IPRB_status(params)
    local ret = __get_IPRB_status()
    return ret
end

--[[
    @method-type: call
    @method-name: get_AW_status
    @method-desc: Get partial local status information of the current AW service.
    @out bool       enabled                 Is the AW function actually enabled or not
    @out string     mode                    Current mode: IPRB or classic
    @out object     status                  State information of each mode
    @out bool       ?status.enable          Only available when mode is classic.
    @out number     ?err_code   Error Code
    @out string     ?err_msg    Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "get_AW_status",{}]}
    @out-example: {"id":15,"jsonrpc":"2.0","result":{"enabled":true,"mode":"IPRB","status":{"lastConnectedTime":"1769681084","is_p2p":"0","be_kicked_out":"0","attempt_to_be":"secondary","state":"connected","identity":"secondary","enabled":"1"}}}
--]]
function M.get_AW_status(params)
    local IPRB_status = __get_IPRB_status()
    local c = uci.cursor()
    local current_mode = c:get("mptun", "global", "service_mode")
    local global_enabled = c:get("mptun", "global", "enable") == '1'
    local ret = {}
    if current_mode == 'IPRB' then
        ret.enabled = (IPRB_status.status.enabled == '1' and global_enabled == true)
        ret.mode = current_mode
        ret.status = IPRB_status.status
    else
        ret.enabled = global_enabled
        ret.mode = 'classic'
        ret.status = { enable = global_enabled }
    end
    return ret
end
local function get_network_id()
    local c = uci.cursor()
    local net_id = c:get("mptun", "IPRB", "net_id")

    if net_id ~= nil and string.match(net_id, valid_query_and_body_params["networkId"]) == net_id then
        return { err_code = 0, networkId = net_id }
    end
    local res = get_info_from_cloud()
    if res.status_code ~= 0 or not res.status_code or res.status_code ~= 200 then
        return { err_code = -1 }
    else
        local res_table
        local ok = false
        if res.body then
            ok, res_table = pcall(cjson.decode, res.body)
        end
        if ok and res_table and res_table.info and type(res_table.info) == "table" and res_table.info.networkId then
            net_id = res_table.info.networkId:match("^%s*(.-)%s*$")
            if string.match(net_id, valid_query_and_body_params["networkId"]) == net_id then
                c:set("mptun", "IPRB", "net_id", net_id)
                c:commit("mptun")
                return { err_code = 0, networkId = net_id }
            end
        end
    end
    return { err_code = -1 }
end

local function send_net_ctrl_msg(active)
    local enableStatus
    if active == 'start' then
        enableStatus = true
    else
        enableStatus = false
    end
    local res = get_network_id()
    if not res and res.err_code ~= 0 then
        return { err_code = -1, err_msg = "get networkID fail!" }
    end
    --local networkId = res.networkId
    local opt = {
        ["req_method"] = "POST",
        ["req_path"] = req_path["req_net_ctrl"],
        ["req_body"] = {
            --["networkId"] = networkId,
            ["enableStatus"] = enableStatus
        }
    }
    res = __raw_request(opt)
    if res.err_code ~= 0 or not res.status_code or res.status_code ~= 200 then
        return { err_code = -1, err_msg = "https communicate fail!", status_code = res.status_code, body = res.body }
    else
        return { err_code = 0, status_code = res.status_code, body = res.body }
    end
end

local function __start_service(params)
    if __is_meet_startup_conditions() == false then
        return { err_code = -4, err_msg = "Startup conditions not met !" }
    end
    local option = {}
    option.disabled = '0'
    option.identity = 'primary'
    set_IPRB_option(option)
    push_status_to_wb()
    local resp = send_net_ctrl_msg('start')
    if resp == nil or resp.err_code ~= 0 then
        return { err_code = -1, err_msg = "communicate fail !" }
    end

    local ok = false
    local res_table
    if resp.body then
        ok, res_table = pcall(cjson.decode, resp.body)
    end
    if not ok or res_table == nil then
        return { err_code = -2, err_msg = "response error !" }
    end
    if res_table.info ~= nil and type(res_table.info) == "boolean" then
        if res_table.info ~= true then
            return { err_code = -3, err_msg = "set fail !" }
        end
        return {}
    else
        return { err_code = -2, err_msg = "response error !" }
    end
end

--[[
    @method-type: call
    @method-name: start_service
    @method-desc: start mptun primary side service.
    @out number     ?err_code   Error Code
    @out string     ?err_msg    Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "start_service",{}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
function M.start_service(params)
    return __start_service(params)
end
local function __pause_service(params)
    local option = {}
    option.disabled = '1'
    option.identity = 'primary'
    set_IPRB_option(option)

    send_net_ctrl_msg('stop')
    -- TODO stop()
    ngx.timer.at(2.0, function()
        ngx.pipe.spawn({ "/etc/init.d/mptun", "stop" }):wait()
    end)
    local c = uci.cursor()
    c:set("mptun", "global", "enable", "0")
    c:commit("mptun")
    push_status_to_wb()
    return {}
end

--[[
    @method-type: call
    @method-name: pause_service
    @method-desc: stop mptun primary side service.
    @out number     ?err_code   Error Code
    @out string     ?err_msg    Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "pause_service",{}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
function M.pause_service(params)
    return __pause_service(params)
end
local function __start_connect(params)
    if __is_meet_startup_conditions() == false then
        return { err_code = -3, err_msg = "Startup conditions not met !" }
    end
    local option = {}
    option.disabled = '0'
    option.identity = 'secondary'
    set_IPRB_option(option)
    push_status_to_wb()
    local resp = send_net_ctrl_msg('start')
    if resp == nil or resp.err_code ~= 0 then
        return { err_code = -1, err_msg = "communicate fail !" }
    end

    local ok = false
    local res_table
    if resp.body ~= nil then
        ok, res_table = pcall(cjson.decode, resp.body)
    end
    if not ok or res_table == nil then
        return { err_code = -2, err_msg = "response error !" }
    end
    if res_table.info ~= nil and type(res_table.info) == "boolean" then
        if res_table.info ~= true then
            return { err_code = -3, err_msg = "set fail !" }
        end
        return {}
    else
        return { err_code = -2, err_msg = "response error !" }
    end
end

--[[
    @method-type: call
    @method-name: start_connect
    @method-desc: start mptun second side connections.
    @out number     ?err_code   Error Code
    @out string     ?err_msg    Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "start_connect",{}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
function M.start_connect(params)
    return __start_connect(params)
end
local function __pause_connect(params)
    local option = {}
    option.disabled = '1'
    option.identity = 'secondary'
    set_IPRB_option(option)
    send_net_ctrl_msg('stop')
    -- TODO stop()
    ngx.timer.at(2.0, function()
        ngx.pipe.spawn({ "/etc/init.d/mptun", "stop" }):wait()
    end)
    local c = uci.cursor()
    c:set("mptun", "global", "enable", "0")
    c:commit("mptun")
    push_status_to_wb()

    return {}
end
--[[
    @method-type: call
    @method-name: pause_connect
    @method-desc: stop mptun second side connections.
    @out number     ?err_code   Error Code
    @out string     ?err_msg    Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "pause_connect",{}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
function M.pause_connect(params)
    return __pause_connect(params)
end

--[[
    @method-type: call
    @method-name: set_use_exit_node
    @method-desc: sencond side set use exit node or not.
    @in bool        enabled     use exit node or not
    @out number     ?err_code   Error Code
    @out string     ?err_msg    Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "set_use_exit_node",{"enabled": true}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
function M.set_use_exit_node(params)
    local enabled
    if params ~= nil and params.enabled ~= nil and type(params.enabled) == "boolean" then
        enabled = params.enabled
    else
        return { err_code = -1, err_msg = "invalid params !" }
    end

    local opt = {
        ["req_method"] = "POST",
        ["req_path"] = req_path["req_use_exit"],
        ["req_body"] = {
            ["useInternetExit"] = enabled
        }
    }
    local resp = __raw_request(opt)
    if resp.err_code ~= 0 or resp.status_code == nil or resp.status_code ~= 200 then
        return { err_code = -1, err_msg = "https communicate fail!" }
    elseif resp.body ~= nil then
        local ok
        local res_table
        ok, res_table = pcall(cjson.decode, resp.body)
        if not ok or res_table == nil then
            return { err_code = -2, err_msg = "error https respones!" }
        end
        if res_table.info ~= nil and type(res_table.info) == "boolean" then
            if res_table.info ~= true then
                return { err_code = -3, err_msg = "set fail!" }
            end
        else
            return { err_code = -2, err_msg = "error https respones!" }
        end
        local option = {}
        if enabled then
            option.use_exit = '1'
        else
            option.use_exit = '0'
        end
        --option.identity = 'secondary'
        set_IPRB_option(option)
        push_status_to_wb()

        return {}
    end
end

--[[
    @method-type: call
    @method-name: reset
    @method-desc: reset configuration.
    @out number     ?err_code   Error Code
    @out string     ?err_msg    Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "reset",{}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
function M.reset(params)
    local c = uci.cursor()
    local opt = {
        ["req_method"] = "POST",
        ["req_path"] = req_path["req_reset"],
    }
    local resp = __raw_request(opt)
    if resp.err_code ~= 0 or resp.status_code == nil or resp.status_code ~= 200 then
        return { err_code = -1, err_msg = "https communicate fail!" }
    elseif resp.body ~= nil then
        local ok
        local res
        ok, res = pcall(cjson.decode, resp.body)
        if ok and res.info ~= nil and type(res.info) == 'boolean' and res.info == true then
            local current_mode = c:get("mptun", "global", "service_mode")
            if current_mode == 'IPRB' then
                c:delete("mptun", "global", "service_mode")
            end
            c:delete("mptun", "IPRB")
            c:commit("mptun")

            push_status_to_wb()
            return {}
        elseif res.msg ~= nil then
            return { err_code = -3, err_msg = res.msg }
        end
    end
    return { err_code = -2, err_msg = "reset fail !" }
end

--[[
    @method-type: call
    @method-name: get_started
    @method-desc: Select a role and save it as the startup intent.
    @in string      attempt_to_be       The role want to set(primary or secondary)
    @out number     ?err_code   Error Code
    @out string     ?err_msg    Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "get_started",{"attempt_to_be": "primary"}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
function M.get_started(params)
    local attempt_to_be
    if params ~= nil and params.attempt_to_be ~= nil and type(params.attempt_to_be) == "string" then
        if params.attempt_to_be == 'primary' or params.attempt_to_be == 'secondary' then
            attempt_to_be = params.attempt_to_be
        else
            return { err_code = -1, err_msg = "invalid params !" }
        end
    else
        return { err_code = -1, err_msg = "invalid params !" }
    end
    local opt = {
        attempt_to_be = attempt_to_be
    }
    set_IPRB_option(opt)
    push_status_to_wb()
    return {}
end

--[[
    @method-type: call
    @method-name: clean_kicked_out_status
    @method-desc: clean option be_kicked_out.
    @out number     ?err_code   Error Code
    @out string     ?err_msg    Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "clean_kicked_out_status",{}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
function M.clean_kicked_out_status(params)
    local c = uci.cursor()
    c:delete("mptun", "IPRB", "be_kicked_out")
    c:commit("mptun")

    push_status_to_wb()
    return {}
end

local function pre_req_deal_identity_verification(req_path, params)
    local c = uci.cursor()
    local enabled
    local is_active = nil
    local service_mode = c:get("mptun", "global", "service_mode") or ''
    local active = c:get("mptun", "IPRB", "active") or ''

    if service_mode == 'IPRB' then
        enabled = IPRB_is_enabled()
        if active == '1' then
            is_active = true
        else
            if active == '0' then
                is_active = false
            end
        end

        -- Synchronization will only be performed once both the local and cloud-based enabled statuses are clear.
        if enabled ~= nil and is_active ~= nil and enabled ~= is_active then
            if enabled then
                send_net_ctrl_msg('start')
            else
                send_net_ctrl_msg('stop')
            end
        end
    end
    return 0
end
local function parse_response_identity_verification(resp)
    local ok = false
    local res_table
    if resp.body ~= nil then
        ok, res_table = pcall(cjson.decode, resp.body)
    end
    if not ok or res_table == nil then
        return -1
    end

    local net_id, is_primary, is_active, is_use_exit
    local is_IPRB = false
    local parse_fail = false
    if res_table ~= nil and res_table.info ~= nil and type(res_table.info) == "table" then
        if res_table.info.networkPlan ~= nil and type(res_table.info) == "string" then
            if res_table.info.networkPlan:match("^IPRB(_%w+)?$") ~= nil then
                is_IPRB = true
            end
        end
        if res_table.info.planCode ~= nil and type(res_table.info.planCode) == "string" then
            local code = tonumber(res_table.info.planCode)
            if code then
                if code == 7 or code == 8 then
                    is_IPRB = true
                end
            end
        end
        if res_table.info.networkId ~= nil and type(res_table.info.networkId) == "string" then
            net_id = res_table.info.networkId:match("^%s*(.-)%s*$")
            if string.match(net_id, valid_query_and_body_params["networkId"]) ~= net_id then
                parse_fail = true
            end
        else
            parse_fail = true
        end
        if res_table.info.primaryDevice ~= nil and type(res_table.info.primaryDevice) == "boolean" then
            is_primary = res_table.info.primaryDevice
        else
            parse_fail = true
        end
        if res_table.info.enableStatus ~= nil and type(res_table.info.enableStatus) == "boolean" then
            is_active = res_table.info.enableStatus
        else
            parse_fail = true
        end
        if res_table.info.useInternetExit ~= nil and type(res_table.info.useInternetExit) == "boolean" then
            is_use_exit = res_table.info.useInternetExit
        end
    else
        return -1
    end
    if is_IPRB ~= true then
        return 1
    end
    if parse_fail == true then
        return -2
    end
    local mode = "classic"
    local identity
    local active
    local use_exit = '0'
    if is_IPRB == true then
        mode = "IPRB"
    end
    if is_primary == true then
        identity = "primary"
    else
        identity = "secondary"
        if is_use_exit == true then
            use_exit = "1"
        else
            use_exit = "0"
        end
    end
    if is_active == true then
        active = "1"
    else
        active = "0"
    end

    return {
        err_code = 0,
        mode = tostring(mode),
        identity = tostring(identity),
        active = tostring(active),
        net_id = tostring(net_id),
        use_exit = tostring(use_exit)
    }
end

local function snoop_response_identity_verification(resp)
    local ret = parse_response_identity_verification(resp)
    if type(ret) == "table" and ret.err_code == 0 then
        if ret.mode ~= nil and (ret.mode == 'classic' or ret.mode == 'IPRB') then
            set_service_mode(tostring(ret.mode))
        end
        if ret.mode == 'IPRB' then
            local c = uci.cursor()
            local opt = {}
            opt.identity = tostring(ret.identity)
            opt.active = tostring(ret.active)
            opt.net_id = tostring(ret.net_id)
            opt.use_exit = tostring(ret.use_exit)

            -- If the local enabled status is unclear, update it according to the cloud status.
            local disabled = c:get("mptun", "IPRB", "disabled")
            if disabled == nil and (ret.active == '1' or ret.active == '0') then
                if ret.active == '1' then
                    opt.disabled = tostring('0')
                else
                    opt.disabled = tostring('1')
                end
            end
            set_IPRB_option(opt)

            push_status_to_wb()
        end
    end
end
local function snoop_response_access_by_code(resp)
    local ok = false
    local res_table
    if resp.body ~= nil then
        ok, res_table = pcall(cjson.decode, resp.body)
    end
    if not ok or res_table == nil then
        return -1
    end
    if res_table ~= nil and res_table.info ~= nil and type(res_table.info) == "table" then
        if res_table.info.verifyResult == nil or type(res_table.info.verifyResult) ~= "boolean" or res_table.info.verifyResult ~= true then
            return -3
        end
    end

    -- write configuration.
    local opt = {}
    opt.identity = "secondary"
    opt.disabled = '0'
    set_IPRB_option(opt)
    push_status_to_wb()
    return 0
end

local function snoop_response_get_access_code(resp)
    local ok = false
    local res_table
    if resp.body then
        ok, res_table = pcall(cjson.decode, resp.body)
    end
    if not ok or res_table == nil then
        return -1
    end
    if res_table == nil or res_table.code == nil or res_table.code ~= 0 then
        return -2
    end

    -- write configuration.
    local opt = {}
    opt.identity = "primary"

    set_IPRB_option(opt)
    return 0
end

--[[
    @method-type: call
    @method-name: get_node_info
    @method-desc: get device identity for cloud.
    @out string     info.mode           currently service_mode
    @out string     info.identity       currently identity
    @out string     info.active         cloud allow enabling or not
    @out string     info.use_exit       for secondary, use exit node or not
    @out string     ?info.net_id        id of this network
    @out number     info.err_code       parse result code inside info, 0 means success
    @out number     ?err_code   Error Code
    @out string     ?err_msg    Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "get_node_info",{}]}
    @out-example: {"id":15,"jsonrpc":"2.0","result":{"info":{"mode":"IPRB","use_exit":"0","active":"1","err_code":0,"identity":"primary","net_id":"7c3e7da5dc2413e8e8ff7f6bdde31cab"}}}
--]]
function M.get_node_info(params)
    local ret
    local resp = get_info_from_cloud()
    if resp ~= nil and resp.err_code == 0 and resp.status_code ~= nil and resp.body ~= nil then
        local opt = {
            status_code = resp.status_code,
            body = resp.body
        }
        ret = parse_response_identity_verification(opt)
        if type(ret) ~= "table" then
            return { err_code = -2, err_msg = "fail!" }
        end
    else
        return { err_code = -1, err_msg = "fail!" }
    end
    return { info = ret }
end
--[[
    @method-type: call
    @method-name: report_event_to_mptun
    @method-desc: The mptun module is notified that certain events have occurred.
    @in string      class       Event Type
    @in string      msg         Brief description of the event
    @in object      info        Detailed information about the relevant events
    @out number     ?err_code   Error Code
    @out string     ?err_msg    Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "report_event_to_mptun",{"class":"cloud","msg":"cloud service closed","info":{"event_code":11}}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {} }
--]]
function M.report_event_to_mptun(params)
    if params.class == nil or params.info == nil then
        return {}
    end
    local event_class = params.class
    local event_info = params.info
    --local event_msg = params.msg or ''
    if event_class == 'cloud' then
        local code = event_info.event_code
        if code == nil or type(code) ~= 'number' then
            return {}
        end
        if code == 11 then
            -- code 11 is cloud service stoped.
            local is_enable = IPRB_is_enabled() or false
            if is_enable then
                local c = uci.cursor()
                -- stop mptun
                c:set("mptun", "global", "enable", "0")
                c:commit("mptun")
                ngx.timer.at(2.0, function()
                    ngx.pipe.spawn({ "/etc/init.d/mptun", "stop" }):wait()
                end)
            end
        elseif code == 10 then
            -- code 10 is cloud service restored.
            local c = uci.cursor()
            local service_mode = c:get("mptun", "global", "service_mode") or ''
            local identity = c:get("mptun", "IPRB", "identity") or ''
            if service_mode == "IPRB" then
                if identity == 'primary' then
                    __start_service()
                elseif identity == 'secondary' then
                    __start_connect()
                end
            end
        end
    end
    return {}
end

--[[
    @method-type: call
    @method-name: is_meet_startup_conditions
    @method-desc: Check if the startup conditions are met..
    @out number     ?err_code   Error Code
    @out string     ?err_msg    Error Message
    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["", "mptun", "is_meet_startup_conditions"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {} }
--]]
function M.is_meet_startup_conditions()
    local ret = __is_meet_startup_conditions()
    if ret == true then
        return {}
    else
        return { err_code = -1 }
    end
end
pre_req_func[req_path["req_get_identity"]] = pre_req_deal_identity_verification
snoop_func[req_path["req_get_identity"]] = snoop_response_identity_verification
snoop_func[req_path["req_access_by_code"]] = snoop_response_access_by_code
snoop_func[req_path["req_get_access_code"]] = snoop_response_get_access_code

return M

