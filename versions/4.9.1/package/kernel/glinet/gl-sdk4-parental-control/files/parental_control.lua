--[[
    @object-name: parental-control
    @object-desc: parental control
--]]

local M = {}

local uci = require "uci"
local utils = require "oui.utils"
local fs = require "oui.fs"
local cjson = require "cjson"

local function apply()
    os.execute("/etc/init.d/parental_control restart")
end

local function random_uci_section(uci_cursor,uci_type)
    local n
    local section
    math.randomseed(os.time())
    for _=1,1000,1 do
        n = math.random(1000000,9999999)
        section = uci_type .. tostring(n)
        if not uci_cursor:get("parental_control_v2", section) then
            return section
        end
    end
    return nil
end

local function get_long_list(list)
    local valid_arr = {}
    local long_list = false
    for i = 1, #list do
        if #list[i] > 253 then
            valid_arr[#valid_arr+1]=list[i]
            long_list = true
        end
    end
    return long_list, valid_arr
end

local function handle_url(list)
    local valid_arr = {}
    local handle_url = false
    for _, s in pairs(list) do
        if string.sub(s, 1, 7) == "http://" then
            valid_arr[#valid_arr+1] = string.sub(s, 8):match('([%w-%.:]+)/') or string.sub(s, 8)
            handle_url = true
        elseif string.sub(s, 1, 8) == "https://" then
            valid_arr[#valid_arr+1] = string.sub(s, 9):match('([%w-%.:]+)/') or string.sub(s, 9)
            handle_url = true
        else
            valid_arr[#valid_arr+1] = s
        end
    end
    return handle_url, valid_arr
end

local function validate_schedule(sched)
    local week = tonumber(sched.week)
    if week then
        if week % 1 ~= 0 then
            return {
                err_code = -2,
                err_msg = "schedule week invalid, must be integer"
            }
        end
        sched.week = math.floor(week)
    end
    if not week or week < 0 or week > 7 then
        return {
            err_code = -2,
            err_msg = "schedule week invalid, expect 0-7"
        }
    end

    if type(sched.begin) ~= "string" then
        return {
            err_code = -2,
            err_msg = "schedule begin must be string"
        }
    end
    local time_str = sched.begin
    local _, colons = time_str:gsub(":", "")
    local bh, bm, sh
    if colons >= 2 then
        bh, bm, sh = time_str:match("^(%d%d):(%d%d):(%d%d)$")
        if not bh or not bm or not sh then
            return {
                err_code = -2,
                err_msg = "schedule begin format invalid, expect hh:mm or hh:mm:ss"
            }
        end
        sh = tonumber(sh)
        if sh > 59 then
            return {
                err_code = -2,
                err_msg = "schedule begin time out of range"
            }
        end
    else
        bh, bm = time_str:match("^(%d%d):(%d%d)$")
        if not bh or not bm then
            return {
                err_code = -2,
                err_msg = "schedule begin format invalid, expect hh:mm or hh:mm:ss"
            }
        end
    end
    bh, bm = tonumber(bh), tonumber(bm)
    if sh then
        sched.begin = string.format("%02d:%02d:%02d", bh, bm, sh)
    else
        sched.begin = string.format("%02d:%02d", bh, bm)
    end
    if bh > 23 or bm > 59 then
        return {
            err_code = -2,
            err_msg = "schedule begin time out of range"
        }
    end

    if type(sched["end"]) ~= "string" then
        return {
            err_code = -2,
            err_msg = "schedule end must be string"
        }
    end
    local time_str = sched["end"]
    local _, colons = time_str:gsub(":", "")
    local eh, em, sh
    if colons >= 2 then
        eh, em, sh = time_str:match("^(%d%d):(%d%d):(%d%d)$")
        if not eh or not em or not sh then
            return {
                err_code = -2,
                err_msg = "schedule end format invalid, expect hh:mm or hh:mm:ss"
            }
        end
        sh = tonumber(sh)
        if sh > 59 then
            return {
                err_code = -2,
                err_msg = "schedule end time out of range"
            }
        end
    else
        eh, em = time_str:match("^(%d%d):(%d%d)$")
        if not eh or not em then
            return {
                err_code = -2,
                err_msg = "schedule end format invalid, expect hh:mm or hh:mm:ss"
            }
        end
    end
    eh, em = tonumber(eh), tonumber(em)
    if sh then
        sched["end"] = string.format("%02d:%02d:%02d", eh, em, sh)
    else
        sched["end"] = string.format("%02d:%02d", eh, em)
    end
    if eh > 23 or em > 59 then
        return {
            err_code = -2,
            err_msg = "schedule end time out of range"
        }
    end

    return nil
end

--[[
    @method-type: call
    @method-name: get_app_list
    @method-desc: Get a list of recognized applications.

    @out  array     apps                  Application
    @out  string    category_name         Application category name
    @out  string    icon                  Application icon
    @out  string    name                  Application name
    @out  string    label                 Application label

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","get_app_list"]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {"apps":[{"icon":"xxxx","name":"xxxx","label":"xxxx"}],"category_name":"advertisement"}}
--]]
M.get_app_list = function()
    local apps = {}
    local c = uci.cursor()
    local result = {}
    local conf_content = utils.readfile("/etc/apps.conf")
    if not conf_content then
        return nil, "not find apps.conf"
    end

    local file = utils.readfile("/etc/app-metadata.json")
    if file == nil then
        ngx.log(ngx.ERR, "The app-metadata parsing failed")
        return nil
    end
    local success, data = pcall(cjson.decode, file)
    if not success then
        ngx.log(ngx.ERR, "The app-metadata parsing failed")
        return nil
    end

    local metadata_index = {}
    for _, item in ipairs(data) do
        if item.id then
            metadata_index[tonumber(item.id)] = {
                label = item.label or "Unknown",
                icon = item.icon and item.icon:gsub("\\/", "/") or "Unknown"
            }
        end
    end

    for line in conf_content:gmatch("[^\r\n]+") do
        local id, name = line:match("app:(%d+):([%w%._-]+)")
        if id and name then
            name = name:gsub("^netify%.", "")
            local num_id = tonumber(id)
            local app_metadata = metadata_index[num_id]
            apps[id] = {
                name = name,
                label = app_metadata.label and app_metadata.label or "Unknown",
                icon = app_metadata.icon and app_metadata.icon:gsub("\\/", "/") or "Unknown",
            }
        end
    end

    local json_content = utils.readfile("/etc/categories.json")
    if not json_content then
        return nil, "not find categories.json"
    end

    local data = cjson.decode(json_content)
    if not data then
        return nil, "JSON decode error"
    end

    local big_categories = {}

    c:foreach("parental_control_apps", "category", function(s)
        if s.category then
            local big_category_name = s[".name"]
            big_categories[big_category_name] = big_categories[big_category_name] or {}

            for _, small_cat in ipairs(s.category) do
                if big_category_name == "sexual_content" or big_category_name == "gambling" or big_category_name == "malicious_content" then
                    result[#result + 1] = {
                        category_name = big_category_name,
                        apps = {}
                    }
                end
                big_categories[big_category_name][small_cat] = true
            end
        end
    end)

    local small_category_to_apps = {}
    local category_tag_index = data.application_tag_index or {}

    local application_index = data.application_index or {}

    for _, item in ipairs(application_index) do
        local cat_id = tostring(item[1])
        local app_ids = item[2] or {}

        local small_category_name = "unknown"
        for tag, id in pairs(category_tag_index) do
            if tostring(id) == cat_id then
                small_category_name = tag:gsub("^netify%.", "")
                break
            end
        end

        small_category_to_apps[small_category_name] = small_category_to_apps[small_category_name] or {}
        for _, app_id in ipairs(app_ids) do
            local id_str = tostring(app_id)
            if apps[id_str] then
                small_category_to_apps[small_category_name][#small_category_to_apps[small_category_name] + 1] = apps[id_str]
            end
        end
    end

    for big_category_name, small_categories in pairs(big_categories) do
        local all_apps = {}
        local app_set = {}

        for small_cat, _ in pairs(small_categories) do
            if small_category_to_apps[small_cat] then
                for _, app_obj in ipairs(small_category_to_apps[small_cat]) do
                    if not app_set[app_obj.name] then
                        all_apps[#all_apps + 1] = app_obj
                        app_set[app_obj.name] = true
                    end
                end
            end
        end

        table.sort(all_apps, function(a, b)
            return a.name < b.name
        end)

        if big_category_name == "sexual_content" or big_category_name == "gambling" or big_category_name == "malicious_content" then
            all_apps = {}
        end
        if #all_apps > 0 then
            result[#result + 1] = {
                category_name = big_category_name,
                apps = all_apps
            }
        end
    end

    table.sort(result, function(a, b)
        return a.category_name < b.category_name
    end)

    return result
end

--[[
    @method-type: call
    @method-name: add_group
    @method-desc: Add device group.

    @in string   name group name
    @in array    macs The device MAC address list contained in the group, which is a string type.
    @in bool     enabled The group switch.
    @in bool     schedules_enabled Time limit switch.
    @in string   ?schedules_days Time limit method.
    @in number   ?schedules.week The day of the week for the schedule. 0 means everyday, 1-7 correspond to Monday through Sunday.
    @in string   ?schedules.begin The start time of the schedule, the format is hh:mm or hh:mm:ss.
    @in string   ?schedules.end The end time of the schedule, the format is hh:mm or hh:mm:ss.
    @in bool     ?schedules.enabled Single time rule switch.
    @in string   ?blacklist Blacklist list.
    @in array    ?apps App list.
    @in array    ?category Category list.

    @out string   id The ID of the new group
    @out number ?err_code     Error code (-1: missing required parameters, -2: passed shedules but missing required parameters)
    @out string ?err_msg      Error message

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","add_group",{"name":"group1","enabled":true,"macs":["98:6B:46:F0:9B:A4","98:6B:46:F0:9B:A5"],"schedules_enabled":true,"schedules_days":"everyday","schedules":[{"week":"1","begin":"12:00","end":"13:00","enabled":true}],"blacklist":["baidu.com"],"apps":["baidu"],"category":[""]}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
M.add_group = function(params)
    if params.name == nil or params.macs == nil or params.schedules_enabled == nil or params.enabled == nil then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end
    if params.schedules_enabled and params.schedules_days == nil then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end

    local c = uci.cursor()

    local group_count = 0
    c:foreach("parental_control_v2", "group", function(s)
        group_count = group_count + 1
    end)

    if group_count >= 10 then
        return {
            err_code = -1,
            err_msg = "the maximum number of groups has reached the limit of 10"
        }
    end

    local group_sid = c:add("parental_control_v2", "group")
    local nsid = random_uci_section(c,"group") or group_sid;
    c:rename("parental_control_v2", group_sid, nsid)
    group_sid = nsid
    c:set("parental_control_v2", group_sid, "name", params.name)
    c:set("parental_control_v2", group_sid, "enabled", params.enabled and "1" or "0")
    c:set("parental_control_v2", group_sid, "schedules_enabled", params.schedules_enabled and "1" or "0")
    c:set("parental_control_v2", group_sid, "schedules_days", #params.schedules_days ~= 0 and params.schedules_days or '')
    if type(params.macs) == "table" and #params.macs ~= 0  then
        for i = 1, #params.macs do
            params.macs[i] = string.upper(params.macs[i])
        end
        c:set("parental_control_v2", group_sid, "macs",params.macs)
    end
    if type(params.schedules) == "table" and #params.schedules ~= 0  then
        for i = 1, #params.schedules do
            if params.schedules[i].week == nil or params.schedules[i].begin == nil or params.schedules[i]["end"] == nil or params.schedules[i].enabled == nil then
                return {
                    err_code = -2,
                    err_msg = "schedule parameter missing"
                }
            end

            local sched_err = validate_schedule(params.schedules[i])
            if sched_err then
                return sched_err
            end

            local sche = c:add("parental_control_v2", "schedule")
            local nsche = random_uci_section(c,"sche") or sche;
            c:rename("parental_control_v2", sche, nsche)
            sche = nsche
            c:set("parental_control_v2", sche, "enabled", params.schedules[i].enabled and "1" or "0")
            c:set("parental_control_v2", sche, "group", group_sid)
            c:set("parental_control_v2", sche, "week", params.schedules[i].week)
            c:set("parental_control_v2", sche, "begin", params.schedules[i].begin)
            c:set("parental_control_v2", sche, "end", params.schedules[i]["end"])
            c:set("parental_control_v2", sche, "rule", "drop")
        end
    end
    if type(params.apps) == "table" or type(params.blacklist) == "table" or type(params.category) == "table" then
        local sid = c:add("parental_control_v2", "rule")
        local nsid = random_uci_section(c, "rule") or sid;
        c:rename("parental_control_v2", sid, nsid)
        sid = nsid
        c:set("parental_control_v2", sid, "manual", params.manual == false and "0" or "1")
        c:set("parental_control_v2", sid, "action", "POLICY_DROP")
        if type(params.apps) == "table" and #params.apps ~= 0 then
            local content = ""
            for _, i in pairs(params.apps) do
                content = content .. i .. "\n"
            end
            local dpi_apps_list_conf = "/etc/parental_control_v2/" .. sid .. "apps.conf"
            utils.writefile(dpi_apps_list_conf, content)
        end
        if type(params.category) == "table" and #params.category ~= 0 then
            c:set("parental_control_v2", sid, "category", params.category)
        end
        if type(params.blacklist) == "table" and #params.blacklist ~= 0  then
            local long_list, valid_arr = get_long_list(params.blacklist)
            if long_list then
                return {
                    err_code = -2,
                    err_msg = "domain invalid",
                    valid_list = valid_arr
                }
            end

            local has_url, valid_arr = handle_url(params.blacklist)
            if has_url then
                return {
                    err_code = -2,
                    err_msg = "domain invalid",
                    valid_list = valid_arr
                }
            end
            local content = ""
            for _, i in pairs(params.blacklist) do
                content = content .. i .. "\n"
            end
            local dpi_black_list_conf = "/etc/parental_control_v2/" .. sid .. "blacklist.conf"
            utils.writefile(dpi_black_list_conf, content)
        end
        c:set("parental_control_v2", group_sid, "default_rule", sid)
    end
    c:set("parental_control_v2", "global", "init", "0")
    c:set("parental_control_v2", "global", "enable", "1")
    c:commit("parental_control_v2")
    fs.sync()
    apply()

    return {id=group_sid}
end

--[[
    @method-type: call
    @method-name: remove_group
    @method-desc: Remove a device group.

    @in string   id The ID of the group to be deleted. The ID of the group can be obtained through get_config.

    @out number ?err_code     Error code (-1: missing required parameter)
    @out string ?err_msg      Error message

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","remove_group",{"id":"cfga01234b"}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
M.remove_group = function(params)
    if params.id == nil then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end
    local c = uci.cursor()
    c:foreach("parental_control_v2", "schedule", function(s)
        if s.group and s.group ==params.id then
            c:delete("parental_control_v2", s[".name"])
        end
    end)
    local rule_name = c:get("parental_control_v2", params.id, "default_rule")
    local dpi_black_domain_conf = "/tmp/" .. rule_name .. ".conf"
    local dpi_black_list_conf = "/etc/parental_control_v2/" .. rule_name .. "blacklist.conf"
    local dpi_apps_list_conf = "/etc/parental_control_v2/" .. rule_name .. "apps.conf"
    c:delete("parental_control_v2", rule_name)
    os.remove(dpi_black_domain_conf)
    os.remove(dpi_black_list_conf)
    os.remove(dpi_apps_list_conf)
    c:delete("parental_control_v2", params.id)

    c:commit("parental_control_v2")
    fs.sync()
    apply()

    return {}
end

--[[
    @method-type: call
    @method-name: set_group
    @method-desc: Modify device group configuration.

    @in bool     ?enabled  Whether to enabled.
    @in string   id The group ID that needs to be set, and the group ID can be obtained through get_config.
    @in string   ?name group name
    @in array    ?macs The device MAC address list contained in the group, which is a string type.
    @in bool     ?schedules_enabled Time limit switch.
    @in string   ?schedules_days Time limit method.
    @in number   ?schedules.week The day of the week for the schedule. 0 means everyday, 1-7 correspond to Monday through Sunday.
    @in string   ?schedules.begin The start time of the schedule, the format is hh:mm or hh:mm:ss.
    @in string   ?schedules.end The end time of the schedule, the format is hh:mm or hh:mm:ss.
    @in bool     ?schedules.enabled Single time rule switch.
    @in string   ?blacklist Blacklist list.
    @in array    ?apps App list.
    @in array    ?category Category list.

    @out number ?err_code     Error code (-1: missing required parameters, -2: passed shedules but missing required parameters)
    @out string ?err_msg      Error message

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","set_group",{"id":"cfga01234b","enabled":true,"name":"group1","blacklist":["baidu.com"],"category":[""],"apps":["baidu"],"macs":["98:6B:46:F0:9B:A4","98:6B:46:F0:9B:A5"],"schedules_enabled":true,"schedules":[{"week":1,"begin":"12:00","end":"13:00","enabled":true}]}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
M.set_group = function(params)
    if params.id == nil  then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end
    local c = uci.cursor()

    local sid = params.id

    if params.enabled ~= nil then
        c:set("parental_control_v2", sid, "enabled", params.enabled and "1" or "0")
    end

    if params.schedules_enabled ~= nil then
        c:set("parental_control_v2", sid, "schedules_enabled", params.schedules_enabled and "1" or "0")
    end

    if params.schedules_days ~= nil then
        c:set("parental_control_v2", sid, "schedules_days", params.schedules_days)
    end

    if params.name ~= nil then
        c:set("parental_control_v2", sid, "name", params.name)
    end

    if params.macs ~= nil then
        if type(params.macs) == "table" and #params.macs ~= 0  then
            for i = 1, #params.macs do
                params.macs[i] = string.upper(params.macs[i])
            end
            c:set("parental_control_v2", sid, "macs",params.macs)
        else
            c:delete("parental_control_v2", sid, "macs")
        end
    end

    if params.schedules ~= nil then
        c:foreach("parental_control_v2", "schedule", function(s)
            if s.group and s.group ==params.id then
                c:delete("parental_control_v2", s[".name"])
            end
        end)
        if type(params.schedules) == "table" and #params.schedules ~= 0  then
            for i = 1, #params.schedules do
                if params.schedules[i].week == nil or params.schedules[i].begin == nil or params.schedules[i]["end"] == nil or params.schedules[i].enabled == nil then
                    return {
                        err_code = -2,
                        err_msg = "schedule parameter missing"
                    }
                end

                local sched_err = validate_schedule(params.schedules[i])
                if sched_err then
                    return sched_err
                end

                local sche = c:add("parental_control_v2", "schedule")
                local nsche = random_uci_section(c,"sche") or sche;
                c:rename("parental_control_v2", sche,nsche)
                sche = nsche
                c:set("parental_control_v2", sche, "enabled", params.schedules[i].enabled and "1" or "0")
                c:set("parental_control_v2", sche, "group", sid)
                c:set("parental_control_v2", sche, "week", params.schedules[i].week)
                c:set("parental_control_v2", sche, "begin", params.schedules[i].begin)
                c:set("parental_control_v2", sche, "end", params.schedules[i]["end"])
                c:set("parental_control_v2", sche, "rule", "drop")
            end
        end
    end

    local rule_name = c:get("parental_control_v2", params.id, "default_rule")
    if params.blacklist ~= nil then
        local dpi_black_list_conf = "/etc/parental_control_v2/" .. rule_name .. "blacklist.conf"
        if type(params.blacklist) == "table" and #params.blacklist ~= 0  then
            local long_list, valid_arr = get_long_list(params.blacklist)
            if long_list then
                return {
                    err_code = -2,
                    err_msg = "domain invalid",
                    valid_list = valid_arr
                }
            end

            local has_url, valid_arr = handle_url(params.blacklist)
            if has_url then
                return {
                    err_code = -2,
                    err_msg = "domain invalid",
                    valid_list = valid_arr
                }
            end
            local content = ""
            for _, i in pairs(params.blacklist) do
                content = content .. i .. "\n"
            end
            utils.writefile(dpi_black_list_conf, content)
        else
            os.remove(dpi_black_list_conf)
        end
    end

    if params.apps ~= nil then
        local dpi_apps_list_conf = "/etc/parental_control_v2/" .. rule_name .. "apps.conf"
        if type(params.apps) == "table" and #params.apps ~= 0  then
            local content = ""
            for _, i in pairs(params.apps) do
                content = content .. i .. "\n"
            end
            utils.writefile(dpi_apps_list_conf, content)
        else
            os.remove(dpi_apps_list_conf)
        end
    end

    if params.category ~= nil then
        if type(params.category) == "table" and #params.category ~= 0  then
            c:set("parental_control_v2", rule_name, "category", params.category)
        else
            c:delete("parental_control_v2", rule_name, "category")
        end
    end

    c:commit("parental_control_v2")
    fs.sync()
    apply()

    return {}
end

local function key_in_array(list,key)
    if list then
        for k, _ in pairs(list) do
          if k == key then
           return true
          end
        end
    end
end

--[[
    @method-type: call
    @method-name: get_config
    @method-desc: Get parental control configuration.

    @out bool     enable  Whether to enable or not.

    @out array    groups Device group list.
    @out string   groups.id Group ID, globally unique, used to distinguish different device groups.
    @out string   groups.name Group's name.
    @out string   groups.blacklist Group's blacklist.
    @out array    groups.macs A list of device MAC addresses contained in the group, which is a string type.
    @out array    groups.schedules The list of schedules contained in the group. If there is a schedule setting for the corresponding group, this parameter will be returned.
    @out number   groups.schedules.id Schedule's ID.
    @out number   groups.schedules.week The day of the week. 0 means everyday, 1-7 correspond to Monday through Sunday.
    @out string   groups.schedules.begin The start time of the schedule, the format is hh:mm or hh:mm:ss.
    @out string   groups.schedules.end The end time of the schedule, the format is hh:mm or hh:mm:ss.
    @out string   groups.schedules.rule The ID of the ruleset to be used by this schedule. The ID of the ruleset must correspond to the ID of the ruleset returned in the rules parameter.


    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","get_config"]}
    @out-example: {"id":1,"jsonrpc":"2.0","result":{"enable":true,"groups":[{"enabled":true,"apps":["qq"],"id":"group1198432","blacklist":["baidu.com"],"name":"group1","schedules_enabled":true,"schedules":[{"begin":"12:00:00","week":"1","enabled":true,"end":"13:00:00"}],"macs":["00:0F:C9:28:FC:7A"],"category":["sexual_content"]}],"init":true}}
--]]
M.get_config = function()
    local c = uci.cursor()
    local ret = {}
    local enable = c:get("parental_control_v2", "global", "enable") or '0'
    local init = c:get("parental_control_v2", "global", "init") or '1'
    local groups ={}
    c:foreach("parental_control_v2", "group", function(s)
        local group = {}
        group["id"] = s[".name"]
        group["name"] = s.name
        if s.macs then
            group["macs"] = s.macs
        end
        group["enabled"] = s.enabled ~= "0"
        group["schedules_enabled"] = s.schedules_enabled ~= "0"
        group["blacklist"] = {}
        group["apps"] = {}
        group["category"] = {}
        group["schedules_days"] = {}
        if s.default_rule then
            local dpi_black_list_conf = "/etc/parental_control_v2/" .. s.default_rule .. "blacklist.conf"
            local dpi_apps_list_conf = "/etc/parental_control_v2/" .. s.default_rule .. "apps.conf"
            local file_content = utils.readfile(dpi_black_list_conf) or ""
            for line in file_content:gmatch("[^\r\n]+") do
                if line ~= "" then
                    group["blacklist"][#group["blacklist"] + 1] = line
                end
            end
            file_content = utils.readfile(dpi_apps_list_conf) or ""
            for line in file_content:gmatch("[^\r\n]+") do
                if line ~= "" then
                    group["apps"][#group["apps"] + 1] = line
                end
            end
            group["category"] = c:get("parental_control_v2", s.default_rule, "category") or {}
        end
        if s.schedules_days then
            group["schedules_days"] = s.schedules_days
        end

        groups[#groups + 1] = group
    end)

    c:foreach("parental_control_v2", "schedule", function(s)
        local schedule = {}
        schedule["week"] = s.week
        schedule["begin"] = s.begin
        schedule["end"] = s["end"]
        schedule["enabled"] = s.enabled ~= '0'
        if #groups then
            for i=1,#groups do
                if groups[i]["id"] == s.group then
                    if not key_in_array(groups[i],"schedules") then
                        groups[i]["schedules"] ={}
                    end
                    groups[i]["schedules"][#groups[i]["schedules"]+1] = schedule
                    break
                end
            end
        end
    end)

    ret["enable"] = enable ~= "0"
    ret["groups"] = groups
    ret["init"] = init == "1"

    return ret
end


--[[
    @method-type: call
    @method-name: set_config
    @method-desc: Set the base configuration.
    @in bool     enable  Whether to enable.

    @out number ?err_code     Error code (-1: missing required parameter)
    @out string ?err_msg      Error message

    @in-example: {"jsonrpc":"2.0","id":1,"method":"call","params":["","parental-control","set_config",{"enable":true}]}
    @out-example: {"jsonrpc": "2.0", "id": 1, "result": {}}
--]]
M.set_config = function(params)
    if params.enable == nil then
        return {
            err_code = -1,
            err_msg = "parameter missing"
        }
    end
    local c = uci.cursor()

    c:set("parental_control_v2", "global", "enable", params.enable and "1" or "0")
    c:commit("parental_control_v2")
    fs.sync()
    apply()

    return {}
end

return M
