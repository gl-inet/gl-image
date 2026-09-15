#ifndef PORT_FORWARD_H
#define PORT_FORWARD_H

#include <linux/types.h>
#include <linux/list.h>

#define PROC_NAME "port_forward"
#define BUF_SIZE 256

#define PF_PROTO_ALL  0
#define PF_PROTO_TCP  IPPROTO_TCP
#define PF_PROTO_UDP  IPPROTO_UDP
#define PF_NAT_MARK 0x10000

struct pf_zone {
    __be32 net;
    __be32 mask;
    __be32 gw;
    bool   valid;
};

static struct pf_zone pf_lan_zone;
static struct pf_zone pf_guest_zone;

struct pf_rule {
    u8 proto;
    __be32 match_ip;
    u16 src_start;
    u16 src_end;
    u16 dst_start;
    u16 dst_end;
    __be32 dst_ip;
    struct list_head list;
};

#endif

