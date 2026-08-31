#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/rwlock.h>
#include <linux/netdevice.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_nat.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <net/netfilter/nf_nat_masquerade.h>
#include <linux/netdevice.h>
#include <linux/rcupdate.h>
#include <net/netfilter/nf_nat_helper.h>
#include <linux/ip.h>
#include "port_forward.h"

static LIST_HEAD(active_rules);
static LIST_HEAD(staging_rules);
static DEFINE_RWLOCK(pf_rule_lock);
static struct proc_dir_entry *pf_proc;
static struct proc_dir_entry *pf_rules_proc;

static struct nf_hook_ops pf_dnat_ops;
static struct nf_hook_ops pf_snat_ops;

#define MAX_SVC_PORTS 16

static u16 svc_tcp_ports[MAX_SVC_PORTS];
static u16 svc_udp_ports[MAX_SVC_PORTS];

static int svc_tcp_cnt;
static int svc_udp_cnt;

static inline bool pf_is_router_service_port(u8 proto, u16 port)
{
    int i;

    if (proto == IPPROTO_TCP) {
        for (i = 0; i < svc_tcp_cnt; i++) {
            if (svc_tcp_ports[i] == port)
                return true;
        }
    }

    if (proto == IPPROTO_UDP) {
        for (i = 0; i < svc_udp_cnt; i++) {
            if (svc_udp_ports[i] == port)
                return true;
        }
    }

    return false;
}

static const char *proto_to_str(u8 proto)
{
    switch (proto) {
        case PF_PROTO_TCP:
            return "tcp";
        case PF_PROTO_UDP:
            return "udp";
        case PF_PROTO_ALL:
            return "all";
        default:
            return "unknown";
    }
}

static int pf_show_rules(struct seq_file *m, void *v)
{
    struct pf_rule *r;
    int index = 0;

    seq_puts(m, "=== Active Port Forwarding Rules ===\n\n");

    read_lock(&pf_rule_lock);

    if (list_empty(&active_rules)) {
        seq_puts(m, "No active rules configured.\n");
        read_unlock(&pf_rule_lock);
        return 0;
    }

    list_for_each_entry(r, &active_rules, list) {
        index++;

        seq_printf(m, "Rule #%d:\n", index);
        seq_printf(m, "  Protocol: %s\n", proto_to_str(r->proto));
        seq_printf(m, "  Public IP: %pI4\n", &r->match_ip);

        if (r->src_start == 0 && r->src_end == 0) {
            seq_puts(m, "  Public Ports: ALL (DMZ)\n");
        } else if (r->src_start == r->src_end) {
            seq_printf(m, "  Public Ports: %u\n", r->src_start);
        } else {
            seq_printf(m, "  Public Ports: %u-%u\n", r->src_start, r->src_end);
        }

        seq_printf(m, "  Internal IP: %pI4\n", &r->dst_ip);

        if (r->dst_start == 0 && r->dst_end == 0) {
            seq_puts(m, "  Internal Ports: SAME AS PUBLIC\n");
        } else if (r->dst_start == r->dst_end) {
            seq_printf(m, "  Internal Ports: %u\n", r->dst_start);
        } else {
            seq_printf(m, "  Internal Ports: %u-%u\n", r->dst_start, r->dst_end);
        }

        if (r->proto == PF_PROTO_ALL &&
                r->src_start == 0 && r->src_end == 0 &&
                r->dst_start == 0 && r->dst_end == 0) {
            seq_puts(m, "  Type: DMZ (Full Port Forwarding)\n");
        } else if (r->src_start != r->dst_start) {
            seq_puts(m, "  Type: Port Mapping\n");
        } else {
            seq_puts(m, "  Type: Direct Forwarding\n");
        }

        seq_puts(m, "\n");
    }

    read_unlock(&pf_rule_lock);

    return 0;
}

static int pf_rules_open(struct inode *inode, struct file *file)
{
    return single_open(file, pf_show_rules, NULL);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops pf_rules_fops = {
    .proc_open = pf_rules_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#else
static const struct file_operations pf_rules_fops = {
    .owner = THIS_MODULE,
    .open = pf_rules_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#endif


static void pf_free_rules(struct list_head *head)
{
    struct pf_rule *r, *tmp;
    list_for_each_entry_safe(r, tmp, head, list) {
        list_del(&r->list);
        kfree(r);
    }
}

static u16 pf_map_port(struct pf_rule *r, u16 dport)
{
    u16 len = r->dst_end - r->dst_start + 1;
    u16 offset = dport - r->src_start;
    if (len <= 1)
        return r->dst_start;
    return (offset < len) ? (r->dst_start + offset) : r->dst_start;
}

static inline void pf_set_mark(struct nf_conn *ct)
{
    u32 mark;

    spin_lock_bh(&ct->lock);
    mark = ct->mark;
    ct->mark = mark | PF_NAT_MARK;
    spin_unlock_bh(&ct->lock);
}

static unsigned int pf_register_dnat(struct sk_buff *skb,
                                     struct pf_rule *r,
                                     u16 new_port)
{
    enum ip_conntrack_info ctinfo;
    struct nf_conn *ct;
    struct nf_nat_range2 range;
    u16 orig_dport = 0;
    u16 target_port = 0;
    unsigned int ret = 0;
    struct iphdr *iph;

    ct = nf_ct_get(skb, &ctinfo);
    if (!ct)
        return NF_ACCEPT;

    pf_set_mark(ct);

    if (nf_nat_initialized(ct, NF_NAT_MANIP_DST))
        return NF_ACCEPT;

    if (ctinfo != IP_CT_NEW)
        return NF_ACCEPT;

    iph = ip_hdr(skb);
    if (iph->protocol == IPPROTO_TCP)
        orig_dport = ntohs(tcp_hdr(skb)->dest);
    else if (iph->protocol == IPPROTO_UDP)
        orig_dport = ntohs(udp_hdr(skb)->dest);

    target_port = new_port ? new_port : orig_dport;

    memset(&range, 0, sizeof(range));
    range.flags |= NF_NAT_RANGE_MAP_IPS;
    range.min_addr.ip = r->dst_ip;
    range.max_addr.ip = r->dst_ip;
    range.flags |= NF_NAT_RANGE_PROTO_SPECIFIED;
    range.min_proto.all = htons(target_port);
    range.max_proto.all = htons(target_port);

    ret = nf_nat_setup_info(ct, &range, NF_NAT_MANIP_DST);

    return ret;
}

static unsigned int pf_dnat_hook(void *priv,
                                 struct sk_buff *skb,
                                 const struct nf_hook_state *state)
{
    struct iphdr *iph;
    struct pf_rule *r;
    u16 dport = 0;
    bool is_l4 = false;
    bool is_dmz = false;
    enum ip_conntrack_info ctinfo;
    struct nf_conn *ct;

    iph = ip_hdr(skb);

    if (!skb || !state || !state->in || !iph || iph->version != 4)
        return NF_ACCEPT;

    ct = nf_ct_get(skb, &ctinfo);
    if (!ct)
        return NF_ACCEPT;

    if (ctinfo != IP_CT_NEW)
        return NF_ACCEPT;

    if (iph->protocol == IPPROTO_TCP) {
        if (!pskb_may_pull(skb, iph->ihl * 4 + sizeof(struct tcphdr)))
            return NF_ACCEPT;
        dport = ntohs(tcp_hdr(skb)->dest);
        is_l4 = true;
    } else if (iph->protocol == IPPROTO_UDP) {
        if (!pskb_may_pull(skb, iph->ihl * 4 + sizeof(struct udphdr)))
            return NF_ACCEPT;
        dport = ntohs(udp_hdr(skb)->dest);
        is_l4 = true;
    }

    if (is_l4 && pf_is_router_service_port(iph->protocol, dport))
        return NF_ACCEPT;

    read_lock(&pf_rule_lock);
    list_for_each_entry(r, &active_rules, list) {
        if (iph->daddr != r->match_ip)
            continue;

        is_dmz = (r->proto == PF_PROTO_ALL &&
                  r->src_start == 0 && r->src_end == 0 &&
                  r->dst_start == 0 && r->dst_end == 0);
        if (is_dmz) {
            pf_register_dnat(skb, r, 0);
            read_unlock(&pf_rule_lock);
            return NF_ACCEPT;
        }

        if (r->proto != PF_PROTO_ALL) {
            if ((r->proto == PF_PROTO_TCP && iph->protocol != IPPROTO_TCP) ||
                    (r->proto == PF_PROTO_UDP && iph->protocol != IPPROTO_UDP))
                continue;
        }

        if (!is_l4)
            continue;

        if (dport < r->src_start || dport > r->src_end)
            continue;

        pf_register_dnat(skb, r, pf_map_port(r, dport));
        read_unlock(&pf_rule_lock);
        return NF_ACCEPT;
    }
    read_unlock(&pf_rule_lock);

    return NF_ACCEPT;
}

static unsigned int pf_snat_hook(void *priv,
                                 struct sk_buff *skb,
                                 const struct nf_hook_state *state)
{
    struct nf_conn *ct;
    enum ip_conntrack_info ctinfo;
    struct nf_nat_range2 range;
    __be32 saddr, daddr, snat_ip = 0;

    if (skb->protocol != htons(ETH_P_IP))
        return NF_ACCEPT;

    ct = nf_ct_get(skb, &ctinfo);
    if (!ct || ctinfo != IP_CT_NEW)
        return NF_ACCEPT;

    if (!(ct->mark & PF_NAT_MARK))
        return NF_ACCEPT;

    if (nf_nat_initialized(ct, NF_NAT_MANIP_SRC))
        return NF_ACCEPT;

    saddr = ip_hdr(skb)->saddr;

    daddr = ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.ip;

    if (pf_lan_zone.valid &&
            (saddr & pf_lan_zone.mask) == pf_lan_zone.net &&
            (daddr & pf_lan_zone.mask) == pf_lan_zone.net) {
        snat_ip = pf_lan_zone.gw;
    } else if (pf_guest_zone.valid &&
               (saddr & pf_guest_zone.mask) == pf_guest_zone.net &&
               (daddr & pf_guest_zone.mask) == pf_guest_zone.net) {
        snat_ip = pf_guest_zone.gw;
    }

    if (!snat_ip)
        return NF_ACCEPT;

    memset(&range, 0, sizeof(range));
    range.flags = NF_NAT_RANGE_MAP_IPS;
    range.min_addr.ip = snat_ip;
    range.max_addr.ip = snat_ip;

    nf_nat_setup_info(ct, &range, NF_NAT_MANIP_SRC);
    return NF_ACCEPT;
}

static void pf_proc_handle_line(const char *kbuf)
{
    struct pf_rule *r;

    char proto[8];
    u16 src_s, src_e, dst_s, dst_e;
    u8 match_ip[4];
    u8 dst_ip[4];

    char cmd[8];
    char zone_name[8];
    u8 net[4], mask[4], gw[4];
    struct pf_zone *z;

    if (sscanf(kbuf, "%7s", cmd) == 1) {

        if (!strcmp(cmd, "zone")) {

            if (sscanf(kbuf,
                       "zone %7s "
                       "%hhu.%hhu.%hhu.%hhu "
                       "%hhu.%hhu.%hhu.%hhu "
                       "%hhu.%hhu.%hhu.%hhu",
                       zone_name,
                       &net[0], &net[1], &net[2], &net[3],
                       &mask[0], &mask[1], &mask[2], &mask[3],
                       &gw[0], &gw[1], &gw[2], &gw[3]) == 13) {

                if (!strcmp(zone_name, "lan"))
                    z = &pf_lan_zone;
                else if (!strcmp(zone_name, "guest"))
                    z = &pf_guest_zone;
                else
                    return;

                z->net  = htonl((net[0]  << 24) |
                                (net[1]  << 16) |
                                (net[2]  << 8)  |
                                net[3]);
                z->mask = htonl((mask[0] << 24) |
                                (mask[1] << 16) |
                                (mask[2] << 8)  |
                                mask[3]);
                z->gw   = htonl((gw[0]   << 24) |
                                (gw[1]   << 16) |
                                (gw[2]   << 8)  |
                                gw[3]);
                z->valid = true;
            }
            return;
        }
    }

    if (!strncmp(kbuf, "svcport", 7)) {
        char proto[8];
        u16 port;

        if (sscanf(kbuf, "svcport %7s %hu", proto, &port) != 2)
            return;

        if (!strcmp(proto, "tcp")) {
            if (svc_tcp_cnt < MAX_SVC_PORTS)
                svc_tcp_ports[svc_tcp_cnt++] = port;
        } else if (!strcmp(proto, "udp")) {
            if (svc_udp_cnt < MAX_SVC_PORTS)
                svc_udp_ports[svc_udp_cnt++] = port;
        }
        return;
    }

    if (!strncmp(kbuf, "clear", 5)) {
        svc_tcp_cnt = 0;
        svc_udp_cnt = 0;
        write_lock(&pf_rule_lock);
        pf_free_rules(&staging_rules);
        write_unlock(&pf_rule_lock);
        return;
    }

    if (!strncmp(kbuf, "commit", 6)) {
        LIST_HEAD(old);

        write_lock(&pf_rule_lock);
        list_replace_init(&active_rules, &old);
        list_replace_init(&staging_rules, &active_rules);
        write_unlock(&pf_rule_lock);

        pf_free_rules(&old);
        return;
    }

    r = kzalloc(sizeof(*r), GFP_KERNEL);
    if (!r)
        return;

    if (sscanf(kbuf,
               "add %7s %hhu.%hhu.%hhu.%hhu %hu %hu %hhu.%hhu.%hhu.%hhu %hu %hu",
               proto,
               &match_ip[0], &match_ip[1], &match_ip[2], &match_ip[3],
               &src_s, &src_e,
               &dst_ip[0], &dst_ip[1], &dst_ip[2], &dst_ip[3],
               &dst_s, &dst_e) != 13) {
        kfree(r);
        return;
    }

    if (!strcmp(proto, "tcp"))
        r->proto = PF_PROTO_TCP;
    else if (!strcmp(proto, "udp"))
        r->proto = PF_PROTO_UDP;
    else
        r->proto = PF_PROTO_ALL;

    r->match_ip = htonl((match_ip[0] << 24) |
                        (match_ip[1] << 16) |
                        (match_ip[2] << 8)  |
                        match_ip[3]);

    r->dst_ip = htonl((dst_ip[0] << 24) |
                      (dst_ip[1] << 16) |
                      (dst_ip[2] << 8)  |
                      dst_ip[3]);

    r->src_start = src_s;
    r->src_end   = src_e;
    r->dst_start = dst_s;
    r->dst_end   = dst_e;

    write_lock(&pf_rule_lock);
    list_add_tail(&r->list, &staging_rules);
    write_unlock(&pf_rule_lock);

    return;
}

static ssize_t pf_proc_write(struct file *file,
                             const char __user *buffer,
                             size_t len, loff_t *ppos)
{
    char kbuf[BUF_SIZE];
    char *cur, *line;
    if (len >= BUF_SIZE)
        len = BUF_SIZE - 1;

    if (copy_from_user(kbuf, buffer, len))
        return -EFAULT;

    kbuf[len] = '\0';
    cur = kbuf;

    while ((line = strsep(&cur, "\n")) != NULL) {
        if (*line == '\0')
            continue;
        pf_proc_handle_line(line);
    }

    return len;

}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops pf_proc_ops = {
    .proc_write = pf_proc_write,
};
#else
static const struct file_operations pf_proc_ops = {
    .write = pf_proc_write,
};
#endif

static int __init pf_init(void)
{
    pf_dnat_ops.hook = pf_dnat_hook;
    pf_dnat_ops.pf = NFPROTO_IPV4;
    pf_dnat_ops.hooknum = NF_INET_PRE_ROUTING;
    pf_dnat_ops.priority = NF_IP_PRI_NAT_DST;

    pf_snat_ops.hook = pf_snat_hook;
    pf_snat_ops.pf = NFPROTO_IPV4;
    pf_snat_ops.hooknum = NF_INET_POST_ROUTING;
    pf_snat_ops.priority = NF_IP_PRI_NAT_SRC;

    if (nf_register_net_hook(&init_net, &pf_dnat_ops)) {
        return -EINVAL;
    }

    if (nf_register_net_hook(&init_net, &pf_snat_ops)) {
        nf_unregister_net_hook(&init_net, &pf_snat_ops);
        return -EINVAL;
    }

    pf_proc = proc_create(PROC_NAME, 0666, NULL, &pf_proc_ops);
    if (!pf_proc) {
        nf_unregister_net_hook(&init_net, &pf_dnat_ops);
        nf_unregister_net_hook(&init_net, &pf_snat_ops);
        return -EINVAL;
    }

    pf_rules_proc = proc_create("port_forward_rules", 0444, NULL, &pf_rules_fops);
    if (!pf_rules_proc) {
        remove_proc_entry(PROC_NAME, NULL);
        nf_unregister_net_hook(&init_net, &pf_dnat_ops);
        nf_unregister_net_hook(&init_net, &pf_snat_ops);
        pr_err("Failed to create port_forward_rules proc entry\n");
        return -EINVAL;
    }
    return 0;
}

static void __exit pf_exit(void)
{
    remove_proc_entry("port_forward_rules", NULL);
    remove_proc_entry(PROC_NAME, NULL);
    nf_unregister_net_hook(&init_net, &pf_dnat_ops);
    nf_unregister_net_hook(&init_net, &pf_snat_ops);

    write_lock(&pf_rule_lock);
    pf_free_rules(&active_rules);
    pf_free_rules(&staging_rules);
    write_unlock(&pf_rule_lock);
}

module_init(pf_init);
module_exit(pf_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("pan.wei@gl-inet.com");
MODULE_DESCRIPTION("Advanced Port Forward Kernel Module");
MODULE_VERSION("1.1");

