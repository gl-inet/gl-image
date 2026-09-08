/*
 *  Copyright (C) 2019 jianhui zhao <zhaojh329@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/jhash.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
#include <asm/unaligned.h>
#else
#include <linux/unaligned.h>
#endif
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/etherdevice.h>
#include <linux/netfilter.h>
#include <linux/inetdevice.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/inet.h>
#include <asm/div64.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/ndisc.h>
#include <net/addrconf.h>
#include <linux/kobject.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/icmpv6.h>
#include <linux/rcupdate.h>

#include "term.h"

#define TERM_HASH_SIZE              (1 << 8)
#define TERM_EVENT_BUF_SIZE         512

static struct kmem_cache *term_cache __read_mostly;
static struct kmem_cache *term_ipv6_cache __read_mostly;
static struct hlist_head terms[TERM_HASH_SIZE];
static u32 hash_rnd __read_mostly;

/*
* keepalive_work:
* Before timeout, ICMP message is sent first. After the timeout time is reached, if the device is not offline,
* the time will be updated, otherwise the device will be offline.
*/
struct delayed_work keepalive_work;
struct delayed_work gc_work;
struct delayed_work keepalive_work_ipv6;
struct delayed_work gc_work_ipv6;

static unsigned long ttl;
static unsigned long icmp_ttl;

static DEFINE_SPINLOCK(hash_lock);

extern u64 uevent_next_seqnum(void);

int send_ns(struct net_device *dev, const struct in6_addr *target_addr);
bool is_in6_addr_zero(struct in6_addr *addr);

static int term_event_add_var(struct term_event *event, int argv,
                              const char *format, ...)
{
    static char buf[128];
    char *s;
    va_list args;
    int len;

    if (argv)
        return 0;

    va_start(args, format);
    len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (len >= sizeof(buf)) {
        WARN_ON(1);
        return -ENOMEM;
    }

    s = skb_put(event->skb, len + 1);
    strncpy(s, buf, sizeof(buf) - 1);

    return 0;
}

static const char *event_action_str(int act)
{
    switch (act) {
        case TERM_EVENT_ADD:
            return "add";
        case TERM_EVENT_DEL:
            return "del";
        case TERM_EVENT_CHANGE:
            return "chg";
        default:
            return NULL;
    }
}

static int hotplug_fill_event(struct term_event *event, const char *action)
{
    int ret;

    ret = term_event_add_var(event, 0, "HOME=%s", "/");
    if (ret)
        return ret;

    ret = term_event_add_var(event, 0, "PATH=%s",
                             "/sbin:/bin:/usr/sbin:/usr/bin");
    if (ret)
        return ret;

    ret = term_event_add_var(event, 0, "SUBSYSTEM=%s", "gl-tertf");
    if (ret)
        return ret;

    ret = term_event_add_var(event, 0, "ACTION=%s", action);
    if (ret)
        return ret;

    ret = term_event_add_var(event, 0, "MAC=%pM", event->mac);
    if (ret)
        return ret;

    ret = term_event_add_var(event, 0, "IP=%pI4", &event->ip);
    if (ret)
        return ret;

    if (event->action == TERM_EVENT_ADD) {
        ret = term_event_add_var(event, 0, "DEV=%s", event->dev);
        if (ret)
            return ret;
    }

    ret = term_event_add_var(event, 0, "SEQNUM=%llu", uevent_next_seqnum());

    return ret;
}

static void hotplug_work(struct work_struct *work)
{
    struct term_event *event = container_of(work, struct term_event, work);
    const char *action = event_action_str(event->action);
    int ret = 0;

    if (!action)
        goto out_free_event;

    event->skb = alloc_skb(TERM_EVENT_BUF_SIZE, GFP_KERNEL);
    if (!event->skb)
        goto out_free_event;

    ret = term_event_add_var(event, 0, "%s@", action);
    if (ret)
        goto out_free_skb;

    ret = hotplug_fill_event(event, action);
    if (ret)
        goto out_free_skb;

    NETLINK_CB(event->skb).dst_group = 1;
    broadcast_uevent(event->skb, 0, 1, GFP_KERNEL);

out_free_skb:
    if (ret) {
        kfree_skb(event->skb);
    }
out_free_event:
    kfree(event);
}

static void hotplug_create_event(struct terminal *term, int action)
{
    struct term_event *event;

    event = kzalloc(sizeof(struct term_event), GFP_ATOMIC);
    if (!event)
        return;

    event->ip = term->ip;

    memcpy(event->mac, term->mac, ETH_ALEN);

    if (action == TERM_EVENT_ADD)
        strncpy(event->dev, netdev_name(term->dev), sizeof(event->dev) - 1);

    event->action = action;

    INIT_WORK(&event->work, hotplug_work);
    schedule_work(&event->work);
}

static inline int term_mac_hash(const u8 *mac)
{
    /* use 1 byte of OUI and 3 bytes of NIC */
    u32 key = get_unaligned((u32 *)(mac + 2));
    return jhash_1word(key, hash_rnd) & (TERM_HASH_SIZE - 1);
}

static void term_rcu_free(struct rcu_head *head)
{
    struct terminal *term = container_of(head, struct terminal, rcu);
    struct term_ipv6 *node, *next;

#if 0
    pr_info("term: %pM %pI4 freed\n", term->mac, &term->ip);
#endif

    hotplug_create_event(term, TERM_EVENT_DEL);

    dev_put(term->dev);
    free_percpu(term->stats);

    list_for_each_entry_safe(node, next, &term->ipv6_list, list_node) {
        list_del(&node->list_node);
        kmem_cache_free(term_ipv6_cache, node);
    }

    kmem_cache_free(term_cache, term);
}

static void term_delete(struct terminal *term)
{
    hlist_del_rcu(&term->hlist);
    call_rcu(&term->rcu, term_rcu_free);
}

static struct terminal *find_term_rcu(struct hlist_head *head, const u8 *mac)
{
    struct terminal *term = NULL;

    hlist_for_each_entry_rcu(term, head, hlist) {
        if (ether_addr_equal(term->mac, mac))
            return term;
    }

    return NULL;
}

static struct terminal *find_term(struct hlist_head *head, const u8 *mac)
{
    struct terminal *term = NULL;

    hlist_for_each_entry(term, head, hlist) {
        if (ether_addr_equal(term->mac, mac))
            return term;
    }

    return NULL;
}

void term_create(const u8 *mac, __be32 addr, struct net_device *dev)
{
    struct hlist_head *head = &terms[term_mac_hash(mac)];
    struct terminal *term, *old;
    int i;

    term = find_term_rcu(head, mac);
    if (likely(term && term->dev == dev))
        return;

    term = kmem_cache_zalloc(term_cache, GFP_ATOMIC);
    if (!term)
        return;

    term->stats = alloc_percpu_gfp(struct term_stats, GFP_ATOMIC);
    if (!term->stats) {
        kmem_cache_free(term_cache, term);
        return;
    }

    spin_lock_init(&term->ipv6_lock);

    INIT_LIST_HEAD(&term->ipv6_list);

    for_each_possible_cpu(i) {
        struct term_stats *st = per_cpu_ptr(term->stats, i);
        u64_stats_init(&st->syncp);
    }

    dev_hold(dev);

    term->ip = addr;
    term->dev = dev;
    memcpy(term->mac, mac, ETH_ALEN);

#if 0
    pr_info("term: %pM %pI4 added\n", term->mac, &term->ip);
#endif

    hotplug_create_event(term, TERM_EVENT_ADD);

    spin_lock(&hash_lock);
    old = find_term(head, mac);
    if (unlikely(old)) {
        hlist_replace_rcu(&old->hlist, &term->hlist);
        call_rcu(&old->rcu, term_rcu_free);
    } else {
        hlist_add_head_rcu(&term->hlist, head);
    }
    spin_unlock(&hash_lock);
}

void term_update(const u8 *mac, __be32 addr, unsigned int rx, unsigned int tx, bool alive)
{
    struct hlist_head *head = &terms[term_mac_hash(mac)];
    struct terminal *term;
    struct term_stats *st;

    term = find_term_rcu(head, mac);
    if (!term)
        return;

    /* Check if term->stats is valid to avoid RCU race condition */
    if (!term->stats)
        return;

    if (alive)
        term->updated = jiffies;

    if (unlikely(addr != term->ip && addr != 0)) {
        term->ip = addr;
        hotplug_create_event(term, TERM_EVENT_CHANGE);
    }

    st = this_cpu_ptr(term->stats);

    u64_stats_update_begin(&st->syncp);
    st->rx_bytes += rx;
    st->tx_bytes += tx;
    u64_stats_update_end(&st->syncp);
}
EXPORT_SYMBOL(term_update);

static void term_update2(u8 *mac, unsigned int rx, unsigned int tx)
{
    struct terminal *term;
    int i;

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        rcu_read_lock();
        hlist_for_each_entry_rcu(term, &terms[i], hlist) {
            if (ether_addr_equal(term->mac, mac)) {
                struct term_stats *st = this_cpu_ptr(term->stats);
                /* Check if term->stats is valid to avoid RCU race condition */
                if (!term->stats) {
                    rcu_read_unlock();
                    return;
                }
                term->updated = jiffies;
                u64_stats_update_begin(&st->syncp);
                st->rx_bytes += rx;
                st->tx_bytes += tx;
                u64_stats_update_end(&st->syncp);
                rcu_read_unlock();
                return;
            }
        }
        rcu_read_unlock();
    }
}

static void term_clean_traffic(void)
{
    struct terminal *term;
    int i;

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        rcu_read_lock();
        hlist_for_each_entry_rcu(term, &terms[i], hlist) {
            int j;
            /* Check if term->stats is valid to avoid RCU race condition */
            if (!term->stats)
                continue;
            term->updated = jiffies;

            for_each_possible_cpu(j) {
                struct term_stats *st = per_cpu_ptr(term->stats, j);
                u64_stats_update_begin(&st->syncp);
                st->tx_bytes = 0;
                st->rx_bytes = 0;
                u64_stats_update_end(&st->syncp);
            }
        }
        rcu_read_unlock();
    }
}

static void term_flush(void)
{
    struct terminal *term = NULL;
    struct hlist_node *n;
    int i;

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        spin_lock_bh(&hash_lock);
        hlist_for_each_entry_safe(term, n, &terms[i], hlist) {
            term_delete(term);
        }
        spin_unlock_bh(&hash_lock);
    }
}

void flush_term_by_dev(struct net_device *dev)
{
    struct terminal *term = NULL;
    struct hlist_node *n;
    int i;

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        spin_lock_bh(&hash_lock);
        hlist_for_each_entry_safe(term, n, &terms[i], hlist) {
            if (dev == term->dev)
                term_delete(term);
        }
        spin_unlock_bh(&hash_lock);
    }
}

static int proc_show(struct seq_file *s, void *v)
{
    int i;
    struct terminal *term;
    struct term_ipv6 *node = NULL;

    seq_printf(s, "%-17s  %-16s %-16s  %-16s  %-16s  %s\n", "MAC", "IP",  "Tx(Byte)", "Rx(Byte)", "Device", "IPv6");

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        rcu_read_lock();
        hlist_for_each_entry_rcu(term, &terms[i], hlist) {
            u64 tx_bytes_sum = 0, rx_bytes_sum = 0;
            int j;
            /* Check if term->stats is valid to avoid RCU race condition */
            if (!term->stats)
                continue;
            for_each_possible_cpu(j) {
                struct term_stats *st = per_cpu_ptr(term->stats, j);
                u64 tx_bytes, rx_bytes;
                unsigned int start;

                do {
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
                    start = u64_stats_fetch_begin_irq(&st->syncp);
#else
                    start = u64_stats_fetch_begin(&st->syncp);
#endif
                    tx_bytes = st->tx_bytes;
                    rx_bytes = st->rx_bytes;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
                } while (u64_stats_fetch_retry_irq(&st->syncp, start));
#else
                }
                while (u64_stats_fetch_retry(&st->syncp, start));
#endif

                tx_bytes_sum += tx_bytes;
                rx_bytes_sum += rx_bytes;
            }

            seq_printf(s, "%pM  %-16pI4  %-16llu  %-16llu  %-16s ",
                       term->mac, &term->ip, tx_bytes_sum, rx_bytes_sum, netdev_name(term->dev));

            list_for_each_entry_rcu(node, &term->ipv6_list, list_node) {
                if (node->list_node.next == &term->ipv6_list)
                    seq_printf(s, "%pI6", &node->ipv6);
                else
                    seq_printf(s, "%pI6,", &node->ipv6);
            }
            seq_printf(s, "\n");
        }
        rcu_read_unlock();
    }

    return 0;
}

static ssize_t proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char data[128] = "";
    char *e;

    if (size > sizeof(data) - 1)
        return -EINVAL;

    if (copy_from_user(data, buf, size))
        return -EFAULT;

    e = strchr(data, '\n');
    if (e)
        *e = 0;

    if (data[0] == 'c') {
        term_flush();
        return size;
    }

    /* clean all traffic*/
    if (data[0] == 't') {
        term_clean_traffic();
        return size;
    }

    /* update traffic */
    if (data[0] == 'u') {
        u8 mac[ETH_ALEN];
        u32 rx, tx;

        if (sscanf(data + 2, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u %u",
                   &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5], &rx, &tx) != 8)
            return -EINVAL;

        term_update2(mac, rx, tx);
    }

    return size;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
static const struct file_operations gl_proc_ops = {
    .owner      = THIS_MODULE,
    .open       = proc_open,
    .read       = seq_read,
    .write      = proc_write,
    .llseek     = seq_lseek,
    .release    = single_release
};
#else
static const struct proc_ops gl_proc_ops = {
    .proc_open       = proc_open,
    .proc_read       = seq_read,
    .proc_write      = proc_write,
    .proc_lseek     = seq_lseek,
    .proc_release    = single_release
};
#endif

static unsigned short calc_icmp6_cksum(struct nd_msg *ndm, struct ipv6hdr *ip6h)
{
    unsigned int tmp = 0, cin = 0;
    unsigned short ret = 0;
    int i;
    u16 *p = (u16 *)&ip6h->saddr;
    u16 *q = (u16 *)&ip6h->daddr;

    for (i = 0; i < 8; i += 2) {
        tmp += ntohs(*(p + i)) + ntohs(*(p + i + 1));
        tmp += ntohs(*(q + i)) + ntohs(*(q + i + 1));
        cin = tmp >> 16;
        if (cin) {
            tmp &= 0xffff;
            tmp += cin;
            cin = 0;
        }
    }

    tmp += 0x3a; //next header
    tmp += ntohs(ip6h->payload_len);

    cin = tmp >> 16;
    if (cin) {
        tmp &= 0xffff;
        tmp += cin;
        cin = 0;
    }

    p = (u16 *)ndm;

    for (i = 0; i < ntohs(ip6h->payload_len) / 2; i += 2) {
        tmp += ntohs(*(p + i)) + ntohs(*(p + i + 1));
        cin = tmp >> 16;
        if (cin) {
            tmp &= 0xffff;
            tmp += cin;
            cin = 0;
        }
    }
    ret = (unsigned short)(~(tmp & 0xffff));
    return ret;
}

int send_ns(struct net_device *dev, const struct in6_addr *target_addr)
{
    int err = -1;
    int hlen, tlen;
    struct sk_buff *skb;
    struct ipv6hdr *ip6h;
    struct ethhdr *eth_header;
    struct in6_addr sip;
    struct nd_msg *ndm;
    struct neighbour *n;
    struct {
        char type;
        char len;
        char mac[ETH_ALEN];
    } icmpv6_opt;

    if (ipv6_dev_get_saddr(&init_net, dev, target_addr, IPV6_PREFER_SRC_PUBTMP_DEFAULT, &sip)) {
        return err;
    }

    hlen = LL_RESERVED_SPACE(dev);
    tlen = dev->needed_tailroom;

    skb = dev_alloc_skb(sizeof(struct ipv6hdr) + sizeof(struct nd_msg) + ETH_ALEN + 2 + hlen + tlen);
    if (!skb) {
        return err;
    }

    skb->dev = dev;
    skb_reserve(skb, hlen);
    skb->protocol = htons(ETH_P_IPV6);

    ip6h = (struct ipv6hdr *)skb_put(skb, sizeof(struct ipv6hdr));
    memset(ip6h, 0, sizeof(struct ipv6hdr));
    ip6h->version = 6;
    ip6h->payload_len = htons(sizeof(struct nd_msg) + ETH_ALEN + 2);
    ip6h->nexthdr = IPPROTO_ICMPV6;
    ip6h->hop_limit = 255;
    ip6h->saddr = sip;
    ip6h->daddr = *target_addr;

    ndm = (struct nd_msg *)skb_put(skb, sizeof(struct nd_msg) + ETH_ALEN + 2);
    memset(ndm, 0, sizeof(struct nd_msg));
    ndm->icmph.icmp6_code = 0;
    ndm->icmph.icmp6_type = NDISC_NEIGHBOUR_SOLICITATION;
    memcpy(&ndm->target, target_addr, sizeof(struct in6_addr));
    memset(&icmpv6_opt, 0, sizeof(icmpv6_opt));
    memcpy(icmpv6_opt.mac, dev->dev_addr, ETH_ALEN);
    icmpv6_opt.type = 1;
    icmpv6_opt.len = 1;
    memcpy(&ndm->opt, &icmpv6_opt, sizeof(icmpv6_opt));
    ndm->icmph.icmp6_cksum = htons(calc_icmp6_cksum(ndm, ip6h));

    eth_header = (struct ethhdr *)skb_push(skb, ETH_HLEN);
    eth_header->h_proto = htons(ETH_P_IPV6);
    memcpy(eth_header->h_source, dev->dev_addr, ETH_ALEN);

    n = __ipv6_neigh_lookup_noref(dev, target_addr);
    if (n == NULL) {
        eth_header->h_dest[0] = 0x33;
        eth_header->h_dest[1] = 0x33;
        eth_header->h_dest[2] = 0xff;
        memcpy(&eth_header->h_dest[3], &target_addr->s6_addr[13], 3);
    } else {
        memcpy(eth_header->h_dest, n->ha, ETH_ALEN);
    }

    //pr_err("sending ns: sip=%pI6, tip=%pI6, eth_header->h_dest=%pM, eth_header->h_source=%pM\n\n", &sip, target_addr, eth_header->h_dest, eth_header->h_source);
    err = dev_queue_xmit(skb);
    if (err < 0) {
        kfree_skb(skb);
        return err;
    }

    return 0;
}

static void term_keepalive(struct work_struct *work)
{
    struct terminal *term = NULL;
    int i;

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        rcu_read_lock();
        hlist_for_each_entry_rcu(term, &terms[i], hlist) {
            struct net_device *dev = term->dev;
            struct net_device *br_dev;
            struct in_device *indev;

            br_dev = netdev_master_upper_dev_get_rcu(term->dev);
            if (br_dev)
                dev = br_dev;

            indev = __in_dev_get_rcu(dev);
            if (indev) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
                for_primary_ifa(indev) {
#else
                const struct in_ifaddr *ifa;
                in_dev_for_each_ifa_rcu(ifa, indev) {
#endif
                    arp_send(ARPOP_REQUEST, ETH_P_ARP, term->ip, dev, ifa->ifa_address, term->mac, dev->dev_addr, NULL);
                }
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
                endfor_ifa(indev);
#endif
            }
        }
        rcu_read_unlock();
    }

    mod_delayed_work(system_long_wq, &keepalive_work,  icmp_ttl);
}

static void term_keepalive_ipv6(struct work_struct *work)
{
    int i;
    struct terminal *term = NULL;
    struct term_ipv6 *node = NULL;

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        rcu_read_lock();
        hlist_for_each_entry_rcu(term, &terms[i], hlist) {
            struct net_device *dev = term->dev;
            struct net_device *br_dev;

            br_dev = netdev_master_upper_dev_get_rcu(term->dev);
            if (br_dev)
                dev = br_dev;
            list_for_each_entry_rcu(node, &term->ipv6_list, list_node) {
                send_ns(dev, &node->ipv6);
            }
        }
        rcu_read_unlock();
    }

    mod_delayed_work(system_long_wq, &keepalive_work_ipv6,  icmp_ttl);

    return;
}

static void term_cleanup(struct work_struct *work)
{
    unsigned long work_delay = ttl;
    struct terminal *term = NULL;
    struct hlist_node *n;
    int i;

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        spin_lock_bh(&hash_lock);
        hlist_for_each_entry_safe(term, n, &terms[i], hlist) {
            if (time_after(jiffies, term->updated + ttl))
                if (list_empty(&term->ipv6_list))
                    term_delete(term);
                else
                    memset(&term->ip, 0x0, sizeof(term->ip));
            else
                work_delay = min(work_delay, term->updated + ttl - jiffies);
        }
        spin_unlock_bh(&hash_lock);
    }

    /* Cleanup minimum 10 milliseconds apart */
    work_delay = max_t(unsigned long, work_delay, msecs_to_jiffies(10));
    mod_delayed_work(system_long_wq, &gc_work, work_delay);
}

static void term_cleanup_ipv6(struct work_struct *work)
{
    int i;
    unsigned long work_delay = ttl;
    struct terminal *term = NULL;
    struct hlist_node *n;
    struct term_ipv6 *next;
    struct term_ipv6 *node = NULL;
    struct term_ipv6 *to_free;
    LIST_HEAD(free_list);

    for (i = 0; i < TERM_HASH_SIZE; i++) {
        spin_lock_bh(&hash_lock);
        hlist_for_each_entry_safe(term, n, &terms[i], hlist) {
            list_for_each_entry_safe(node, next, &term->ipv6_list, list_node) {
                if (time_after(jiffies, node->updated + ttl)) {
                    spin_lock_bh(&term->ipv6_lock);
                    list_del_init(&node->list_node);
                    list_add(&node->list_node, &free_list);
                    spin_unlock_bh(&term->ipv6_lock);
                } else {
                    work_delay = min(work_delay, node->updated + ttl - jiffies);
                }
            }
            if (time_after(jiffies, term->updated + ttl)) {
                bool empty;
                spin_lock_bh(&term->ipv6_lock);
                empty = list_empty(&term->ipv6_list);
                spin_unlock_bh(&term->ipv6_lock);
                if (empty)
                    term_delete(term);
            }
        }
        spin_unlock_bh(&hash_lock);
    }

    /* wait for RCU grace period to avoid race condition */
    if (!list_empty(&free_list)) {
        synchronize_rcu();
        list_for_each_entry_safe(to_free, next, &free_list, list_node) {
            list_del(&to_free->list_node);
            kmem_cache_free(term_ipv6_cache, to_free);
        }
    }

    /* Cleanup minimum 10 milliseconds apart */
    work_delay = max_t(unsigned long, work_delay, msecs_to_jiffies(10));
    mod_delayed_work(system_long_wq, &gc_work_ipv6, work_delay);
}

void set_term_ttl(unsigned long t)
{
    if (t > 0) {
        icmp_ttl = t * HZ / 3;//每个TTL周期内发送3个心跳包，如果丢包率高可以调整
        ttl = t * HZ;
    }

    mod_delayed_work(system_long_wq, &keepalive_work, 0);
    mod_delayed_work(system_long_wq, &gc_work, 0);
    mod_delayed_work(system_long_wq, &keepalive_work_ipv6, 0);
    mod_delayed_work(system_long_wq, &gc_work_ipv6, 0);
}

unsigned long get_term_ttl(void)
{
    return ttl / HZ;
}

int term_init(struct proc_dir_entry *proc)
{
    int ret;

    term_cache = kmem_cache_create("term_cache", sizeof(struct terminal), 0, 0, NULL);
    if (!term_cache)
        return -ENOMEM;

    term_ipv6_cache = kmem_cache_create("term_ipv6_cache", sizeof(struct term_ipv6), 0, 0, NULL);
    if (!term_ipv6_cache) {
        ret = -ENOMEM;
        goto err;
    }

    proc_create("term", 0644, proc, &gl_proc_ops);

    get_random_bytes(&hash_rnd, sizeof(hash_rnd));

    INIT_DELAYED_WORK(&keepalive_work, term_keepalive);
    INIT_DELAYED_WORK(&gc_work, term_cleanup);
    INIT_DELAYED_WORK(&keepalive_work_ipv6, term_keepalive_ipv6);
    INIT_DELAYED_WORK(&gc_work_ipv6, term_cleanup_ipv6);

    set_term_ttl(60);

    return 0;

err:
    kmem_cache_destroy(term_cache);
    kmem_cache_destroy(term_ipv6_cache);

    return 0;
}

void term_free(void)
{
    cancel_delayed_work_sync(&keepalive_work);
    cancel_delayed_work_sync(&gc_work);
    cancel_delayed_work_sync(&keepalive_work_ipv6);
    cancel_delayed_work_sync(&gc_work_ipv6);

    term_flush();

    rcu_barrier();  /* Wait for completion of call_rcu()'s */

    kmem_cache_destroy(term_cache);
    kmem_cache_destroy(term_ipv6_cache);
}

bool is_in6_addr_zero(struct in6_addr *addr)
{
    struct in6_addr zero_addr;
    memset(&zero_addr, 0, sizeof(struct in6_addr));

    return memcmp(addr, &zero_addr, sizeof(struct in6_addr)) == 0;
}

void term_ipv6_update(const u8 *mac, struct in6_addr *addr, unsigned int rx, unsigned int tx, bool alive)
{
    struct hlist_head *head = &terms[term_mac_hash(mac)];
    struct terminal *term;
    struct term_stats *st;
    struct term_ipv6 *node;
    bool exist = false;

    if (is_in6_addr_zero(addr))
        return;

    term = find_term_rcu(head, mac);
    if (!term)
        return;

    /* Check if term->stats is valid to avoid RCU race condition */
    if (!term->stats)
        return;

    list_for_each_entry_rcu(node, &term->ipv6_list, list_node) {
        if (ipv6_addr_equal(&node->ipv6, addr)) {
            if (alive)
                node->updated = jiffies;
            exist = true;
            break;
        }
    }

    if (!exist) {
        node  = kmem_cache_zalloc(term_ipv6_cache, GFP_ATOMIC);
        if (!node) {
            return;
        }

        node->updated = jiffies;
        memcpy(&node->ipv6, addr, sizeof(struct in6_addr));

        spin_lock_bh(&term->ipv6_lock);
        list_add_tail_rcu(&node->list_node, &term->ipv6_list);
        spin_unlock_bh(&term->ipv6_lock);
    }

    st = this_cpu_ptr(term->stats);

    u64_stats_update_begin(&st->syncp);
    st->rx_bytes += rx;
    st->tx_bytes += tx;
    u64_stats_update_end(&st->syncp);
}
EXPORT_SYMBOL(term_ipv6_update);

void term_ipv6_create(const u8 *mac, struct in6_addr *addr, struct net_device *dev)
{
    int i;
    struct hlist_head *head = &terms[term_mac_hash(mac)];
    struct terminal *term, *old;
    struct term_ipv6 *node;

    if (is_in6_addr_zero(addr))
        return;

    term = find_term_rcu(head, mac);
    if (likely(term && term->dev == dev)) {
        term_ipv6_update(mac, addr, 0, 0, true);
        return;
    }

    term = kmem_cache_zalloc(term_cache, GFP_ATOMIC);
    if (!term)
        return;

    term->stats = alloc_percpu_gfp(struct term_stats, GFP_ATOMIC);
    if (!term->stats) {
        kmem_cache_free(term_cache, term);
        return;
    }

    node  = kmem_cache_zalloc(term_ipv6_cache, GFP_ATOMIC);
    if (!node) {
        free_percpu(term->stats);
        kmem_cache_free(term_cache, term);
        return;
    }

    for_each_possible_cpu(i) {
        struct term_stats *st = per_cpu_ptr(term->stats, i);
        u64_stats_init(&st->syncp);
    }

    dev_hold(dev);

    term->dev = dev;
    memcpy(term->mac, mac, ETH_ALEN);

    spin_lock_init(&term->ipv6_lock);

    INIT_LIST_HEAD(&term->ipv6_list);
    node->updated = jiffies;
    memcpy(&node->ipv6, addr, sizeof(struct in6_addr));
    list_add_tail(&node->list_node, &term->ipv6_list);

    spin_lock(&hash_lock);
    old = find_term(head, mac);
    if (unlikely(old)) {
        hlist_replace_rcu(&old->hlist, &term->hlist);
        call_rcu(&old->rcu, term_rcu_free);
    } else {
        hlist_add_head_rcu(&term->hlist, head);
    }
    spin_unlock(&hash_lock);
}
EXPORT_SYMBOL(term_ipv6_create);
