local ubus = require 'oui.ubus'

local M = {}

function M.status()
    local initializing = -1
    return ubus.call('repeater', 'status') or { state = initializing }
end

return M
