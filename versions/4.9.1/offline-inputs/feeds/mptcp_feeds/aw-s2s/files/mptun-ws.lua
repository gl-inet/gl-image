local M = {}

function M.IPRB_status()
    local mod = dofile('/usr/lib/oui-httpd/rpc/mptun')
    return mod.get_IPRB_status()
end

return M
