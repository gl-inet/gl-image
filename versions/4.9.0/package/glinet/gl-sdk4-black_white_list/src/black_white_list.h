#ifndef __BLACK_WHITE_LIST__
#define __BLACK_WHITE_LIST__

#include <linux/if_ether.h>
#include <linux/kern_levels.h>

#define MAC_HASHENTRIES         25
#define BUFSIZE                 256
#define CMD_LEN                 3
#define MODE_LEN                5
#define ENALBE_LEN              6
#define DISABLE_LEN             7

#define ISOLATE_UN_SET         (0)
#define LAN_ISOLATE_SET        (1 << LAN_ISOLATE_BIT)
#define GUEST_ISOLATE_SET      (1 << GUEST_ISOLATE_BIT)
#define IOT_ISOLATE_SET      (1 << IOT_ISOLATE_BIT)
#define LAN_TRANSFER_SET       (1 << LAN_TRANSFER_BIT)
#define ISOLATE_SET_ALL        (LAN_ISOLATE_SET | GUEST_ISOLATE_SET | IOT_ISOLATE_SET)
#define ISOLATE_MASK           (ISOLATE_SET_ALL)
#define NET_SE_CONF_MAX        (LAN_ISOLATE_SET | GUEST_ISOLATE_SET | LAN_TRANSFER_SET | IOT_ISOLATE_SET)

#define STRING_MAC_LEN        17

struct dev_info {
    struct hlist_node hlist;
    struct rcu_head rcu;
    u8 mac[ETH_ALEN];
};

enum {
    BLACK = 0,
    WHITE,
    DISABLE
};

enum {
    LIST_ADD = 0,
    LIST_DEL,
    LIST_CLR,
    LIST_SW_SET,
    LIST_MODE_SET,
    LIST_UNDEF
};

enum net_secure_conf {
    LAN_ISOLATE_BIT = 0,
    GUEST_ISOLATE_BIT,
    LAN_TRANSFER_BIT,
    IOT_ISOLATE_BIT,
};

#endif
