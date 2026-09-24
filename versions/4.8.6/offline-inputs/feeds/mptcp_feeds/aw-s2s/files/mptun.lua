--[[
    @object-name: mptun
    @object-desc: mptun settings
--]]

local uci = require "uci"
local fs = require "oui.fs"
local ubus = require "oui.ubus"
local utils = require "oui.utils"
local cjson = require "cjson"

local M = {}

local ERR_PARAMETER = 1


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

return M

