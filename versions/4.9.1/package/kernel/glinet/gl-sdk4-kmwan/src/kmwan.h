#ifndef __KMWAN_H__
#define __KMWAN_H__

#include <linux/string.h>
#include <linux/timekeeping.h>
#include <linux/version.h>
#include <linux/u64_stats_sync.h>

extern rwlock_t gl_netcell_lock;
extern rwlock_t detect_time_lock;
extern struct list_head gl_netcell_head;
extern s64 g_if_detect_time;
extern s64 g_offset_time;
extern unsigned long g_delay;
extern struct delayed_work poll_work;
extern int g_stop_flag;
extern int kmwan_debug_level;

#define gl_netcell_read_lock()       read_lock_bh(&gl_netcell_lock)
#define gl_netcell_read_unlock()     read_unlock_bh(&gl_netcell_lock)
#define gl_netcell_write_lock()      write_lock_bh(&gl_netcell_lock)
#define gl_netcell_write_unlock()    write_unlock_bh(&gl_netcell_lock)

#define IS_IP(skb) (skb->protocol == htons(ETH_P_IP))
#define IS_IPV6(skb) (skb->protocol == htons(ETH_P_IPV6))

#define TYPE_V4 4
#define TYPE_V6 6

#define GL_LOG_LEVEL kmwan_debug_level
#define LOG(level, fmt, ...) do { \
    if ((level) <= GL_LOG_LEVEL) { \
        printk(fmt, ##__VA_ARGS__); \
    } \
} while (0)

#define LLOG(level, fmt, ...) do { \
    if ((level) <= GL_LOG_LEVEL) { \
        pr_info_ratelimited(fmt, ##__VA_ARGS__); \
    } \
} while (0)

#define GL_ERROR(...)           LOG(0, ##__VA_ARGS__)
#define GL_WARN(...)            LOG(1, ##__VA_ARGS__)
#define GL_INFO(...)            LOG(2, ##__VA_ARGS__)
#define GL_DEBUG(...)           LOG(3, ##__VA_ARGS__)

#define GL_LMT_ERROR(...)        LLOG(0, ##__VA_ARGS__)
#define GL_LMT_WARN(...)         LLOG(1, ##__VA_ARGS__)
#define GL_LMT_INFO(...)         LLOG(2, ##__VA_ARGS__)
#define GL_LMT_DEBUG(...)        LLOG(3, ##__VA_ARGS__)

enum netstate {
    IDEL = 0,
    DEAD,
    ACTIVE
};

enum netdir {
    GL_CELL_TX,
    GL_CELL_RX
};

typedef struct dir_statis {
    ktime_t     tstamp;
    u64 packets;
} dir_statis_t;

enum tracktype {
    PING
};

enum trackmode {
    FORCE,
    PASSIVE,
    STRICT
};

enum {
    ETABLE = 1, //1:IPV4 table is empty
    ELOOKUP,    //2:IPV4 table lookup failed
    ERESFI,     //3:IPV4 fib info is empty
    ETABLE6,    //4:IPV6 table is empty
    ERESFI6,    //5:IPV6 fib info is empty
    EDEVICE,    //6:No device found
    ESADDR,     //7:Device address error
    ENEIGH,     //8:No neighbor information found
    EDEVIDX,    //9:No device index
    EOTHER      //10:other error info
};

typedef struct gl_net_track {
    enum tracktype type;
    union {
        __be32           ip;
        struct in6_addr ip6;
    } ipaddr;
} gl_net_track_t;

struct ifstats {
    dir_statis_t tx;
    dir_statis_t rx;
    struct u64_stats_sync syncp;
};

/*
 * trigger_mark: 触发DEAD状态标记 0:初始状态 1:仅由外部通信触发 2:含检测ip触发
 * sync: 接口状态是否同步路由状态 0:不同步，1:同步
 */
typedef struct gl_netcell {
    struct list_head head;
    struct work_struct work;
    struct rcu_head rcu;
    enum netstate state;
    bool online;
    bool probe_enable;
    bool force_dead;
    bool rt_chg;
    char interface[IFNAMSIZ];
    char netdev[IFNAMSIZ];
    char track_mode;
    char trigger_mark;
    int ifindex;
    int track_size;
    int addr_type;
    int sync;
    struct ifstats __percpu *stats;
    __be32 ip;
    __be32 force_ip; //当处于passthrough模式时，使用指定ip去检测，而不是接口ip
    gl_net_track_t tracks[0];
} gl_net_cell_t;

struct gl_active_info {
    atomic_t node_cnt;
    atomic_t node6_cnt;
    atomic_t all_node_cnt;
    atomic_t all_node6_cnt;
};

extern void kmwan_hotplug(struct gl_netcell *node);
extern void kmwan_hotplug_init(struct gl_netcell *node);
extern struct gl_active_info g_active_node;
extern void netcell_poll_work(struct work_struct *work);
extern void send_pkg_by_ifindex(gl_net_cell_t *node, int idx, __be16 seq);
extern int set_fib_nh(char flag, gl_net_cell_t *node);
extern void clean_netcell(void);
extern void config_handle(char *json, size_t len);
extern int  gl_kmwan6_init(void);
extern void gl_kmwan6_exit(void);
extern void increase_package_by_name(struct sk_buff *skb, enum netdir dir, int addr_type, u8 icmp_type);
extern struct fib_table *__fib_get_table(struct net *net, u32 id);
extern int get_ifindex_by_ifname(const char *ifname);
extern __be32 get_ip_by_netdev(struct net_device *dev);

static inline void set_new_period(gl_net_cell_t *node)
{
    int i;
    for_each_possible_cpu(i) {
        struct ifstats *st = per_cpu_ptr(node->stats, i);
        u64_stats_update_begin(&st->syncp);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
        st->tx.tstamp = 0;
        st->rx.tstamp = 0;
#else
        st->tx.tstamp.tv64 = 0;
        st->rx.tstamp.tv64 = 0;
#endif
        st->tx.packets = 0;
        st->rx.packets = 0;
        u64_stats_update_end(&st->syncp);
    }
    if (node->track_mode == PASSIVE)
        node->trigger_mark = 0;
}

static inline int get_active_node_num(struct gl_active_info *active_node, int addr_type)
{
    if (addr_type == TYPE_V4)
        return atomic_read(&active_node->node_cnt);

    return atomic_read(&active_node->node6_cnt);
}

static inline void active_node_total_inc(struct gl_active_info *active_node, int addr_type)
{
    if (addr_type == TYPE_V4)
        atomic_inc(&active_node->all_node_cnt);
    else
        atomic_inc(&active_node->all_node6_cnt);
}

static inline void active_node_total_dec(struct gl_active_info *active_node, int addr_type)
{
    if (addr_type == TYPE_V4)
        atomic_dec(&active_node->all_node_cnt);
    else
        atomic_dec(&active_node->all_node6_cnt);
}

static inline void active_node_inc(struct gl_active_info *active_node,
                                   int addr_type, bool inc_total)
{
    if (inc_total)
        active_node_total_inc(active_node, addr_type);

    if (addr_type == TYPE_V4)
        atomic_inc(&active_node->node_cnt);
    else
        atomic_inc(&active_node->node6_cnt);
}

static inline void active_node_dec(struct gl_active_info *active_node,
                                   int addr_type, bool dec_total)
{
    if (dec_total)
        active_node_total_dec(active_node, addr_type);

    if (!get_active_node_num(active_node, addr_type))
        return;

    if (addr_type == TYPE_V4)
        atomic_dec(&active_node->node_cnt);
    else
        atomic_dec(&active_node->node6_cnt);
}

static inline void set_active_node_zero(struct gl_active_info *active_node)
{
    atomic_set(&active_node->node_cnt, 0);
    atomic_set(&active_node->node6_cnt, 0);
    atomic_set(&active_node->all_node_cnt, 0);
    atomic_set(&active_node->all_node6_cnt, 0);
}

#endif /* __KMWAN_H__ */
