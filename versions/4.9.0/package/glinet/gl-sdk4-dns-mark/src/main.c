#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <net/netfilter/nf_conntrack.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/if_ether.h>
#include <linux/version.h>
#include <linux/ratelimit.h>
#include <linux/rtnetlink.h>
#include <linux/ipv6.h>
#include <net/ip.h>
#include <net/route.h>
#include <net/addrconf.h>
#include <linux/rcupdate.h>
#include "dns_mark.h"
#include "proc.h"

struct dns_mark_rule_mgr rule_mgr;  /* Global rule manager */
#if defined(CONFIG_JUMP_LABEL)
DEFINE_STATIC_KEY_FALSE(dns_mark_debug_key);
DEFINE_STATIC_KEY_TRUE(dns_mark_intercept_forward_key);
#else
atomic_t dns_mark_debug_flag = ATOMIC_INIT(0);
atomic_t dns_mark_intercept_forward_flag = ATOMIC_INIT(1);
#endif

static unsigned int handle_tcp_dns(struct sk_buff *skb, struct dns_query_info *info,
                                   int (*parse_dns)(struct sk_buff *, struct dns_query_info *))
{
    struct tcphdr *tcph;
    struct nf_conn *ct;
    enum ip_conntrack_info ctinfo;
    int ret;
    u32 mark;
    int transport_offset, header_offset, payload_len;

    tcph = tcp_hdr(skb);
    if (!tcph || !dns_mark_port_is_dns(ntohs(tcph->dest)))
        return NF_ACCEPT;

    /* 只处理已建立的TCP连接 */
    ct = nf_ct_get(skb, &ctinfo);
    if (!ct || !(ctinfo == IP_CT_ESTABLISHED ||
                 ctinfo == IP_CT_ESTABLISHED_REPLY)) {
        return NF_ACCEPT;
    }

    transport_offset = skb_transport_offset(skb);
    header_offset = transport_offset + tcph->doff * 4;
    payload_len = skb->len - header_offset;
    DNS_MARK_DBG_RL("dns-mark: tcp pre-parse payload_len=%d flags S=%d F=%d R=%d A=%d\n",
                    payload_len, tcph->syn, tcph->fin, tcph->rst, tcph->ack);

    /* Parse DNS payload only if there exists any domain rule */
    info->domain[0] = '\0';
    info->domain_len = 0;
    if (dns_mark_need_domain_parse()) {
        ret = parse_dns(skb, info);
        if (ret < 0) {
            DNS_MARK_DBG_RL("dns-mark: tcp parse failed ret=%d (possible split/partial)\n", ret);
            return NF_ACCEPT;
        }
        DNS_MARK_DBG_RL("dns-mark: tcp parsed domain='%s' len=%zu\n", info->domain, info->domain_len);
    } else {
        /* No domain rules configured, skip parsing to save cycles */
        DNS_MARK_DBG_RL("dns-mark: tcp skip parse (no domain rules)\n");
    }
    mark = dns_mark_apply_rules(skb, info);
    if (mark && CTINFO2DIR(ctinfo) == IP_CT_DIR_ORIGINAL) {
        ct->mark = mark;
        DNS_MARK_DBG_RL("dns-mark: tcp set ct mark=0x%x (ORIG)\n", mark);
    } else if (mark) {
        DNS_MARK_DBG_RL("dns-mark: tcp got mark=0x%x but not ORIG dir (skip set)\n", mark);
    } else {
        DNS_MARK_DBG_RL("dns-mark: tcp no rule matched (mark=0)\n");
    }

    return NF_ACCEPT;
}

static unsigned int handle_udp_dns(struct sk_buff *skb, struct dns_query_info *info,
                                   int (*parse_dns)(struct sk_buff *, struct dns_query_info *))
{
    struct udphdr *udph;
    int ret;
    u32 mark;

    udph = udp_hdr(skb);
    if (!udph || ntohs(udph->dest) != 53)
        return NF_ACCEPT;

    /* Parse DNS payload only if there exists any domain rule */
    info->domain[0] = '\0';
    info->domain_len = 0;
    if (dns_mark_need_domain_parse()) {
        ret = parse_dns(skb, info);
        if (ret < 0)
            return NF_ACCEPT;
    } else {
        DNS_MARK_DBG_RL("dns-mark: udp skip parse (no domain rules)\n");
    }

    mark = dns_mark_apply_rules(skb, info);
    if (mark) {
        skb->mark = mark;
    }

    return NF_ACCEPT;
}

/* Netfilter hook function for TCP DNS */
static unsigned int dns_mark_hook_tcp(void *priv,
                                      struct sk_buff *skb,
                                      const struct nf_hook_state *state)
{
    struct dns_query_info query_info;
    int (*parse_dns)(struct sk_buff *, struct dns_query_info *);

    if (state->pf == NFPROTO_IPV4) {
        struct iphdr *iph = ip_hdr(skb);
        if (!iph || iph->protocol != IPPROTO_TCP)
            return NF_ACCEPT;
        parse_dns = dns_mark_parse_query;
    } else if (state->pf == NFPROTO_IPV6) {
        struct ipv6hdr *ip6h = ipv6_hdr(skb);
        if (!ip6h || ip6h->nexthdr != IPPROTO_TCP)
            return NF_ACCEPT;
        parse_dns = dns_mark_parse_query6;
    } else {
        return NF_ACCEPT;
    }

    return handle_tcp_dns(skb, &query_info, parse_dns);
}

/* Netfilter hook function for UDP DNS */
static unsigned int dns_mark_hook_udp(void *priv,
                                      struct sk_buff *skb,
                                      const struct nf_hook_state *state)
{
    struct dns_query_info query_info;
    int (*parse_dns)(struct sk_buff *, struct dns_query_info *);

    if (state->pf == NFPROTO_IPV4) {
        struct iphdr *iph = ip_hdr(skb);
        bool intercept_forward = dns_mark_intercept_forward_enabled();
        if (!iph || iph->protocol != IPPROTO_UDP)
            return NF_ACCEPT;
        /* 若未启用拦截转发，则仅处理发往本机的目的地址（等效 -m addrtype --dst-type LOCAL） */
        if (!intercept_forward) {
            u8 atype;
            rcu_read_lock();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
            atype = inet_addr_type_table(dev_net(state->in), iph->daddr, RT_TABLE_LOCAL);
#else
            atype = inet_addr_type(iph->daddr);
#endif
            rcu_read_unlock();
            if (atype != RTN_LOCAL)
                return NF_ACCEPT;
        }
        parse_dns = dns_mark_parse_query;
    } else if (state->pf == NFPROTO_IPV6) {
        struct ipv6hdr *ip6h = ipv6_hdr(skb);
        if (!ip6h || ip6h->nexthdr != IPPROTO_UDP)
            return NF_ACCEPT;
        /* 若未启用拦截转发，则仅处理发往本机的目的地址 */
        if (!dns_mark_intercept_forward_enabled()) {
            rcu_read_lock();
            if (!ipv6_chk_addr(dev_net(state->in), &ip6h->daddr, NULL, 0)) {
                rcu_read_unlock();
                return NF_ACCEPT;
            }
            rcu_read_unlock();
        }
        parse_dns = dns_mark_parse_query6;
    } else {
        return NF_ACCEPT;
    }

    return handle_udp_dns(skb, &query_info, parse_dns);
}

static struct nf_hook_ops nf_dns_mark_ops[] __read_mostly = {
    {
        .hook = dns_mark_hook_tcp,
        .pf = NFPROTO_IPV4,
        .hooknum = NF_INET_LOCAL_IN,
        .priority = NF_IP_PRI_CONNTRACK + 1,  /* TCP在连接跟踪之后才能拿到ct */
    },
    {
        .hook = dns_mark_hook_tcp,
        .pf = NFPROTO_IPV6,
        .hooknum = NF_INET_LOCAL_IN,
        .priority = NF_IP_PRI_CONNTRACK + 1,
    },
    {
        .hook = dns_mark_hook_udp,
        .pf = NFPROTO_IPV4,
        .hooknum = NF_INET_PRE_ROUTING,
        .priority = NF_IP_PRI_FIRST,         /* UDP在最前面，配合iptables raw表CT zone规则*/
    },
    {
        .hook = dns_mark_hook_udp,
        .pf = NFPROTO_IPV6,
        .hooknum = NF_INET_PRE_ROUTING,
        .priority = NF_IP_PRI_FIRST,
    },
};

static int __init dns_mark_init(void)
{
    int ret;

    /* Initialize rule manager */
    INIT_LIST_HEAD(&rule_mgr.rules);
    spin_lock_init(&rule_mgr.lock);
    atomic_set(&rule_mgr.rule_count, 0);
    atomic_set(&rule_mgr.domain_rules_present, 0);

    /* Initialize proc filesystem */
    ret = dns_mark_proc_init();
    if (ret < 0)
        return ret;

    /* Initialize netfilter hook */
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 12, 14)
    ret = nf_register_net_hooks(&init_net, nf_dns_mark_ops, ARRAY_SIZE(nf_dns_mark_ops));
#else
    ret = nf_register_hooks(nf_dns_mark_ops, ARRAY_SIZE(nf_dns_mark_ops));
#endif
    if (ret < 0) {
        dns_mark_proc_cleanup();
        return ret;
    }

    printk(KERN_INFO "DNS Mark module loaded\n");
    return 0;
}

static void __exit dns_mark_exit(void)
{
    struct dns_mark_rule *rule = NULL, *tmp;
    struct dns_mark_domain_entry *domain_entry, *domain_tmp;
    struct dns_mark_mac_entry *mac_entry, *mac_tmp;
    struct dns_mark_ifname_entry *ifname_entry, *ifname_tmp;

    /* Unregister netfilter hook */
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 12, 14)
    nf_unregister_net_hooks(&init_net, nf_dns_mark_ops, ARRAY_SIZE(nf_dns_mark_ops));
#else
    nf_unregister_hooks(nf_dns_mark_ops, ARRAY_SIZE(nf_dns_mark_ops));
#endif

    /* Clean up rules */
    list_for_each_entry_safe(rule, tmp, &rule_mgr.rules, list) {
        /* Free domain entries */
        list_for_each_entry_safe(domain_entry, domain_tmp, &rule->domain_list, list) {
            list_del(&domain_entry->list);
            kfree(domain_entry->domain_pattern);
            kfree(domain_entry);
        }

        /* Free MAC entries */
        list_for_each_entry_safe(mac_entry, mac_tmp, &rule->mac_list, list) {
            list_del(&mac_entry->list);
            kfree(mac_entry);
        }

        /* Free ifname entries */
        list_for_each_entry_safe(ifname_entry, ifname_tmp, &rule->ifname_list, list) {
            list_del(&ifname_entry->list);
            kfree(ifname_entry);
        }

        list_del(&rule->list);
        dns_mark_remove_rule_proc(rule->proc);
        kfree(rule);
    }

    /* Clean up proc filesystem */
    dns_mark_proc_cleanup();

    /* Clean up any remaining pending buffer */
    if (dns_mark_write_domains_pending_buf) {
        kfree(dns_mark_write_domains_pending_buf);
        dns_mark_write_domains_pending_buf = NULL;
    }
}

module_init(dns_mark_init);
module_exit(dns_mark_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("handongming@gl-inet.com");
MODULE_DESCRIPTION("DNS Query Marking Module");
MODULE_VERSION("1.0");
