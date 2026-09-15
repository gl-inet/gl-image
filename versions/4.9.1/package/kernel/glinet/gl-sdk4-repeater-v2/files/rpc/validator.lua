local function valid_macaddr(v)
    return true
end

return {
    connect = {
        ssid = '.+',
        key = '.+',
        identity = '.+',
        macaddr = valid_macaddr
    },
    remove_saved_ap = {
        ssid = '.+'
    }
}
