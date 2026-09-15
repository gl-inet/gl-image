-- Author: Jianhui Zhao <jianhui.zhao@gl-inet.com>

local httpc = require 'eco.http.client'
local file = require 'eco.file'
local time = require 'eco.time'
local ubus = require 'eco.ubus'
local sys = require 'eco.sys'
local log = require 'eco.log'
local uci = require 'uci'

local htmlparser = require 'htmlparser'

local M = {}

local original_cfg = {}

local status = {
    bare_mode = false,
    detected = false,
    detecting = false
}
-- Headless engine: ubus screen_portal (gl-sdk4-screen-portal).
-- Optional wake: gl_screen turnon (replaces gl_wake_up on legacy gl_screen portal_* JSON).
local function notify_screen_portal()
    local ok, err = pcall(function()
        if status.detected and status.portal_url then
            ubus.call('screen_portal', 'portal_required', {
                portal_url = status.portal_url,
                portal_dns = status.dns or '',
                portal_ifname = status.ifname or ''
            })
        else
            ubus.call('gl_screen', 'set', {method="portal_closed"})
            ubus.call('screen_portal', 'portal_closed', {})
        end
    end)
    if not ok then
        log.debug('notify screen_portal ubus failed:', err)
    end
end
local function call_rpc(mod, func, params)
    local res = ubus.call('gl-session', 'call', { module = mod, func = func, params = params })
    if res then
        return res.result
    end
end

local function disable_adg()
    local res = call_rpc('adguardhome', 'get_config')
    if not res then
        return
    end

    if res.enabled then
        original_cfg.adg = res
        log.info('disable adg')
        call_rpc('adguardhome', 'set_config', { enabled = false })
    end
end

local function enable_adg()
    local adg = original_cfg.adg

    if adg and adg.enabled then
        log.info('enable adg')
        call_rpc('adguardhome', 'set_config', { enabled = true })
    end
end


local function disable_dns()
    local res = call_rpc('dns', 'get_config')
    if not res then
        return
    end

    if res.mode ~= 'auto' or res.rebind_protection or not res.force_dns or not res.override_vpn then
        original_cfg.dns = res
        log.info('disable dns security')
        call_rpc('dns', 'set_config', { mode = 'auto', force_dns = true, override_vpn = true })
    end
end

local function enable_dns()
    local dns = original_cfg.dns

    if not dns then
        return
    end

    if dns.mode ~= 'auto' or dns.rebind_protection or not dns.force_dns or not dns.override_vpn then
        log.info('enable dns security')
        call_rpc('dns', 'set_config', dns)
    end
end


local function disable_zerotier()
    local res = call_rpc('zerotier', 'get_config')
    if not res then
        return
    end

    if res.enabled then
        original_cfg.zerotier = res
        log.info('disable zerotier')
        call_rpc('zerotier', 'set_config', { enabled = false })
    end
end

local function enable_zerotier()
    local zerotier = original_cfg.zerotier

    if zerotier and zerotier.enabled then
        log.info('enable zerotier')
        call_rpc('zerotier', 'set_config', zerotier)
    end
end


local function disable_parental_control()
    local res = call_rpc('parental-control', 'get_config')

    if not res then
        return
    end

    if res.enable then
        local cfg = {
            enable = false,
            drop_anonymous = res.drop_anonymous,
            auto_update = res.auto_update
        }
        original_cfg.parental_control = res
        log.info('disable parental control')
        call_rpc('parental-control', 'set_config', cfg)
    end
end

local function enable_parental_control()
    local parental_control = original_cfg.parental_control

    if parental_control and parental_control.enable then
        log.info('enable parental control')
        call_rpc('parental-control', 'set_config', parental_control)
    end
end

local function disable_tailscale()
    local res = call_rpc('tailscale', 'get_config')
    if not res then
        return
    end

    if res.enabled then
        original_cfg.tailscale = res
        log.info('disable tailscale')
        call_rpc('tailscale', 'set_config', { enabled = false })
    end
end

local function enable_tailscale()
    local tailscale = original_cfg.tailscale

    if tailscale and tailscale.enabled then
        log.info('enable tailscale')
        call_rpc('tailscale', 'set_config', tailscale)
    end
end

local function skip_aw_policy_for_auth()
    local client_macaddr = status.client_macaddr
    local cmd

    if client_macaddr then
        cmd = string.format('mptun_tools mac_skip_policy %s >/dev/null 2>&1', client_macaddr)
        log.info('set', client_macaddr, 'bypass aw')
    else
        cmd = 'mptun_tools skip_policy >/dev/null 2>&1'
        log.info('set all bypass aw')
    end

    sys.exec('sh', '-c', cmd):wait()
end

local function restore_skiped_aw_policy()
    local client_macaddr = status.client_macaddr
    local cmd

    if client_macaddr then
        cmd = string.format('mptun_tools mac_restore_policy %s >/dev/null 2>&1', client_macaddr)
        log.info('set', client_macaddr, 'via aw')
    else
        cmd = 'mptun_tools restore_policy >/dev/null 2>&1'
        log.info('set all via aw')
    end

    sys.exec('sh', '-c', cmd):wait()
end

local function disable_vpn()
    local client_macaddr = status.client_macaddr
    local cmd

    if client_macaddr then
        cmd = string.format('iptables -w -t mangle -I PREROUTING -i br-+ -m mac --mac-source %s -j MARK --set-mark 0x8000/0xf000 -m comment --comment repeater-portal', client_macaddr)
        log.info('set', client_macaddr, 'bypass vpn')
    else
        cmd = 'iptables -w -t mangle -I PREROUTING -i br-+ -j MARK --set-mark 0x8000/0xf000 -m comment --comment repeater-portal'
        log.info('set all bypass vpn')
    end

    sys.exec('sh', '-c', cmd):wait()
end

local function enable_vpn()
    local client_macaddr = status.client_macaddr
    local cmd

    if client_macaddr then
        cmd = string.format('iptables -w -t mangle -D PREROUTING -i br-+ -m mac --mac-source %s -j MARK --set-mark 0x8000/0xf000 -m comment --comment repeater-portal', client_macaddr)
        log.info('set', client_macaddr, 'via vpn')
    else
        cmd = 'iptables -w -t mangle -D PREROUTING -i br-+ -j MARK --set-mark 0x8000/0xf000 -m comment --comment repeater-portal'
        log.info('set all via vpn')
    end

    sys.exec('sh', '-c', cmd):wait()
end


local function disable_tor()
    local res = call_rpc('tor', 'get_config')
    if not res then
        return
    end

    if res.enable then
        original_cfg.tor = res
        log.info('disable tor')
        call_rpc('tor', 'set_config', { enable = false, manual = res.manual })
    end
end

local function enable_tor()
    local tor = original_cfg.tor

    if tor and tor.enable then
        log.info('enable tor')
        call_rpc('tor', 'set_config', tor)
    end
end

local function kmwan_probe_enabled()
    if not file.access('/proc/gl-kmwan/config') then
        return false
    end

    local c = uci.cursor()
    local disabled = c:get('kmwan', 'wwan', 'disabled') or '1'
    c:close()

    if disabled == '1' then
        return false
    end

    local s = ubus.call('network.interface.wwan', 'status') or {}
    if not s.up then
        return false
    end

    local t = 5

    while t > 0 do
        if file.readfile('/proc/gl-kmwan/config'):match('wwan') then
            return true
        end
        t = t - 1
        time.sleep(1)
    end

    return false
end

local function kmwan_probe_ctl(on)
    if not kmwan_probe_enabled() then
        log.debug('wwan does not tracked by kmwan')
        return
    end

    log.debug(on and 'enable' or 'disable', 'kmwan track wwan')

    local data = string.format('{"op":%d,"data":{"cells":["wwan"]}}\n', on and 4 or 5)
    file.writefile('/proc/gl-kmwan/config', data)

    time.sleep(1)
end

local function kmwan_force_dead_ctl(on)
    if not file.access('/proc/gl-kmwan/config') then
        return
    end

    log.debug(on and 'force dead' or 'restore detect', 'kmwan wwan')

    local data = string.format('{"op":%d,"data":{"cells":["wwan"]}}\n', on and 7 or 8)
    file.writefile('/proc/gl-kmwan/config', data)
end

function M.enter_bare_mode(client_macaddr)
    if not status.detected then
        return
    end

    if status.bare_mode then
        log.err('bare mode already enabled')
        return
    end

    log.info('enter bare mode...')

    status.bare_mode = true

    status.client_macaddr = client_macaddr

    kmwan_probe_ctl(false)

    disable_adg()
    disable_dns()
    disable_zerotier()
    disable_tailscale()
    disable_vpn()
    skip_aw_policy_for_auth()
    disable_tor()
    disable_parental_control()
end

function M.exit_bare_mode()
    if not status.bare_mode then
        return
    end

    status.bare_mode = false

    log.info('exit bare mode...')

    kmwan_probe_ctl(true)

    enable_adg()
    enable_dns()
    enable_zerotier()
    enable_tailscale()
    enable_vpn()
    restore_skiped_aw_policy()
    enable_tor()
    enable_parental_control()

    original_cfg = {}
end

function M.get_status()
    return {
        detected = status.detected,
        detecting = status.detecting,
        portal_url = status.portal_url,
        bare_mode = status.bare_mode
    }
end

function M.on_kmwan_wwan_event(interface)
    if interface ~= 'wwan' then
        return
    end

    if status.detected then
        kmwan_force_dead_ctl(true)
    end
end

--[[
<!DOCTYPE html>
<html>
  <head>
    <meta http-equiv="refresh" content="0; url=https://yanlinlin.cn/">
  </head>
</html>
--]]
local function parse_refresh(root)
    local metas = root:select('head meta')
    if #metas == 0 then
        metas = root:select('HEAD META')
    end

    local function match_http_equiv(meta)
        for k, v in pairs(meta.attributes) do
            if k:lower() == 'http-equiv' then
                return v:lower() == 'refresh'
            end
        end
    end

    local function match_http_content(meta)
        for k, v in pairs(meta.attributes) do
            if k:lower() == 'content' then
                return v
            end
        end
    end

    for _, meta in ipairs(metas) do
        if match_http_equiv(meta) then
        local content = match_http_content(meta)
            if content then
                return content
            end
        end
    end
end

local function extract_url_from_code(code)
    local quote = code:sub(1, 1)

    if quote ~= '\'' and quote ~= '"' then
        return nil
    end

    local pos = code:find(quote, 2)
    local url = code:sub(2, pos - 1)
    if not url then
        return nil
    end

    -- skip: 'http://xx.com?lan${navigator.language}';
    if url:find('${') then
        return nil
    end

    -- skip: 'http://xx.com?lan' + navigator.language;
    if code:sub(pos + 1):find('[%s\t\n]%+') == 1 then
        return nil
    end

    if not url:match("^https?://") then
        return nil
    end

    return url
end

local function parse_js_location(root)
    local scripts = root:select('script')
    if #scripts == 0 then
        scripts = root:select('SCRIPT')
    end

    if #scripts == 0 then
        return nil
    end

    local patterns = {
        -- location.href = 'url'
        {keyword = 'window.location.href', type = 'assign'},
        {keyword = 'window.location', type = 'assign'},
        {keyword = 'top.location.href', type = 'assign'},
        {keyword = 'top.location', type = 'assign'},
        {keyword = 'location.href', type = 'assign'},
        {keyword = 'location', type = 'assign'},

        -- location.replace('url')
        {keyword = 'window.location.replace', type = 'method'},
        {keyword = 'window.location.assign', type = 'method'},
        {keyword = 'top.location.replace', type = 'method'},
        {keyword = 'top.location.assign', type = 'method'},
        {keyword = 'location.replace', type = 'method'},
        {keyword = 'location.assign', type = 'method'},
    }

    for _, pattern in ipairs(patterns) do
        local keyword = pattern.keyword .. '[%s\t\n]*'
        if pattern.type == 'assign' then
            keyword = keyword .. '='
        else
            keyword = keyword .. '%('
        end

        pattern.keyword = keyword .. '[%s\t\n]*'
    end

    for _, script in ipairs(scripts) do
        local content = script:getcontent()
        local content_lower = content:lower()

        for _, pattern in ipairs(patterns) do
            local _, match_end = content_lower:find(pattern.keyword)
            if match_end then
                return extract_url_from_code(content_lower:sub(match_end + 1))
            end
        end
    end
end

local function random_str(n)
    local t = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"
    }

    local f = io.open('/dev/urandom')

    assert(f)

    local s = {}
    for _ = 1, n do
        local i = f:read(1)
        s[#s + 1] = t[i:byte() % #t + 1]
    end

    f:close()

    return table.concat(s)
end

local function match_content(body, content)
    if not content then
        return false
    end

    local t = type(content)
    if t == 'string' then
        return body:find(content)
    elseif t == 'table' then
        for _, v in ipairs(content) do
            if type(v) == 'string' and body:find(v) then
                return true
            end
        end
    end

    return false
end

-- Return 1 for detected, 0 for not detected, -1 for error
local function detect_portal_url(ifname, dns, last_detecting, u)
    local resp, err = httpc.get(u.url, {
        nameservers = dns,
        device = ifname,
        mark = 0x8000
    })

    if last_detecting.done then
        return 0
    end

    if not resp then
        log.err('detect portal fail:', err)
        return -1
    end

    local code = resp.code

    if code ~= 301 and code ~= 302 and code ~= 303 and code ~= 307 and code ~= 200 then
        log.err('detect portal with unknown http code:', code)
        return -1
    end

    local headers = resp.headers
    local location

    if code == 200 then
        local body = resp.body
        if not body or body == '' then
            log.err('detect portal fail: not found body')
            return -1
        end

        if match_content(body, u.content) then
            return 0
        end

        file.writefile('/var/log/gl-repeater/portal.html', body)

        local root = htmlparser.parse(body)

        local refresh = parse_refresh(root)
        if refresh then
            local k, v = refresh:match('(%w+)[%s\t]*=[%s\t]*(%S+)')
            if k and v then
                if k:lower() == 'url' then
                    location = v
                    local s = location:sub(1, 1)
                    if s == '\'' or s == '"' then
                        location = location:sub(2, #location - 1)
                    end
                end
            end
        end

        if not location then
            location = parse_js_location(root)
        end
    else
        location = headers['location']
    end

    if not location then
        local prefix = random_str(10)
        local surffix = random_str(10)
        location = 'http://' .. prefix .. '.gl-wifi.com/' .. surffix
        log.err('detect portal: not found location, use default')
    end

    status.portal_url = location

    return 1
end

-- Return 1 for detected, 0 for not detected, -1 for error
local function detect_portal(ifname, dns, last_detecting)
    local urls = {
        {
            url = 'http://captive.apple.com/hotspot-detect.html',
            content = {'<BODY>Success</BODY>', '<TITLE>Success</TITLE>'}
        },
        {
            url = 'http://www.msftconnecttest.com/connecttest.txt',
            content = 'Microsoft Connect Test'
        }
    }

    local res

    for _, u in ipairs(urls) do
        res = detect_portal_url(ifname, dns, last_detecting, u)

        if last_detecting.done then
            return 0
        end

        if res == 1 then
            return res
        end
    end

    if res == 0 then
        status.portal_url = nil
    end

    return res
end

local function detect_portal_loop(R, ctx)
    time.sleep(3)

    while not ctx.done do
        if ctx.done then
            break
        end

        local r = detect_portal(ctx.ifname, ctx.dns, ctx)

        if ctx.done then
            break
        end

        if r == 0 then
            status.detected = false

            kmwan_force_dead_ctl(false)

            if status.bare_mode then
                M.exit_bare_mode()
            end

            R:notify_status()
            notify_screen_portal()
            log.info('portal pass')

            break
        end

        time.sleep(3)
    end
end

function M.stop_detect()
    if status.last_detecting then
        status.last_detecting.done = true
        status.last_detecting = nil
    end

    kmwan_force_dead_ctl(false)

    status.detected = false
    status.detecting = false
    status.portal_url = nil
    notify_screen_portal()
end

local function detect_portal_multi(ifname, dns, last_detecting, n)
    local res

    while n > 0 do
        res = detect_portal(ifname, dns, last_detecting)
        if res ~= -1 then
            return res
        end
        n = n -1
        time.sleep(1)
    end
    return res
end

function M.detect(R, ifname, dns, auto_portal)
    M.stop_detect()

    -- stop kmwan probe wwan
    kmwan_probe_ctl(false)

    -- delayed 1s in kmwan_probe_ctl, maybe the connect state changed at the time
    if not R:is_connected() then
        kmwan_probe_ctl(true)
        return
    end

    log.info('portal detecting...')

    status.detecting = true
    R:notify_status()

    status.dns = dns
    status.ifname = ifname
    local last_detecting = {
        auto_portal = auto_portal,
        ifname = ifname,
        dns = dns
    }

    status.last_detecting = last_detecting

    local detected = detect_portal_multi(ifname, dns, last_detecting, 3) > 0

    -- enable kmwan probe wwan
    kmwan_probe_ctl(true)

    if last_detecting.done then
        log.info('detect portal aborted')
        status.detecting = false
        R:notify_status()
        return
    end

    status.detected = detected
    status.detecting = false
    R:notify_status()

    if not status.detected then
        log.info('not found portal')
        notify_screen_portal()
        return
    end

    kmwan_force_dead_ctl(true)

    eco.run(detect_portal_loop, R, last_detecting)

    log.info('portal detected')

    if auto_portal then
        M.enter_bare_mode()
    end

    R:notify_status()
    notify_screen_portal()
end

return M
