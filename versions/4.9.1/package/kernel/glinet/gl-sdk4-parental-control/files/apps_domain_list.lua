local fs = require "oui.fs"
local utils = require "oui.utils"
local uci = require "uci"
local cjson = require "cjson"

local c = uci.cursor()

local function str_action_num(action_str)
    local action_map = {
        DROP = 0,
        ACCEPT = 1,
        POLICY_DROP = 2,
        POLICY_ACCEPT = 3
    }

    return action_map[action_str] or 0
end

local function url_to_app_feature(input)
    if not input or input == "" then
        return ""
    end

    if input:sub(1, 1) == "[" then
        local result = input:gsub("%[", ""):gsub("%]", "")
        result = result:gsub("|", "")
        result = result:gsub(",", " ")
        return result
    else
        return "tcp;;;" .. input .. ";;"
    end
end

local function update_dpi_black_list()
    local big_category_mappings = {}
    c:foreach("parental_control_apps", "category", function(section)
        local big_category_name = section[".name"]
        big_category_mappings[big_category_name] = section.category or {}
    end)

    local small_category_to_app_names = {}

    local id_to_app_name = {}
    local app_name_to_id = {}
    local domain_mappings = {}

    if fs.access("/etc/apps.conf") then
        for line in io.lines("/etc/apps.conf") do
            local parts = {}
            for part in line:gmatch("[^:]+") do
                parts[#parts + 1] = part
            end

            if #parts >= 3 then
                if parts[1] == "app" then
                    local app_id = parts[2]
                    local app_name = parts[3]:gsub("^netify%.", "")
                    id_to_app_name[app_id] = app_name
                    app_name_to_id[app_name] = app_id
                elseif parts[1] == "dom" then
                    local app_id = parts[2]
                    local domain = parts[3]
                    domain_mappings[app_id] = domain_mappings[app_id] or {}
                    table.insert(domain_mappings[app_id], domain)
                end
            end
        end
    end

    local json_content = utils.readfile("/etc/categories.json")
    if json_content then
        local success, data = pcall(cjson.decode, json_content)
        if success and data then
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

                small_category_to_app_names[small_category_name] = small_category_to_app_names[small_category_name] or {}
                for _, app_id in ipairs(app_ids) do
                    local id_str = tostring(app_id)
                    local app_name = id_to_app_name[id_str]
                    if app_name then
                        small_category_to_app_names[small_category_name][app_name] = true
                    end
                end
            end
        end
    end

    c:foreach("parental_control_v2", "group", function(s)
        local rule_name = c:get("parental_control_v2", s[".name"], "default_rule")
        if rule_name == nil then
            return
        end
        local dpi_category = c:get("parental_control_v2", rule_name, "category") or {}
        local dpi_black_list_conf = "/tmp/" .. rule_name .. ".conf"
        utils.writefile(dpi_black_list_conf, "")

        if s.enabled == '1' then
            local target_app_names = {}
            local file = "/etc/parental_control_v2/" .. rule_name .. "apps.conf"
            local file_content = utils.readfile(file) or ""
            for line in file_content:gmatch("[^\r\n]+") do
                if line ~= "" then
                    target_app_names[line] = true
                end
            end

            for _, big_cat in ipairs(dpi_category) do
                if big_cat ~= "" then
                    local small_categories = big_category_mappings[big_cat] or {}
                    for _, small_cat in ipairs(small_categories) do
                        local apps_in_category = small_category_to_app_names[small_cat] or {}
                        for app_name, _ in pairs(apps_in_category) do
                            target_app_names[app_name] = true
                        end
                    end
                end
            end

            local target_domains = {}
            file = "/etc/parental_control_v2/" .. rule_name .. "blacklist.conf"
            file_content = utils.readfile(file) or ""
            for line in file_content:gmatch("[^\r\n]+") do
                if line ~= "" then
                    target_domains[line] = true
                end
            end

            if next(target_app_names) ~= nil then
                for app_name, _ in pairs(target_app_names) do
                    local app_id = app_name_to_id[app_name]
                    if app_id and domain_mappings[app_id] then
                        for _, domain in ipairs(domain_mappings[app_id]) do
                            target_domains[domain] = true
                        end
                    end
                end
            end

            local action = c:get("parental_control_v2", rule_name, "action") or "ACCEPT"
            local action_num = str_action_num(action)
            if next(target_domains) ~= nil then
                local rule_data = {
                    id = rule_name,
                    action = action_num,
                    blacklist = {}
                }

                for domain, _ in pairs(target_domains) do
                    domain = url_to_app_feature(domain)
                    rule_data.blacklist[#rule_data.blacklist + 1] = domain
                end

                local add_rule = 1
                local config_json = {
                    op = add_rule,
                    data = {
                        rules = { rule_data }
                    }
                }

                local json_content = cjson.encode(config_json)
                utils.writefile(dpi_black_list_conf, json_content)
            end
        end
    end)
end

local function main()
    update_dpi_black_list()
end

main()
