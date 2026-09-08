#!/usr/bin/lua
-- Alternative for OpenWrt's /sbin/wifi.
-- Copyright Not Reserved.
-- Hua Shao <nossiac@163.com>

package.path = '/lib/wifi/?.lua;'..package.path


local mtkdat = require("mtkdat")
local nixio = require("nixio")
local hostapd = require("hostapd")
local supplicant = require("supplicant")

local function esc(x)
   return (x:gsub('%%', '%%%%')
            :gsub('^%^', '%%^')
            :gsub('%$$', '%%$')
            :gsub('%(', '%%(')
            :gsub('%)', '%%)')
            :gsub('%.', '%%.')
            :gsub('%[', '%%[')
            :gsub('%]', '%%]')
            :gsub('%*', '%%*')
            :gsub('%+', '%%+')
            :gsub('%-', '%%-')
            :gsub('%?', '%%?'))
end

local function add_vif_into_net(vif, net)
    if net == nil then
        return
    end
    nixio.syslog("debug", "add "..vif.." into br-"..net)
    os.execute("ubus -t 20 wait_for network.interface."..net..
        " && ubus call network.interface."..net.." add_device \"{\\\"name\\\":\\\""..vif.."\\\"}\"")
    if mtkdat.exist("/proc/sys/net/ipv6/conf/"..vif.."/disable_ipv6") then
        os.execute("echo 0 > /proc/sys/net/ipv6/conf/"..vif.."/disable_ipv6")
    end
end


local function del_vif_from_net(vif)
    for _,f in ipairs(string.split(mtkdat.read_pipe("ls /sys/class/net/"..vif.." 2> /dev/null"), "\n")) do
        if string.match(f, "upper_(%a+)") then
            if mtkdat.exist("/proc/sys/net/ipv6/conf/"..vif.."/disable_ipv6") then
                os.execute("echo 1 > /proc/sys/net/ipv6/conf/"..vif.."/disable_ipv6")
            end
            local bridge = string.split(f, "_")[2]
            if bridge then
                nixio.syslog("debug", "remove "..vif.." into "..bridge)

                local net = string.split(bridge, "-")[2]
                if net then
                    os.execute("ubus call network.interface."..net.." remove_device \"{\\\"name\\\":\\\""..vif.."\\\"}\"")
                end
                os.execute("brctl delif "..bridge.." "..vif.." 2> /dev/null")
                return
            end
        end
    end
end

local function del_vif_from_l3_net(vif)
    local uci = mtkdat.uci_load_wireless()
    local uci_vif = mtkdat.get_uci_vif_by_vif_name(uci, vif)
    if uci_vif and uci_vif.network then
        if mtkdat.exist("/proc/sys/net/ipv6/conf/"..vif.."/disable_ipv6") then
            os.execute("echo 0 > /proc/sys/net/ipv6/conf/"..vif.."/disable_ipv6")
        end
        nixio.syslog("debug", "mtwifi, del "..vif.." from "..uci_vif.network)
        os.execute("ubus call network.interface."..uci_vif.network.." remove_device \"{\\\"name\\\":\\\""..vif.."\\\"}\"")
    end
end

function cfg80211_tool()
    if mtkdat.exist("/usr/sbin/mwctl") then
        return "/usr/sbin/mwctl"
    end
    if mtkdat.exist("/usr/sbin/iwpriv") then
        return "/usr/sbin/iwpriv"
    end
    return nil
end

function mtwifi_setup_apcli_vif_preinit(uci_dev, uci_vif)
    if uci_vif.mwds ~= nil then
        if uci_vif.mwds ~= "0" then
            os.execute(cfg80211_tool().." "..uci_vif.ifname.." set mwds enable=1")
        else
            os.execute(cfg80211_tool().." "..uci_vif.ifname.." set mwds enable=0")
        end
    end
end

function mtwifi_setup_apcli_vif_postinit(uci_dev, uci_vif)
end

function mtwifi_setup_ap_vif_postinit(uci_dev, uci_vif)
    if uci_vif.mwds ~= nil then
        if uci_vif.mwds ~= "0" then
            os.execute(cfg80211_tool().." "..uci_vif.ifname.." set mwds enable=1")
        else
            os.execute(cfg80211_tool().." "..uci_vif.ifname.." set mwds enable=0")
        end
    end
end

local function mtwifi_main_dev_up(uci_dev, dev, uci_main_vif)
    if mtkdat.exist("/sys/class/net/"..dev.main_ifname) then
        if not mtkdat.exist("/etc/init.d/wpad") or ( dev.TestModeEn ~= nil and dev.TestModeEn ~= "0" ) then
            if (uci_dev.disabled == nil or uci_dev.disabled == '0') and
               (uci_main_vif.disabled == nil or uci_main_vif.disabled == '0') then
                os.execute("ifconfig "..dev.main_ifname.." up")
            end
            add_vif_into_net(dev.main_ifname, uci_main_vif.network)
        else
            if uci_main_vif.mode == "ap" then
                hostapd_setup_vif(uci_dev, uci_main_vif)
                os.execute("ifconfig "..dev.main_ifname.." up")
                if (uci_dev.disabled == nil or uci_dev.disabled == '0') and
                   (uci_main_vif.disabled == nil or uci_main_vif.disabled == '0') then
                    hostapd_enable_vif(dev.main_ifname, uci_main_vif)
                end
                set_mld_bcn_down()
                add_vif_into_net(dev.main_ifname, uci_main_vif.network)
            end
        end
        mtwifi_setup_ap_vif_postinit(uci_dev, uci_main_vif)
    else
        nixio.syslog("err", "mtwifi_up: main_ifname "..dev.main_ifname.." missing, quit!")
        return
    end
end

local function mtwifi_apcli_up(uci_vif, dev, uci_dev)
    mtwifi_setup_apcli_vif_preinit(uci_dev, uci_vif)
    if not mtkdat.exist("/etc/init.d/wpad") or ( dev.TestModeEn ~= nil and dev.TestModeEn ~= "0" ) then
        nixio.syslog("debug", "mtwifi_apcli_up: ifconfig "..uci_vif.ifname.." up")
        os.execute("ifconfig "..uci_vif.ifname.." up")
        add_vif_into_net(uci_vif.ifname, uci_vif.network)
    else
        if uci_vif.mode == "sta" then
            if (uci_dev.disabled == nil or uci_dev.disabled == '0') then
                if uci_vif.disabled == nil or uci_vif.disabled ~= '1' then
                    add_vif_into_net(uci_vif.ifname, uci_vif.network)
                    supp_setup_vif(uci_dev, uci_vif)
                    local uci_mld = mtkdat.get_uci_mld_by_vif_name(uci, uci_vif.ifname)
                    if uci_mld ~= nil and ( uci_mld.disabled == nil or uci_mld.disabled == '0' ) then
                        local uci_main_vif = mtkdat.get_uci_sta_mld_main_iface(uci, uci_mld)
                        if uci_main_vif.ifname ~= uci_vif.ifname then
                            supp_enable_vif(uci_vif)
                            if (uci_vif.disabled ~= nil and uci_vif.disabled ~= '0') then
                                os.execute("ifconfig "..uci_vif.ifname.." down")
                            end
                        end
                        if mtkdat.check_if_sta_mld_all_members_up(uci, uci_mld) ~= 0 then
                            supp_enable_vif(uci_main_vif)
                            if (uci_main_vif.disabled ~= nil and uci_main_vif.disabled ~= '0') then
                                os.execute("ifconfig "..uci_main_vif.ifname.." down")
                            end
                        end
                    else
                        supp_enable_vif(uci_vif)
                    end
                end
            end
        end
    end
    mtwifi_setup_apcli_vif_postinit(uci_dev, uci_vif)
end

local function mtwifi_wds_mesh_up(uci_vif, dev, uci_dev)
    nixio.syslog("debug", "mtwifi_wds_mesh_up: ifconfig "..uci_vif.ifname.." up")
    os.execute("ifconfig "..uci_vif.ifname.." up")
    add_vif_into_net(uci_vif.ifname, uci_vif.network)
end

local function mtwifi_vap_up(uci_vif, dev, uci_dev)
    if uci_vif.mode == "ap" then
        if not mtkdat.exist("/etc/init.d/wpad") or ( dev.TestModeEn ~= nil and dev.TestModeEn ~= "0" ) then
            if uci_vif.disabled == nil or uci_vif.disabled ~= '1' then
                os.execute("ifconfig "..uci_vif.ifname.." up")
                add_vif_into_net(uci_vif.ifname, uci_vif.network)
            end
        else
            local phyname = mtkdat.get_phy_by_main_ifname(dev.main_ifname)
            local need_ceate_vif = true

            for _,vif in ipairs(string.split(mtkdat.read_pipe("ls /sys/class/net"), "\n")) do
                if vif == uci_vif.ifname then
                    need_ceate_vif = false
                end
            end
            if need_ceate_vif then
                os.execute("iw phy "..phyname.." interface add "..uci_vif.ifname.." type __ap 2> /dev/null")
            end
            hostapd_setup_vif(uci_dev, uci_vif)
            if (uci_dev.disabled == nil or uci_dev.disabled == '0') and
                (uci_vif.disabled == nil or uci_vif.disabled == '0') then
                hostapd_enable_vif(dev.main_ifname, uci_vif)
                add_vif_into_net(uci_vif.ifname, uci_vif.network)
            end

            set_mld_bcn_down()
            mtwifi_setup_ap_vif_postinit(uci_dev, uci_vif)
        end
    end
end


local function mtwifi_create_ext_vif(dev)
    local phyname = mtkdat.get_phy_by_main_ifname(dev.main_ifname)
    local need_ceate_vif = true
    local ifname_ext = dev.ext_ifname..tostring(15)

    for _,vif in ipairs(string.split(mtkdat.read_pipe("ls /sys/class/net"), "\n")) do
        if vif == ifname_ext then
            need_ceate_vif = false
        end
    end
    if need_ceate_vif then
        os.execute("iw phy "..phyname.." interface add "..ifname_ext.." type __ap 2> /dev/null")
    end

    os.execute("ifconfig "..ifname_ext.." up")
    os.execute("mwctl "..ifname_ext.." set no_bcn 1")
end

local function mtwifi_apcli_down(dev, vif)
    if string.match(vif, esc(dev.apcli_ifname).."[0-9]+") then
        del_vif_from_l3_net(vif)
        os.execute("ifconfig "..vif.." down 2> /dev/null")
        nixio.syslog("debug", "mtwifi_apcli_down, supp_disable_vif "..vif)
        supp_disable_vif(vif)
        del_vif_from_net(vif)
    end
end

local function mtwifi_vap_down(dev, vif, is_del_vif)
    if string.match(vif, esc(dev.ext_ifname).."[1-9]+") then
        nixio.syslog("debug", "mtwifi_down, hostapd_disable_vif "..vif)
        hostapd_disable_vif(vif)
        os.execute("ifconfig "..vif.." down 2> /dev/null")
        del_vif_from_net(vif)
        if is_del_vif then
            os.execute("iw dev "..vif.." del 2> /dev/null")
        end
    end
end

local function mtwifi_wds_mesh_down(dev, vif)
    if string.match(vif, esc(dev.wds_ifname).."[0-9]+") or
        string.match(vif, esc(dev.mesh_ifname).."[0-9]+") then
        nixio.syslog("debug", "mtwifi_down: ifconfig "..vif.." down")
        os.execute("ifconfig "..vif.." down 2> /dev/null")
        del_vif_from_net(vif)
    end
end

local function mtwifi_main_dev_down(dev, vif)
    if vif == dev.main_ifname then
        nixio.syslog("debug", "mtwifi_down, hostapd_disable_vif "..vif)
        hostapd_disable_vif(vif)
        os.execute("ifconfig "..vif.." down 2> /dev/null")
        del_vif_from_net(vif)
    end
end

local function mtwifi_release_ext_vif(dev)
    local ifname_ext = dev.ext_ifname..tostring(15)
    nixio.syslog("debug", "mtwifi_release_ext_vif: ifconfig "..ifname_ext.." down")
    os.execute("ifconfig "..ifname_ext.." down 2> /dev/null")
end

function mtwifi_up(devname)
    local wifi_services_exist = false
    local l1profile = "/etc/wireless/l1profile.dat"
    local uci = mtkdat.uci_load_wireless()

    if devname then
        local profiles = mtkdat.search_dev_and_profile()
        local path = profiles[devname]
        local path_base = string.match(path, "([^/]+)%.dat") or "unknown"
        if not mtkdat.exist("/tmp/mtk/wifi/"..string.match(path, "([^/]+).dat")..".last") then
            os.execute("cp -f "..path.." "..mtkdat.__profile_previous_settings_path(path))
        end
    end

    local path = "/etc/config/wireless"
    os.execute("cp -f "..path.." "..mtkdat.__uci_applied_settings_path())

    if mtkdat.cfg_is_diff() then
        mtkdat.uci2dat()
    end

    if  mtkdat.exist("/lib/wifi/wifi_services.lua") then
        wifi_services_exist = require("wifi_services")
    end

    nixio.syslog("debug", "mtwifi called up!")

    local devs, l1parser = mtkdat.__get_l1dat()
    -- l1 profile present, good!
    if l1parser and devs then
        dev = devs.devname_ridx[devname]
        if not dev then
            nixio.syslog("err", "mtwifi: dev "..devname.." not found!")
            return
        end

        local devname2 = string.gsub(devname, "%.", "_")
        local uci_dev = mtkdat.get_uci_dev_by_dev_name(uci, devname2)
        local uci_main_vif = mtkdat.get_uci_vif_by_vif_name(uci, dev.main_ifname)
        local profile = mtkdat.search_dev_and_profile()[devname]
        local cfgs = mtkdat.load_profile(profile)
        -- we have to bring up main_ifname first, main_ifname will create all other vifs.
        mtwifi_main_dev_up(uci_dev, dev, uci_main_vif)

        for _,vif in ipairs(string.split(mtkdat.read_pipe("ls /sys/class/net"), "\n"))
        do
            if vif ~= dev.main_ifname then
                local uci_vif = mtkdat.get_uci_vif_by_vif_name(uci, vif)

                if uci_vif ~= nil then
                    if string.match(vif, esc(dev.apcli_ifname).."[0-9]+") then
                        mtwifi_apcli_up(uci_vif, dev, uci_dev)
                    elseif (string.match(vif, esc(dev.wds_ifname).."[0-9]+") and
                            cfgs.WdsEnable ~= "0" and cfgs.WdsEnable ~= "") or
                           string.match(vif, esc(dev.mesh_ifname).."[0-9]+") then
                        mtwifi_wds_mesh_up(uci_vif, dev, uci_dev)
                    end
                end
            -- else nixio.syslog("debug", "mtwifi_up: skip "..vif..", prefix not match "..pre)
            end
        end

        local uci_vifs = mtkdat.get_uci_vifs_by_dev_name(uci, devname2)
        if uci_vifs ~= nil then
            local uci_vif
            for _, uci_vif in pairs(uci_vifs)
            do
                if uci_vif.ifname ~= dev.main_ifname and
                   string.match(uci_vif.ifname, esc(dev.ext_ifname).."[0-9]+") then
                    mtwifi_vap_up(uci_vif, dev, uci_dev)
                end
            end
        end

        if mtkdat.exist("/sys/class/net/"..dev.main_ifname) then
            if mtkdat.exist("/etc/init.d/wpad") and ( dev.TestModeEn == nil or dev.TestModeEn == "0" ) then
                if (uci_dev.disabled ~= nil and uci_dev.disabled ~= '0') or
                   (uci_main_vif.disabled ~= nil and uci_main_vif.disabled ~= '0') then
                    os.execute("ifconfig "..dev.main_ifname.." down")
                end
            end
        end
    else nixio.syslog("debug", "mtwifi_up: skip "..devname..", config(l1profile) not exist")
    end

    mtwifi_create_ext_vif(dev)
    os.execute(" rm -rf /tmp/mtk/wifi/mtwifi*.need_reload")
end

local function get_vifs_by_phy(phyname)
    local devs={}

    for filename in io.popen("ls /sys/class/net"):lines() do
        local parent=io.open("/sys/class/net/"..filename.."/phy80211/name")
        if parent ~= nil then
            local name=parent:read "*a"

            if string.match(name, phyname) then
                table.insert(devs, filename)
            end
            io.close(parent)
        end
    end
    return devs
end

function mtwifi_down(devname)
    os.execute("echo wifi down > /dev/console")
    if mtkdat.cfg_is_diff() then
        mtkdat.uci2dat()
    end

    local path = "/etc/config/wireless"
    os.execute("cp -f "..path.." "..mtkdat.__uci_applied_settings_path())

    local wifi_services_exist = false
    if  mtkdat.exist("/lib/wifi/wifi_services.lua") then
        wifi_services_exist = require("wifi_services")
    end

    nixio.syslog("debug", "mtwifi_down called!")

    -- M.A.N service
    if mtkdat.exist("/etc/init.d/man") then
        os.execute("/etc/init.d/man stop")
    end


    local devs, l1parser = mtkdat.__get_l1dat()
    -- l1 profile present, good!
    if l1parser and devs then
        dev = devs.devname_ridx[devname]
        if not dev then
            nixio.syslog("err", "mtwifi_down: dev "..devname.." not found!")
            return
        end
        if not mtkdat.exist("/sys/class/net/"..dev.main_ifname) then
            nixio.syslog("err", "mtwifi_down: main_ifname "..dev.main_ifname.." missing, quit!")
            return
        end
        os.execute("mwctl "..dev.main_ifname.." set hw_nat_register=0")
        for _,vif in ipairs(string.split(mtkdat.read_pipe("ls /sys/class/net"), "\n"))
        do
            mtwifi_wds_mesh_down(dev, vif)
            mtwifi_apcli_down(dev, vif)
            mtwifi_vap_down(dev, vif, true)
            mtwifi_main_dev_down(dev, vif)
        end
        mtwifi_release_ext_vif(dev)

    else nixio.syslog("debug", "mtwifi_down: skip "..devname..", config not exist")
    end

    os.execute(" rm -rf /tmp/mtk/wifi/mtwifi*.need_reload")
end

local function mtkwifi_vap_down_master(device, ifname)
    if ifname == device.main_ifname then
        mtwifi_main_dev_down(device, ifname)
    else
        mtwifi_vap_down(device, ifname, false)
    end
end

local function mtkwifi_vap_up_master(uci_vif, device, uci_dev, ifname)
    if uci_vif.disabled == "1" then
        return
    end
    if ifname == device.main_ifname then
        mtwifi_main_dev_up(uci_dev, device, uci_vif)
    else
        mtwifi_vap_up(uci_vif, device, uci_dev)
    end
end

function mtwifi_single_vap_restart(device, _devname, vifname)
    local uci = mtkdat.uci_load_wireless()
    local uci_vif = mtkdat.get_uci_vif_by_vif_name(uci, vifname)
    local devname2 = string.gsub(_devname, "%.", "_")
    local uci_dev = mtkdat.get_uci_dev_by_dev_name(uci, devname2)

    mtkwifi_vap_down_master(device, vifname)

    mtkwifi_vap_up_master(uci_vif, device, uci_dev, vifname)
end

local function table_contains(tbl, item)
    if not tbl then return false end
    for _, value in ipairs(tbl) do
        if value == item then
            return true
        end
    end
    return false
end

local function restart_vif(device, uci, vifname, uci_dev, vif_tab)
    if not table_contains(vif_tab, vifname) then
        local uci_vif = mtkdat.get_uci_vif_by_vif_name(uci, vifname)
        mtkwifi_vap_down_master(device, vifname)
        if uci_vif.macaddr then
            os.execute('ip link set ' .. vifname .. ' address ' .. uci_vif.macaddr)
        end
        mtkwifi_vap_up_master(uci_vif, device, uci_dev, vifname)
        table.insert(vif_tab, vifname)
    end
end

local function process_ap_enable(diff, cfgs, device, uci, vifext, vif_tab, uci_dev)
    local vifidx = cfgs.ApEnable:split(";")
    for i = 1, #vifidx do
        local vifname = vifext..tostring(i-1)
        local uci_vif = mtkdat.get_uci_vif_by_vif_name(uci, vifname)

        if token(cfgs.ApEnable, i) ~= token(diff.ApEnable[2], i) then
            if token(diff.ApEnable[2], i) == "0" then
                if not table_contains(vif_tab, vifname) then
                    mtkwifi_vap_down_master(device, vifname)
                    table.insert(vif_tab, vifname)
                end
            else
                if not table_contains(vif_tab, vifname) then
                    mtkwifi_vap_up_master(uci_vif, device, uci_dev, vifname)
                    table.insert(vif_tab, vifname)
                end
            end
        end
    end
end

-- ========================================
-- Storage file: /tmp/mtwifi_auto_channel.dat
-- Format: ifname=channel\n (for example, rai0=165\n)

local AUTO_CHANNEL_FILE = "/tmp/mtwifi_auto_channel.dat"

local function mtwifi_get_real_channel(ifname)
    -- Read the actual channel from iw ifname info.
    -- Output format: "channel 165 (5825 MHz), width: 20 MHz, center1: 5825 MHz"
    local cmd = "iw " .. ifname .. " info 2>/dev/null"
    local output = mtkdat.read_pipe(cmd)

    -- Parse "channel XXX" to get the channel number.
    local channel = output:match("channel%s+(%d+)")
    return channel and tonumber(channel) or nil
end

local function mtwifi_store_auto_channel(ifname)
    -- Read and store the current actual channel.
    local real_ch = mtwifi_get_real_channel(ifname)
    if real_ch then
        -- Read existing data.
        local data = {}
        local fp = io.open(AUTO_CHANNEL_FILE, "r")
        if fp then
            for line in fp:lines() do
                local k, v = line:match("(%S+)=(%d+)")
                if k and v and k ~= ifname then
                    data[#data + 1] = k .. "=" .. v
                end
            end
            fp:close()
        end

        -- Add the new data.
        data[#data + 1] = ifname .. "=" .. real_ch

        -- Write the data back.
        fp = io.open(AUTO_CHANNEL_FILE, "w")
        if fp then
            fp:write(table.concat(data, "\n") .. "\n")
            fp:close()
        end
    end
end

local function mtwifi_load_auto_channel(ifname)
    -- Read the stored channel from the file.
    local fp = io.open(AUTO_CHANNEL_FILE, "r")
    if fp then
        for line in fp:lines() do
            local k, v = line:match("(%S+)=(%d+)")
            if k == ifname then
                fp:close()
                return tonumber(v)
            end
        end
        fp:close()
    end
    return nil
end

local function mtwifi_restore_channel(uci_dev, ifname, cfgs)
    local phyname = iface_type(ifname)
    local bw = __bw(cfgs["HT_BW"], cfgs["VHT_BW"], cfgs["EHT_ApBw"])
    local channel = uci_dev.channel

    -- In auto mode, read the previously stored actual channel.
    if channel == "auto" then
        local stored_ch = mtwifi_load_auto_channel(ifname)
        if stored_ch then
            channel = stored_ch
        end
    end

    os.execute("mwctl phy " .. phyname .. " set channel bw=".. bw)
    os.execute("mwctl phy " .. phyname .. " set channel num=".. channel)
end

local function process_apcli_enable(diff, cfgs, device, uci, vifapcli, uci_dev)
    for i = 1, 1 do
        local vifname = vifapcli..tostring(i-1)
        local uci_vif = mtkdat.get_uci_vif_by_vif_name(uci, vifname)

        if token(cfgs.ApCliEnable, i) ~= token(diff.ApCliEnable[2], i) then
            if token(diff.ApCliEnable[2], i) == "0" then
                mtwifi_restore_channel(uci_dev, device.main_ifname, cfgs)
                mtwifi_apcli_down(device, vifname)
            else
                if uci_dev.channel == "auto" then
                    mtwifi_store_auto_channel(device.main_ifname)
                end
                if uci_vif.macaddr then
                    os.execute('ip link set ' .. vifname .. ' address ' .. uci_vif.macaddr)
                end
                mtwifi_apcli_up(uci_vif, device, uci_dev)
            end
        end
    end
end

local function process_apcli_restart(diff, cfgs, device, uci, vifapcli, uci_dev)
    for i = 1, 1 do
        local vifname = vifapcli..tostring(i-1)
        local uci_vif = mtkdat.get_uci_vif_by_vif_name(uci, vifname)
        if diff.ApCliEnable == nil and (uci_vif.disabled == "0" or uci_vif.disabled == nil) then
            mtwifi_apcli_down(device, vifname)
            if uci_vif.macaddr then
                os.execute('ip link set ' .. vifname .. ' address ' .. uci_vif.macaddr)
            end
            mtwifi_apcli_up(uci_vif, device, uci_dev)
        end
    end
end

local function process_mac_address(k, diff, device, uci, vifext, vif_tab, uci_dev)
    local idx = k == "MacAddress" and 0 or tonumber(string.match(k, "^MacAddress(%d+)$"))
    local vifname = vifext..tostring(idx)
    restart_vif(device, uci, vifname, uci_dev, vif_tab)
end

local function process_ssid(k, diff, device, uci, vifext, vif_tab, uci_dev)
    local idx = k == "SSID" and 0 or tonumber(string.match(k, "^SSID(%d+)$")) - 1
    local vifname = vifext..tostring(idx)
    restart_vif(device, uci, vifname, uci_dev, vif_tab)
end

local function process_key(k, diff, device, uci, vifext, vif_tab, uci_dev)
    local idx = k == "WPAPSK" and 0 or tonumber(string.match(k, "^WPAPSK(%d+)$")) - 1
    local vifname = vifext..tostring(idx)
    restart_vif(device, uci, vifname, uci_dev, vif_tab)
end

local function process_auth_mode(diff, cfgs, device, uci, vifext, vif_tab, uci_dev)
    local vifidx = cfgs.AuthMode:split(";")
    for i = 1, #vifidx do
        local vifname = vifext..tostring(i-1)

        if not table_contains(vif_tab, vifname) then
            local uci_vif = mtkdat.get_uci_vif_by_vif_name(uci, vifname)

            if token(cfgs.AuthMode, i) ~= token(diff.AuthMode[2], i) then
                mtkwifi_vap_down_master(device, vifname)
                mtkwifi_vap_up_master(uci_vif, device, uci_dev, vifname)
                table.insert(vif_tab, vifname)
            end
        end
    end
end

local function mtkwifi_vap_wifi_updown(uci, cfgs, diff, device, uci_dev)
    local vifext = device.ext_ifname
    local vifapcli = device.apcli_ifname
    local vif_tab = {}

    for k, v in pairs(diff) do
        if k == "ApEnable" then
            process_ap_enable(diff, cfgs, device, uci, vifext, vif_tab, uci_dev)
        elseif k == "ApCliEnable" then
            process_apcli_enable(diff, cfgs, device, uci, vifapcli, uci_dev)
        elseif k == "MacAddress" or string.match(k, "MacAddress[0-9]+") then
            process_mac_address(k, diff, device, uci, vifext, vif_tab, uci_dev)
        elseif k:find("^SSID") then
            process_ssid(k, diff, device, uci, vifext, vif_tab, uci_dev)
        elseif k:find("^WPAPSK") then
            process_key(k, diff, device, uci, vifext, vif_tab, uci_dev)
        elseif k == "AuthMode" then
            process_auth_mode(diff, cfgs, device, uci, vifext, vif_tab, uci_dev)
        end
    end

    if __set_wifi_apcli_security(cfgs, diff, device) then
        process_apcli_restart(diff, cfgs, device, uci, vifapcli, uci_dev)
    end
end

function mtwifi_reload(devname)
    local normal_reload = false
    local qsetting = false
    local cfgs, diff
    local path, profiles
    local devs, l1parser = mtkdat.__get_l1dat()
    local uci = mtkdat.uci_load_wireless()

    if mtkdat.exist("/lib/wifi/quick_setting.lua") then
        qsetting = true
        profiles = mtkdat.search_dev_and_profile()
    end

    for devname, dev in mtkdat.spairs(devs.devname_ridx) do
        if qsetting then
            -- Create devname.last for quick setting
            path = profiles[devname]
            -- Cycle the interface on first startup even when no diff exists.
            if not mtkdat.exist(mtkdat.__profile_previous_settings_path(path)) then
                normal_reload = true
            end
            os.execute("cp -f "..path.." "..mtkdat.__profile_previous_settings_path(path))
        end
    end

    local path = "/etc/config/wireless"
    os.execute("cp -f "..path.." "..mtkdat.__uci_applied_settings_path())

    if mtkdat.cfg_is_diff() then
        mtkdat.uci2dat(uci)
    end

    -- For one card , all interface should be down, then up
    if not devname then
        local cfgs_table = {}
        local diff_table = {}
        if qsetting then
            for _devname, _dev in mtkdat.spairs(devs.devname_ridx) do
                _path = profiles[_devname]
                _dev.normal_reload, _cfgs, _diff = check_quick_settings(_devname, _path)
                cfgs_table[_devname] = _cfgs
                diff_table[_devname] = _diff
            end
        end

        for _devname, _dev in mtkdat.spairs(devs.devname_ridx) do
            if _dev.normal_reload or normal_reload then
                    mtwifi_down(_devname)
                    mtwifi_up(_devname)
            else
                if cfgs_table[_devname] and diff_table[_devname] then
                    local devname2 = string.gsub(_devname, "%.", "_")
                    local uci_dev = mtkdat.get_uci_dev_by_dev_name(uci, devname2)
                    for _,vif in ipairs(string.split(mtkdat.read_pipe("ls /sys/class/net"), "\n"))
                    do
                        if string.match(vif, esc(_dev.ext_ifname).."[0-9]+") then
                            local uci_vif = mtkdat.get_uci_vif_by_vif_name(uci, vif)
                            if uci_vif ~= nil and (uci_vif.disabled == nil or uci_vif.disabled == "0") then
                                hostapd_setup_vif(uci_dev, uci_vif)
                            end
                        elseif string.match(vif, esc(_dev.apcli_ifname).."[0-9]+") then
                            local uci_vif = mtkdat.get_uci_vif_by_vif_name(uci, vif)
                            if uci_vif ~= nil and (uci_vif.disabled == nil or uci_vif.disabled == "0") then
                                supp_setup_vif(uci_dev, uci_vif)
                            end
                        end
                    end
                    -- Cycle the VAP when its disabled state changes.
                    mtkwifi_vap_wifi_updown(uci, cfgs_table[_devname], diff_table[_devname], _dev, uci_dev)
                    do_quick_settings(_devname, cfgs_table[_devname], diff_table[_devname])
                end
            end
        end
    else
        if qsetting then
            path = profiles[devname]
            normal_reload, cfgs, diff = check_quick_settings(devname, path)
        end

        if normal_reload then
            local dev = devs.devname_ridx[devname]
            --assert(exist(dev.init_script))
            local compatname = dev.init_compatible
            -- Different cards do not affect each other
            if not string.find(dev.profile_path, "dbdc") then
                if dev.init_compatible == compatname then
                    mtwifi_down(devname)
                    mtwifi_up(devname)
                end
            --If the reloaded device belongs to dbdc, then another device on dbdc also need to be reloaded
            else
                for devname, dev in pairs(devs.devname_ridx) do
                    if dev.init_compatible == compatname then mtwifi_down(devname) end
                end
                for devname, dev in mtkdat.__spairs(devs.devname_ridx) do
                    if dev.init_compatible == compatname then mtwifi_up(devname) end
                end
            end
        else
            local devname2 = string.gsub(devname, "%.", "_")
            local uci_dev = mtkdat.get_uci_dev_by_dev_name(uci, devname2)
            local uci_vifs = mtkdat.get_uci_vifs_by_dev_name(uci, devname2)
            if uci_vifs ~= nil then
                for _, uci_vif in pairs(uci_vifs) do
                    if uci_vif.mode == "ap" then
                        -- Update each vap.conf file.
                        hostapd_setup_vif(uci_dev, uci_vif)
                    end
                end
            end
            if cfgs and diff then
                do_quick_settings(devname, cfgs, diff)
            end
        end
    end
end

function mtwifi_restart(devname)
    --local uci  = require "luci.model.uci".cursor()
    local devs, l1parser = mtkdat.__get_l1dat()

    if mtkdat.cfg_is_diff() then
        mtkdat.uci2dat()
    end

    local profiles = mtkdat.search_dev_and_profile()
    for devname, dev in mtkdat.spairs(devs.devname_ridx) do
        path = profiles[devname]
        os.execute("cp -f "..path..
            " "..mtkdat.__profile_previous_settings_path(path))
    end

    local path = "/etc/config/wireless"
    os.execute("cp -f "..path.." "..mtkdat.__uci_applied_settings_path())

    -- for AX8400 add 5G interface
    local isRoot = false
    if devname then
        local dev, path, diff
        local is7915 = false
        dev = devs.devname_ridx[devname]
        path = dev.profile_path
        if path then
            is7915 = string.find(path, "mt7915")
            diff =  mtkdat.diff_profile(path)
            if is7915 and diff.BssidNum then
                isRoot = true
            end
        end
    end

    nixio.syslog("debug", "mtwifi_restart called!")

    -- if wifi driver is built-in, it's necessary action to reboot the device
    if mtkdat.exist("/sys/module/mt_wifi") == false or isRoot then
        os.execute("echo reboot_required > /tmp/mtk/wifi/reboot_required")
        return
    end

    if devname then
        mtwifi_down(devname)
    else
--        for _devname, _dev in mtkdat.spairs(devs.devname_ridx, function(a,b) return string.upper(a) > string.upper(b) end) do
        for _devname, _dev in mtkdat.spairs(devs.devname_ridx) do
            mtwifi_down(_devname)
        end
    end
    os.execute("rmmod mt_whnat")
    os.execute("/etc/init.d/fwdd stop")
    os.execute("rmmod mtfwd")
    --os.execute("rmmod mtk_warp_proxy")
    -- warp.ko actually no need to unload , due to sqc wifi restart may encounter the WO load fail
    -- keep the workaround until the warp / WO module init flow PASS in SQC env.
    -- os.execute("rmmod mtk_warp")
    -- mt7915_mt_wifi is for dual ko only
    --os.execute("rmmod mt7915_mt_wifi")
    os.execute("rmmod mt_wifi")

    os.execute("modprobe mt_wifi")
    --os.execute("modprobe mt7915_mt_wifi")
    -- os.execute("modprobe mtk_warp")
    --os.execute("modprobe mtk_warp_proxy")
    os.execute("modprobe mtfwd")
    os.execute("/etc/init.d/fwdd start")
    --os.execute("modprobe mt_whnat")
    if devname then
        mtwifi_up(devname)
    else
        for _devname, _dev in mtkdat.spairs(devs.devname_ridx) do
            mtwifi_up(_devname)
        end
    end
end

function mtwifi_reset(devname)
    nixio.syslog("debug", "mtwifi_reset called!")
    if mtkdat.exist("/rom/etc/wireless/mediatek/") then
        os.execute("rm -rf /etc/wireless/mediatek/")
        os.execute("cp -rf /rom/etc/wireless/mediatek/ /etc/wireless/")
        if mtkdat.exist("/etc/config/wireless") then
            os.execute("rm -rf /etc/config/wireless")
            mtwifi_detect(devname)
        end
        if mtkdat.exist("/tmp/mtk/wifi/") then
            os.execute("rm -rf /tmp/mtk/wifi/")
        end
        mtwifi_reload(devname)
    else
        nixio.syslog("debug", "mtwifi_reset: /rom"..profile.." missing, unable to reset!")
    end
end

function mtwifi_status(devname)
    return wifi_common_status()
end

function mtwifi_hello(devname)
   os.execute("echo mtwifi_hello: "..devname)
end

function mtwifi_detect(devname)
    nixio.syslog("debug", "mtwifi_detect")
    mtkdat.dat2uci()
end

function mtwifi_save(devname)
    mtkdat.uci2dat()
end

function mtwifi_check()
    local path = "/etc/wireless/mediatek/mt7993.1.dat"
    local dat_b0_path = "/etc/wireless/mediatek/mt7993.b0.dat"
    local dat_b1_path = "/etc/wireless/mediatek/mt7993.b1.dat"

    nixio.syslog("debug", "mtwifi_check: "..path)
    mtkdat.check_band_profile(path)
    mtkdat.check_dat_file(dat_b0_path)
    mtkdat.check_dat_file(dat_b1_path)
end
