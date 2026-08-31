package main

import (
    "flag"
    "fmt"
    "github.com/mdlayher/arp"
    "log"
    "net"
    "net/netip"
    "sync"
    "time"
)

var (
    // ifaceFlag is used to set a network interface for ARP requests
    ifaceFlag = flag.String("i", "eth0", "network interface to use for ARP request")
)

// ARPScan scans the local network for devices.
func ARPScan(interfaceName string, ipAddrs []string) {
    // Ensure valid network interface
    ifi, err := net.InterfaceByName(interfaceName)
    if err != nil {
        log.Fatal(err)
    }
    var wg sync.WaitGroup
    // Determine the size of each chunk based on the length of ipAddrs
    length := len(ipAddrs)
    var chunkSize int
    var scanTime int64 = 5
    var retryCount int = 0
    switch {
    case length < 600:
        chunkSize = 50
        scanTime = 100
        retryCount = 1
    case length < 1500:
        chunkSize = 50
        scanTime = 100
    case length < 5000:
        chunkSize = 1000
    case length < 10000:
        chunkSize = 2000
    default:
        chunkSize = 10000
    }
    numChunks := (len(ipAddrs) + chunkSize - 1) / chunkSize // Calculate number of chunks
    // Function to process a chunk of IP addresses
    processChunk := func(chunk []string) {
        defer wg.Done() // Decrement the counter when the goroutine completes
        // Set up ARP client with socket
        c, err := arp.Dial(ifi)
        if err != nil {
            log.Println("Error dialing ARP:", err)
            return
        }
        defer c.Close()

        scanWithRetries(c, chunk, scanTime, retryCount)
    }

    // Create goroutines for each chunk
    for i := 0; i < numChunks; i++ {
        start := i * chunkSize
        end := start + chunkSize
        if end > len(ipAddrs) {
            end = len(ipAddrs)
        }

        wg.Add(1)
        go processChunk(ipAddrs[start:end])
    }

    wg.Wait() // Wait for all goroutines to complete
}

func scanWithRetries(c *arp.Client, chunk []string, scanTime int64, retryCount int) {
    for attempt := 0; attempt <= retryCount; attempt++ {
        ipFailedList := make([]string, 0)
        for _, ipStr := range chunk {
            // Set request deadline
            if err := c.SetDeadline(time.Now().Add(time.Duration(scanTime) * time.Millisecond)); err != nil {
                return
            }
            // Perform ARP request
            ip, err := netip.ParseAddr(ipStr)
            if err != nil {
                continue
            }
            mac, err := c.Resolve(ip)
            if err != nil {
                ipFailedList = append(ipFailedList, ipStr)
                continue
            }
            fmt.Printf("%-15s\t%-17s\n", ip, mac)
        }
        // If there are no failed IPs, break early
        if len(ipFailedList) == 0 {
            break
        }

        // Wait before next retry
        time.Sleep(time.Duration(scanTime) * time.Millisecond)

        // Replace chunk with failed IPs for the next retry
        chunk = ipFailedList
    }
}

// GetInterfaceInfo retrieves the first IPv4 address of the specified network interface.
// It returns the IP address and CIDR notation.
func GetInterfaceInfo(interfaceName string) (string, string, error) {
    // Get network interface by name
    ifi, err := net.InterfaceByName(interfaceName)
    if err != nil {
        return "", "", fmt.Errorf("failed to get interface: %v", err)
    }
    // Get a list of addresses assigned to the interface
    addrs, err := ifi.Addrs()
    if err != nil {
        return "", "", fmt.Errorf("failed to get addresses: %v", err)
    }
    // Iterate over the addresses to find the first IPv4 address
    for _, addr := range addrs {
        switch v := addr.(type) {
        case *net.IPNet:
            ip := v.IP
            if ip.To4() != nil { // Check if the IP is IPv4
                netmask := net.IP(v.Mask).String()
                if ip.String() != "" && netmask != "" {
                    return ip.String(), netmask, nil
                }
            }
        }
    }
    return "", "", fmt.Errorf("no valid IPv4 address found")
}

// GenerateIPs generates all possible IP addresses in the subnet defined by the given IP and netmask.
func GenerateIPs(ipStr, maskStr string) ([]string, error) {
    ip := net.ParseIP(ipStr)
    mask := net.IPMask(net.ParseIP(maskStr).To4())
    if ip == nil || mask == nil {
        return nil, fmt.Errorf("invalid IP or mask")
    }
    network := ip.Mask(mask)
    var ips []string
    ones, bits := mask.Size()
    if bits != 32 {
        return nil, fmt.Errorf("unexpected netmask size: %d", bits)
    }
    // Iterate over all possible host addresses in the subnet
    for i := 1; i < 1<<(32-ones)-1; i++ {
        host := make(net.IP, len(network))
        copy(host, network)
        for j := 0; j < 4; j++ {
            host[3-j] += byte((i >> (8 * j)) & 0xFF)
        }
        ips = append(ips, host.String())
    }
    return ips, nil
}
func main() {
    flag.Parse()
    interfaceName := *ifaceFlag
    ip, mask, err := GetInterfaceInfo(interfaceName)
    if err != nil {
        log.Fatal("Error:", err)
    }
    ips, err := GenerateIPs(ip, mask)
    if err != nil {
        log.Fatal("Error generating IPs:", err)
        return
    }
    ARPScan(interfaceName, ips)
}
