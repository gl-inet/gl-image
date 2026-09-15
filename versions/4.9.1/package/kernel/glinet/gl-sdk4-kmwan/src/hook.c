#include <linux/init.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/timekeeping.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/in.h>
#include <linux/inetdevice.h>
#include <net/netfilter/nf_conntrack.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <uapi/linux/icmp.h>

#include "kmwan.h"

struct delayed_work poll_work;
int g_stop_flag = 0;
struct gl_active_info g_active_node;
int kmwan_debug_level = 2;
unsigned short DNS_PORT = htons(53);

static s64 get_ktime_val(ktime_t t)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
    return t;
#else
    return t.tv64;
#endif
}

static void track_node(gl_net_cell_t *node, int tx_flag, bool force)
{
    int i;
    static __be16 seq = 1;

    if (force) {
        /*
         * 只有处于被动模式下的节点由于外部通信导致dead时才会进来
         * 这里，为了确认节点的真实状态，必须先将每个cpu的发包时间
         * 置为0.然后再通过track_ip进行一次检测
         */
        for_each_possible_cpu(i) {
            struct ifstats *st = per_cpu_ptr(node->stats, i);
            u64_stats_update_begin(&st->syncp);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
            st->tx.tstamp = 0;
#else
            st->tx.tstamp.tv64 = 0;
#endif
            u64_stats_update_end(&st->syncp);
        }
        goto force_send;
    }

    if (node->track_mode == PASSIVE && node->state != DEAD) //被动模式下，仅在接口为DEAD状态是才主动监测网络
        return;

force_send:
    for (i = 0; i < node->track_size; i++) {
        struct ifstats *st;

        /* 进入这里，说明node需要发包来探测当前的网络是否正常
         * send_pkg_by_ifindex函数并不保证发包成功，但当它发包
         * 失败时,说明当前网络环境异常，我们需将其视作发包成功,
         * 方便进行网络的切换。
         */
        send_pkg_by_ifindex(node, i, seq++);
        st = this_cpu_ptr(node->stats);
        u64_stats_update_begin(&st->syncp);

        if (i == 0 && (!get_ktime_val(st->tx.tstamp) || tx_flag))
            st->tx.tstamp = ktime_get_boottime();

        st->tx.packets++;
        u64_stats_update_end(&st->syncp);
    }

    if (node->track_mode == PASSIVE)
        node->trigger_mark = 2;
}

static bool is_normal_ping(struct sk_buff *skb, u8 icmp_type)
{
    struct ipv6hdr *ip6h;
    struct icmp6hdr *icmp6h;

    if (IS_IP(skb)) {
        struct iphdr *iph = ip_hdr(skb);
        struct icmphdr *icmph;

        if (iph->protocol != IPPROTO_ICMP)
            return false;

        icmph = icmp_hdr(skb);

        return icmph->type == icmp_type && icmph->code == 0;
    }

    ip6h = ipv6_hdr(skb);
    if (ip6h->nexthdr != IPPROTO_ICMPV6) {
        return false;
    }

    icmp6h = icmp6_hdr(skb);
    return icmp6h->icmp6_type == icmp_type && icmp6h->icmp6_code == 0;
}

/*
 * When a raw ICMPv6 reply response packet is received on the modem interface,
 * the ICMPv6 header cannot be obtained normally. Therefore, the method of judging
 * the source IPv6 address of the data packet is adopted.
 */
static bool is_raw_ip6_ping(struct sk_buff *skb, gl_net_cell_t *node)
{
    struct ipv6hdr *ip6h = NULL;
    struct in6_addr *saddr = NULL;
    int i = 0;

    if (IS_IP(skb) || !node) {
        return false;
    }

    ip6h = ipv6_hdr(skb);
    if (ip6h && ip6h->nexthdr != IPPROTO_ICMPV6) {
        return false;
    }

    saddr = &ip6h->saddr;
    for (i = 0; i < node->track_size; i++) {
        if (ipv6_addr_equal(saddr, &node->tracks[i].ipaddr.ip6)) {
            return true;
        }
    }

    return false;
}

void increase_package_by_name(struct sk_buff *skb, enum netdir dir, int addr_type, u8 icmp_type)
{
    gl_net_cell_t *node = NULL;
    struct ifstats *st = NULL;

    //use the ktime_get_boottime include the time spent in suspend
    ktime_t t = ktime_get_boottime();
    rcu_read_lock();
    list_for_each_entry_rcu(node, &gl_netcell_head, head) {
        if (skb->dev->ifindex == node->ifindex && node->addr_type == addr_type) {
            st = this_cpu_ptr(node->stats);

            u64_stats_update_begin(&st->syncp);
            if (dir == GL_CELL_TX) {
                if (node->track_mode != STRICT)
                    st->tx.packets++;
                else if (is_normal_ping(skb, icmp_type))
                    st->tx.packets++;

                if (get_ktime_val(st->tx.tstamp) == 0) //only recode the first TX time
                    st->tx.tstamp = t;

                if (node->track_mode == PASSIVE && !node->trigger_mark)
                    node->trigger_mark = 1;
            } else {
                if (node->track_mode != STRICT)
                    st->rx.packets++;
                else if (is_normal_ping(skb, icmp_type))
                    st->rx.packets++;
                else if (is_raw_ip6_ping(skb, node))
                    st->rx.packets++;

                st->rx.tstamp = t;//recode the last RX time
            }
            u64_stats_update_end(&st->syncp);
        }
    }
    rcu_read_unlock();
}

static void active_to_dead(gl_net_cell_t *node, char *rtflag)
{
    active_node_dec(&g_active_node, node->addr_type, false);

    if (get_active_node_num(&g_active_node, node->addr_type) > 0) {
        node->sync = 1;
        *rtflag = 1;
    } else {
        node->sync = 0;
    }
    node->state = DEAD;
    node->online = false;
}

static void dead_to_dead(gl_net_cell_t *node, char *rtflag)
{
    if (get_active_node_num(&g_active_node, node->addr_type) > 0 && node->sync == 0) {
        node->sync = 1;
        *rtflag = 1;
    }
}

static void dead_to_active(gl_net_cell_t *node, char *rtflag)
{
    node->state = ACTIVE;
    active_node_inc(&g_active_node, node->addr_type, false);

    if (node->sync == 1)
        *rtflag = 2;

    node->sync = 1;
    node->online = true;
    node->rt_chg = false;
}

/*
 * 处于被动模式下的节点非DEAD状态下不会发ping包检测网络状态，此时会由外部通信触发节
 * 点dead，(modem)抓包发现其收发包时间差达到3秒多，这意味着检测间隔至少要设置10秒左
 * 右，才能保证节点状态正常，因此当仅由外部通信触发时，需检测网络真实状态状态。
 */
static bool need_send_dectect_pkg(gl_net_cell_t *node)
{
    return node->track_mode == PASSIVE && node->trigger_mark != 2;
}

static void update_netcell_status(int addr_type)
{
    gl_net_cell_t *node = NULL;
    ktime_t now = ktime_get_boottime();
    s64 time_tx_diff = 0;
    gl_net_cell_t *tmp_node;
    int total_cnt = 0, dead_cnt = 0;

    rcu_read_lock();
    //use the ktime_get_boottime include the time spent in suspend
    //do not put it in the loop, so as not to create a condition in the loop time difference
    list_for_each_entry_rcu(node,    &gl_netcell_head, head) {
        char route_flag = 0; //0: no change 1: dead 2:active
        int i, tx_flag = 1;
        u64 tx_packets_sum = 0, rx_packets_sum = 0;
        s64 tx_stamp = 0;

        if (node->addr_type != addr_type || !node->probe_enable)
            continue;

        tmp_node = node;
        total_cnt++;
        GL_DEBUG("[%s %d]iface:%s ifindex:%d cur status:%s\n", __FUNCTION__, __LINE__, node->interface,
                 node->ifindex, node->state == ACTIVE ? "ACTIVE" : node->state == DEAD ? "DEAD" : "IDEL");

        for_each_possible_cpu(i) {
            struct ifstats *st = per_cpu_ptr(node->stats, i);

            u64_stats_update_begin(&st->syncp);
            tx_packets_sum += st->tx.packets;
            rx_packets_sum += st->rx.packets;
            if (get_ktime_val(st->tx.tstamp))
                tx_stamp = tx_stamp ? min(get_ktime_val(st->tx.tstamp), tx_stamp) : get_ktime_val(st->tx.tstamp);
            u64_stats_update_end(&st->syncp);
        }

        time_tx_diff = get_ktime_val(now) - tx_stamp;
        if (time_tx_diff + g_offset_time < g_if_detect_time) {
            tx_flag = 0;
            if (node->state == DEAD && node->sync == 1) {
                dead_cnt++;
            }
            goto send;
        }

        switch (node->state) {
            case IDEL:
                node->online = true;
                node->sync = 1;
                //IDEL->ACTIVE
                if (rx_packets_sum) {
                    node->state = ACTIVE;
                } else if (tx_packets_sum && !rx_packets_sum) {
                    if (need_send_dectect_pkg(node)) {
                        track_node(node, tx_flag, true);
                        continue;
                    }
                    active_to_dead(node, &route_flag);
                    kmwan_hotplug(node);
                    dead_cnt = node->sync == 1 ? dead_cnt + 1 : dead_cnt;
                }
                break;
            case DEAD:
                //DEAD->IDEL not allowed
                if (rx_packets_sum && !node->force_dead) {
                    dead_to_active(node, &route_flag);
                    kmwan_hotplug(node);
                } else {
                    dead_to_dead(node, &route_flag);
                    dead_cnt = node->sync == 1 ? dead_cnt + 1 : dead_cnt;
                    if (node->rt_chg) {
                        route_flag = 1;
                        node->rt_chg = false;
                        node->sync = 1;
                    }
                }
                break;
            case ACTIVE:
                node->online = true;
                node->sync = 1;
                //ACTIVE->IDEL
                if (!tx_packets_sum && !rx_packets_sum) {
                    node->state = IDEL;
                } else if (tx_packets_sum && !rx_packets_sum) {
                    if (need_send_dectect_pkg(node)) {
                        track_node(node, tx_flag, true);
                        continue;
                    }
                    active_to_dead(node, &route_flag);
                    kmwan_hotplug(node);
                    dead_cnt = node->sync == 1 ? dead_cnt + 1 : dead_cnt;
                }
                break;
            default:
                break;
        }
        set_new_period(node);
        if (route_flag) {
            int err;
            rcu_read_unlock();
            err = set_fib_nh(route_flag, node);
            if (err) {
                GL_DEBUG("[%s %d]set nh route_flag failed.Ecode:%d\n", __FUNCTION__, __LINE__, err);
            }
            rcu_read_lock();
        }
        GL_DEBUG("[%s %d]iface:%s ifindex:%d next status:%s\n", __FUNCTION__, __LINE__, node->interface,
                 node->ifindex, node->state == ACTIVE ? "ACTIVE" : node->state == DEAD ? "DEAD" : "IDEL");
send:
        track_node(node, tx_flag, false);
    }

    rcu_read_unlock();

    if (dead_cnt && total_cnt == dead_cnt) {
        set_fib_nh(ACTIVE, tmp_node);
        gl_netcell_write_lock();
        tmp_node->sync = 0;
        gl_netcell_write_unlock();
    }

}

void netcell_poll_work(struct work_struct *work)
{
    update_netcell_status(TYPE_V4);
    update_netcell_status(TYPE_V6);

    if (kmwan_debug_level == 4)
        g_delay = HZ;

    mod_delayed_work(system_long_wq, &poll_work, g_delay);
}

static int check_special_ip(__be32 ip)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
    bool flag = ipv4_is_all_snoopers(ip);
#else
    bool flag = false;
#endif

    return ipv4_is_private_10(ip) || ipv4_is_private_172(ip)
           || ipv4_is_private_192(ip) || ipv4_is_linklocal_169(ip)
           || ipv4_is_loopback(ip) || ipv4_is_test_192(ip)
           || ipv4_is_test_198(ip) || ipv4_is_anycast_6to4(ip)
           || ipv4_is_multicast(ip) || ipv4_is_local_multicast(ip)
           || ipv4_is_lbcast(ip) || flag || ipv4_is_zeronet(ip);
}

#if 1
static void change_conntrack_info(struct sk_buff *skb)
{
    enum ip_conntrack_info ctinfo;
    struct iphdr *iph = ip_hdr(skb);
    struct nf_conn *ct = nf_ct_get(skb, &ctinfo);

    if (ct) {
        ct->tuplehash[1].tuple.dst.u3.ip = iph->saddr;
        GL_DEBUG("[%s %d]now dip is %pI4\n", __FUNCTION__, __LINE__, &ct->tuplehash[1].tuple.dst.u3.ip);
    }
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static u_int32_t netstatus_pre_hook(void *priv,
                                    struct sk_buff *skb,
                                    const struct nf_hook_state *state)
{
#else
static u_int32_t netstatus_pre_hook(unsigned int hook,
                                    struct sk_buff *skb,
                                    const struct net_device *in,
                                    const struct net_device *out,
                                    int (*okfn)(struct sk_buff *))
{
#endif

    struct iphdr *iph;

    if (!IS_IP(skb))
        return NF_ACCEPT;

    iph = ip_hdr(skb);
    if (check_special_ip(iph->saddr))
        return NF_ACCEPT;

    if (iph->protocol == IPPROTO_UDP && udp_hdr(skb)->source == DNS_PORT)
        return NF_ACCEPT;

    if (skb->dev) {
        GL_DEBUG("%s get RX package from %-16pI4\n", skb->dev->name, &iph->saddr);
        increase_package_by_name(skb, GL_CELL_RX, TYPE_V4, ICMP_ECHOREPLY);
    }

    return NF_ACCEPT;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static u_int32_t netstatus_post_hook(void *priv,
                                     struct sk_buff *skb,
                                     const struct nf_hook_state *state)
{
#else
static u_int32_t netstatus_post_hook(unsigned int hook,
                                     struct sk_buff *skb,
                                     const struct net_device *in,
                                     const struct net_device *out,
                                     int (*okfn)(struct sk_buff *))
{
#endif

    struct iphdr *iph;

    if (!IS_IP(skb))
        return NF_ACCEPT;

    iph = ip_hdr(skb);
    if (check_special_ip(iph->daddr))
        return NF_ACCEPT;

    if (iph->protocol == IPPROTO_UDP && udp_hdr(skb)->dest == DNS_PORT)
        return NF_ACCEPT;

    if (skb->dev) {
        GL_DEBUG("%s get TX package to %-16pI4\n", skb->dev->name, &iph->daddr);
        increase_package_by_name(skb, GL_CELL_TX, TYPE_V4, ICMP_ECHO);
    }

    return NF_ACCEPT;
}

#if 1
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static u_int32_t saddr_conversion_hook(void *priv,
                                       struct sk_buff *skb,
                                       const struct nf_hook_state *state)
{
#else
static u_int32_t saddr_conversion_hook(unsigned int hook,
                                       struct sk_buff *skb,
                                       const struct net_device *in,
                                       const struct net_device *out,
                                       int (*okfn)(struct sk_buff *))
{
#endif
    struct iphdr *iph;
    int ifindex;
    __be32 ip, dev_ip = 0;
    gl_net_cell_t *node;
    bool do_nat = false;

    if (!IS_IP(skb))
        return NF_ACCEPT;

    iph = ip_hdr(skb);
    if (check_special_ip(iph->daddr))
        return NF_ACCEPT;

    ip = iph->saddr;
    if (skb->dev) {
        ifindex = skb->dev->ifindex;
        list_for_each_entry_rcu(node, &gl_netcell_head, head) {
            if (ip == node->ip)     // cppcheck-suppress uninitvar
                do_nat = true;

            if (node->ifindex == ifindex)
                dev_ip = get_ip_by_netdev(skb->dev);
        }
    }

    if (!do_nat || dev_ip == 0)
        return NF_ACCEPT;

    if (dev_ip ^ ip) {
        iph->saddr = dev_ip;
        change_conntrack_info(skb);
    }

    return NF_ACCEPT;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static struct nf_hook_ops netstatus_ops[] __read_mostly = {
    {
        .hook = netstatus_pre_hook,
        .pf = PF_INET,
        .hooknum = NF_INET_PRE_ROUTING,
        .priority = NF_IP_PRI_FIRST + 1,
    },
    {
        .hook = netstatus_post_hook,
        .pf = PF_INET,
        .hooknum = NF_INET_POST_ROUTING,
        .priority = NF_IP_PRI_FIRST + 1,
    },
#if 1
    {
        .hook = saddr_conversion_hook,
        .pf = PF_INET,
        .hooknum = NF_INET_POST_ROUTING,
        .priority = NF_IP_PRI_CONNTRACK_HELPER - 1,
    },
#endif
};
#else
static struct nf_hook_ops netstatus_ops[] __read_mostly = {
    {
        .hook = netstatus_pre_hook,
        .pf = PF_INET,
        .owner = THIS_MODULE,
        .hooknum = NF_INET_PRE_ROUTING,
        .priority = NF_IP_PRI_FIRST + 1,
    },
    {
        .hook = netstatus_post_hook,
        .pf = PF_INET,
        .owner = THIS_MODULE,
        .hooknum = NF_INET_POST_ROUTING,
        .priority = NF_IP_PRI_FIRST + 1,
    },
#if 1
    {
        .hook = saddr_conversion_hook,
        .pf = PF_INET,
        .owner = THIS_MODULE,
        .hooknum = NF_INET_POST_ROUTING,
        .priority = NF_IP_PRI_CONNTRACK_HELPER - 1,
    },
#endif
};
#endif

static int status_proc_show(struct seq_file *s, void *v)
{
    gl_net_cell_t *node = NULL;
    int i;
    seq_printf(s, "%-16s%-17s%-10s%-15s%-16s%-16s%-16s%-13s%-16s\n", "Interface", "Netdev",
               "Ifindex", "State", "TrackMode", "TX packets", "TX stamp", "RX packets", "RX stamp");

    rcu_read_lock_bh();
    list_for_each_entry_rcu(node, &gl_netcell_head, head) {
        u64 tx_packets_sum = 0, rx_packets_sum = 0;
        s64 tx_stamp = 0, rx_stamp = 0;

        for_each_possible_cpu(i) {
            struct ifstats *st = per_cpu_ptr(node->stats, i);
            unsigned int start;

            do {
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
                start = u64_stats_fetch_begin_irq(&st->syncp);
#else
                start = u64_stats_fetch_begin(&st->syncp);
#endif
                tx_packets_sum += st->tx.packets;
                rx_packets_sum += st->rx.packets;

                if (get_ktime_val(st->tx.tstamp))
                    tx_stamp = tx_stamp ? min(get_ktime_val(st->tx.tstamp), tx_stamp) : get_ktime_val(st->tx.tstamp);

                if (get_ktime_val(st->rx.tstamp))
                    rx_stamp = rx_stamp ? max(get_ktime_val(st->rx.tstamp), rx_stamp) : get_ktime_val(st->rx.tstamp);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
            } while (u64_stats_fetch_retry_irq(&st->syncp, start));
#else
            }
            while (u64_stats_fetch_retry(&st->syncp, start));
#endif
        }

        seq_printf(s, "%-16s%-17s%-10d%-15s%-16s%-16llu%-16lld%-13llu%-16lld\n", node->interface,
                   node->netdev, node->ifindex,
                   node->state == IDEL ? "IDEL" :
                   node->state == ACTIVE ? "ACTIVE" :
                   node->track_mode != PASSIVE ? "DEAD" : "UNSTABLE",
                   node->track_mode == FORCE ? "force" :
                   node->track_mode == PASSIVE ? "passive" : "strict",
                   tx_packets_sum, tx_stamp,
                   rx_packets_sum, rx_stamp);

        if (node->track_size > 0)
            seq_printf(s, "Track method\tip Info\n");

        if (node->addr_type == TYPE_V4)
            for (i = 0; i < node->track_size; i++) {
                seq_printf(s, "%-12s\t%pI4\n", node->tracks[i].type == PING ? "ping" : "ping", &node->tracks[i].ipaddr.ip);
            } else
            for (i = 0; i < node->track_size; i++) {
                seq_printf(s, "%-12s\t%pI6\n", node->tracks[i].type == PING ? "ping" : "ping", &node->tracks[i].ipaddr.ip6.s6_addr);
            }
        seq_printf(s, "online:%-9sstate_sync:%d\n", node->online ? "true" : "false", node->sync);
        seq_printf(s, "probe_enable:%s\n", node->probe_enable ? "true" : "false");
        seq_printf(s, "force_dead:%s\n", node->force_dead ? "true" : "false");
        seq_printf(s, "\n");
    }
    rcu_read_unlock_bh();

    return 0;
}

static int status_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, status_proc_show, NULL);
}

static int config_proc_show(struct seq_file *s, void *v)
{
    gl_net_cell_t *node = NULL;
    rcu_read_lock();
    list_for_each_entry_rcu(node, &gl_netcell_head, head) {
        seq_printf(s, "%s:%s\n", node->interface, node->online ? "online" : "offline");
    }
    rcu_read_unlock();

    return 0;
}

static int config_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, config_proc_show, NULL);
}

static ssize_t config_proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char *conf = kzalloc(size, GFP_KERNEL);

    if (!conf)
        return -EFAULT;

    if (copy_from_user(conf, buf, size)) {
        kfree(conf);
        return -EFAULT;
    }
    config_handle(conf, size);

    kfree(conf);
    return size;
}

static int debug_proc_show(struct seq_file *s, void *v)
{
    seq_printf(s, "debug_level:0:ERR 1:WARN 2:INFO 3:DEBUG 4:DEBUG(packet sending interval:1s)\n");
    seq_printf(s, "%-17s : %d\n%-17s : %lu\n%-17s : %u\n%-17s : %lld\n%-17s : %d\n%-17s : %d\n%-17s : %d\n%-17s : %d\n",
               "cur_level", kmwan_debug_level,
               "delay", g_delay,
               "freq", HZ,
               "offset_time", g_offset_time,
               "ipv4 active nodes", get_active_node_num(&g_active_node, TYPE_V4),
               "ipv4 node total", atomic_read(&g_active_node.all_node_cnt),
               "ipv6 active nodes", get_active_node_num(&g_active_node, TYPE_V6),
               "ipv6 node total", atomic_read(&g_active_node.all_node6_cnt));

    read_lock_bh(&detect_time_lock);
    seq_printf(s, "g_if_detect_time  : %llu ns\n\n", g_if_detect_time);
    read_unlock_bh(&detect_time_lock);
    return 0;
}

static int debug_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, debug_proc_show, NULL);
}

static ssize_t debug_proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char tmp_buf[8] = {0};
    char ch;

    if (copy_from_user(tmp_buf, buf, size))
        return -EFAULT;

    ch = tmp_buf[size - 1];
    tmp_buf[size - 1] = (ch == '\n' ? '\0' : ch);

    if (strlen(tmp_buf) == 1)
        kmwan_debug_level = tmp_buf[0] - '0';

    return size;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 5, 0)
static const struct file_operations netstatus_status_fops = {
    .owner = THIS_MODULE,
    .open = status_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
static const struct file_operations netstatus_config_fops = {
    .owner = THIS_MODULE,
    .read = seq_read,
    .open = config_proc_open,
    .write = config_proc_write,
    .llseek = seq_lseek,
    .release = single_release,
};
static const struct file_operations netstatus_debug_fops = {
    .owner = THIS_MODULE,
    .read = seq_read,
    .open = debug_proc_open,
    .write = debug_proc_write,
    .llseek = seq_lseek,
    .release = single_release,
};
#else
static const struct proc_ops netstatus_status_fops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_read = seq_read,
    .proc_open = status_proc_open,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
static const struct proc_ops netstatus_config_fops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_read = seq_read,
    .proc_open = config_proc_open,
    .proc_write = config_proc_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
static const struct proc_ops netstatus_debug_fops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_read = seq_read,
    .proc_open = debug_proc_open,
    .proc_write = debug_proc_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#endif

static int netstatus_init_procfs(void)
{
    struct proc_dir_entry *proc;

    proc = proc_mkdir("gl-kmwan", NULL);
    if (!proc) {
        GL_ERROR("can't create dir /proc/gl-kmwan/\n");
        return -ENODEV;;
    }

    proc_create("status", 0644, proc, &netstatus_status_fops);
    proc_create("config", 0644, proc, &netstatus_config_fops);
    proc_create("debug", 0644, proc, &netstatus_debug_fops);

    return 0;
}

static void update_dev_ifindex(struct net_device *dev)
{
    gl_net_cell_t *node = NULL;

    rcu_read_lock();
    list_for_each_entry_rcu(node, &gl_netcell_head, head) {
        if (!strcmp(dev->name, node->netdev)) {
            if (unlikely(dev->ifindex != node->ifindex)) {
                node->ifindex = dev->ifindex;
                rcu_read_unlock();
                return;
            }
        }
    }
    rcu_read_unlock();
}

static int kmwan_device_event(struct notifier_block *unused,
                              unsigned long event, void *ptr)
{
    struct net_device *dev = netdev_notifier_info_to_dev(ptr);

    switch (event) {
        case NETDEV_CHANGENAME:
        case NETDEV_REGISTER:
        case NETDEV_UP:
            update_dev_ifindex(dev);
            break;
    }

    return NOTIFY_DONE;
}

static struct notifier_block kmwan_notifier_block = {
    .notifier_call = kmwan_device_event,
};

static int __init netstatus_init(void)
{
    int ret = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
    ret = nf_register_net_hooks(&init_net, netstatus_ops, ARRAY_SIZE(netstatus_ops));
#else
    ret = nf_register_hooks(netstatus_ops, ARRAY_SIZE(netstatus_ops));
#endif

    if (ret) {
        GL_ERROR("[%s %d] register netstatus_ops failed.\n", __FUNCTION__, __LINE__);
        goto err;
    }

    register_netdevice_notifier(&kmwan_notifier_block);
    ret = gl_kmwan6_init();
    if (ret) {
        GL_ERROR("[%s %d] register kmwan6_ops failed.\n", __FUNCTION__, __LINE__);
        goto err;
    }

    set_active_node_zero(&g_active_node);
    netstatus_init_procfs();
    INIT_DELAYED_WORK(&poll_work, netcell_poll_work);
    mod_delayed_work(system_long_wq, &poll_work, 0);
    // GL_INFO("gl-kmwan: (C) 2023 chongjun luo <luochognjun@gl-inet.com>\n");

    return 0;
err:
    return -1;
}

static void __exit netstatus_exit(void)
{
    GL_INFO("=====================  kmwan exit ==================\n");
    cancel_delayed_work_sync(&poll_work);
    g_stop_flag = 1;
    remove_proc_subtree("gl-kmwan", NULL);
    clean_netcell();
    gl_kmwan6_exit();
    unregister_netdevice_notifier(&kmwan_notifier_block);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
    nf_unregister_net_hooks(&init_net, netstatus_ops, ARRAY_SIZE(netstatus_ops));
#else
    nf_unregister_hooks(netstatus_ops, ARRAY_SIZE(netstatus_ops));
#endif
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("luochongjun@gl-inet.com");
MODULE_DESCRIPTION("kmwan module");
MODULE_VERSION("1.0");
module_init(netstatus_init);
module_exit(netstatus_exit);

