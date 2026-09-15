根据ifname、mac和域名对dns查询包打mark

## 使用方法

### 创建DNS标记规则
```
echo 1 >/proc/dns_mark/create
```
这将在 `/proc/dns_mark/` 目录下创建一个名为 `rule1` 的新规则集。

### 添加mac到规则集
```
echo "94:83:C4:19:6D:FD" >/proc/dns_mark/rule1/macs
```

### 添加域名到规则集
```
cat /root/domain1000.txt >/proc/dns_mark/rule1/domains
```

### 修改mark
```
echo 0xa000 >/proc/dns_mark/rule1/mark
```

### 设置状态查询
```
/proc/dns_mark/dump 
```

设置状态示例：
```
root@GL-MT3000:/proc/dns_mark# ls -l 
--w-------    1 root     root             0 Dec 24 16:41 clear
--w-------    1 root     root             0 Dec 24 17:23 create
-r--r--r--    1 root     root             0 Dec 24 14:42 dump
dr-xr-xr-x    8 root     root             0 Dec 24 16:41 rule1
dr-xr-xr-x    8 root     root             0 Dec 24 16:41 rule2
dr-xr-xr-x    8 root     root             0 Dec 24 16:41 rule3
dr-xr-xr-x    8 root     root             0 Dec 24 17:23 rule4
root@GL-MT3000:/proc/dns_mark# 
root@GL-MT3000:/proc/dns_mark# ls -l rule1/
-rw-r--r--    1 root     root             0 Dec 24 16:41 domain_flag
-rw-r--r--    1 root     root             0 Dec 24 17:24 domains
-r--r--r--    1 root     root             0 Dec 24 17:24 dump
-rw-r--r--    1 root     root             0 Dec 24 16:41 mac_flag
-rw-r--r--    1 root     root             0 Dec 24 17:24 macs
-rw-r--r--    1 root     root             0 Dec 24 16:41 mark

root@GL-MT3000:~# cat /proc/dns_mark/dump 
DNS Mark Rules:
Rule 1:
  Ifnames:
  MACs (blacklist=0):
    94:83:c4:19:6d:fd
  Domains (blacklist=1):
  Mark: 0x80000
Rule 2:
  Ifnames:
  MACs (blacklist=1):
    94:83:c4:19:6d:fd
  Domains (blacklist=1):
    ip.gs
    192.1.1.1
  Mark: 0x80000
Rule 3:
  Ifnames:
  MACs (blacklist=1):
  Domains (blacklist=1):
  Mark: 0x1000
```

### mark trace：
```
iptables -t raw -A PREROUTING -p udp -m udp --dport 53 -j TRACE
iptables -t raw -A PREROUTING -p udp -m udp --sport 53 -j TRACE
iptables -t raw -A OUTPUT -p udp -m udp --dport 53 -j TRACE
iptables -t raw -A OUTPUT -p udp -m udp --sport 53 -j TRACE
logread -f
```
搜索MARK关键字

### 解除trace
```
iptables -t raw -D PREROUTING -p udp -m udp --dport 53 -j TRACE
iptables -t raw -D PREROUTING -p udp -m udp --sport 53 -j TRACE
iptables -t raw -D OUTPUT -p udp -m udp --dport 53 -j TRACE
iptables -t raw -D OUTPUT -p udp -m udp --sport 53 -j TRACE
```
