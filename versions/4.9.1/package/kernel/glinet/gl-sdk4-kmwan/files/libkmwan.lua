local M = {}

local fs = require "oui.fs"

M.get_ifstatus = function(iface)
    local pattern = "([_%w-]+):(%w+)"

    if fs.access("/proc/gl-kmwan/config") then
        for l in io.lines("/proc/gl-kmwan/config") do
            local name, status = string.match(l, pattern)
            if name == iface then
                return status
            end
        end
        return "offline"
    end
end

return M