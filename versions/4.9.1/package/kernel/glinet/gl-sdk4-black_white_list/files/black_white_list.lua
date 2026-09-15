--[[
    @object-name: black_white_list
    @object-desc: black and white list settings
--]]

local uci = require "uci"
local fs  = require "oui.fs"
local rpc = require 'oui.rpc'
local utils = require 'oui.utils'
local ubus = require "oui.ubus"

local M = {}

local MAC_TYPE_ERR     = -1   --MAC address conflict
local WHITE_INFO_ERR     = -2   --whitelist information error
--[[
    @method-type: call
    @method-name: get_config
    @method-desc: get blacklist and whitelist configuration information

    @out string mode    [black:blacklist; white:whitelist]
    @out array mac      the mac address configured under the black and white list

    @in-example:  {"jsonrpc":"2.0","id":1,"method":"call","params":["","black_white_list","get_config", {}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"mode": "black", "black_mac": ["08:10:7B:B3:E8:92","94:83:C4:0C:6D:D6"],"white_mac":["00:4E:36:27:15:20"]}}
--]]


function M.get_config()
    local c = uci.cursor()
    local mode = c:get("gl-black_white_list", "global", "mode")
    local black_mac = c:get("gl-black_white_list", "black", "mac")
    local white_mac = c:get("gl-black_white_list", "white", "mac")

    local info = {
        mode = mode,
        black_mac = black_mac,
        white_mac = white_mac
    }
    return info
end

local function check_mac_in_config(mac, mac_list)
    if type(mac_list) == "table" then
        for k, v in pairs(mac_list) do
            if v == mac then
                return k
            end
        end
    end
    return nil
end

--[[
    @method-type: call
    @method-name: set_single_mac
    @method-desc: set the configuration information of a single mac

    @in string    mode          [black:blacklist; white:whitelist]
    @in string    operate       [add:add configuration information; del:delete configuration information]
    @in string    mac           manipulated MAC address.

    @out number   ?err_code     Error code
    @out number   ?err_msg      Error message
    @in-example:  {"jsonrpc":"2.0","method":"call","params":["","black_white_list","set_single_mac",{"mode":"black", "operate":"add", "mac":"94:83:C4:0C:6D:D6"}],"id":1}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": null}
--]]
function M.set_single_mac(params)
    local c = uci.cursor()
    local mode = params.mode
    local op = params.operate
    local mac = params.mac

    local mac_list = c:get("gl-black_white_list", mode, "mac") or {}
    local pos = check_mac_in_config(mac, mac_list)
    if op == "add" and not pos then
        mac_list[#mac_list+1] = mac
    elseif op == "del" then
        while pos do
            table.remove(mac_list, pos)
            pos = check_mac_in_config(mac, mac_list)
        end
    end

    c:delete("gl-black_white_list", mode, "mac")
    if #mac_list ~= 0 then
        c:set("gl-black_white_list", mode, "mac", mac_list)
    end

    c:commit("gl-black_white_list")
    ngx.pipe.spawn({"/etc/init.d/gl-black_white_list","start"})
    fs.sync()
end

--[[
    @method-type: call
    @method-name: set_config
    @method-desc: set blacklist or whitelist

    @in string    mode        [black:blacklist; white:whitelist]
    @in array     mac         the mac address list set in the corresponding mode.

    @out number   ?err_code   Error code,-2 : whitelist information is empty; -1: mac type err.
    @out number   ?err_msg    Error message

    @in-example:  {"jsonrpc":"2.0","method":"call","params":["","black_white_list","set_config",{"mode":"black", "mac":["08:10:7B:B3:E8:92","94:83:C4:0C:6D:D6"]}],"id":1}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": null}
--]]
function M.set_config(params)
    local c = uci.cursor()
    local mode = params.mode
    local remote_mac
    local flag = false

    if type(params.mac) ~= "table" then
        return {err_code = MAC_TYPE_ERR, err_msg="mac required"}
    end

    if mode and mode ~= 'black' and mode ~= 'white' then
        return rpc.ERROR_CODE_INVALID_PARAMS
    end

    c:set("gl-black_white_list", "global", "mode", mode)
    if mode == "black" then
        if #params.mac ~= 0 then
            c:set("gl-black_white_list", "black", "mac", params.mac)
        else
            c:delete("gl-black_white_list", "black", "mac")
        end
    else
        if #params.mac == 0 then
            return {err_code = -WHITE_INFO_ERR, err_msg="white_list is null"}
        else
            --support goodcloud set white list
            if ngx.var.remote_addr == "127.0.0.1" then
                flag = true
            else
                local r = utils.get_client_data_by_socket("list") or {}
                for mac, info in pairs(r.clients or {}) do
                    if info.ip == ngx.var.remote_addr and info.online == true then
                        remote_mac = mac
                        break
                    end
                end

                for _, mac in ipairs(params.mac) do
                    if mac == remote_mac then
                        flag = true
                        break
                    end
                end
            end

            if flag then
                c:set("gl-black_white_list", "white", "mac", params.mac)
                ubus.send('white_list_changed', {})
            else
                return {err_code = -WHITE_INFO_ERR, err_msg="white_list doesn't have a native mac"}
            end
        end
    end

    c:commit("gl-black_white_list")
    ngx.pipe.spawn({"/etc/init.d/gl-black_white_list","start"})
    fs.sync()
end

return M
