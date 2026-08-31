#include <linux/netfilter.h>
#include <linux/netfilter_ipv6.h>
#include <linux/skbuff.h>
#include <linux/timekeeping.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/ip.h>
#include <net/addrconf.h>
#include <linux/inetdevice.h>
#include <net/netfilter/nf_conntrack.h>
#include <linux/udp.h>

#include "kmwan.h"
extern unsigned short DNS_PORT;

static int check_ipv6_addr(struct in6_addr *addr)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
    bool flag = ipv6_addr_is_all_snoopers(addr);
#else
    bool flag = false;
#endif

    return  ipv6_addr_is_solict_mult(addr) || ipv6_addr_is_ll_all_nodes(addr)
            || ipv6_addr_is_ll_all_routers(addr) || ipv6_addr_is_isatap(addr)
            || flag || ipv6_addr_loopback(addr) || ipv6_addr_is_multicast(addr);
}

/*排除RS、RA、NS、NA、重定向报文和icmpv6通信失败的报文影响*/
static int check_ipv6pkg(struct ipv6hdr *ip6h)
{
    struct icmp6hdr *icmp6h;

    if (ip6h->nexthdr != IPPROTO_ICMPV6)
        return 0;

    icmp6h = (struct icmp6hdr *)((u8 *)ip6h + sizeof(struct ipv6hdr));

    if (icmp6h->icmp6_type == 1) {
        return (icmp6h->icmp6_code == ICMPV6_NOROUTE || icmp6h->icmp6_code == ICMPV6_ADM_PROHIBITED
                || icmp6h->icmp6_code == ICMPV6_NOT_NEIGHBOUR || icmp6h->icmp6_code == ICMPV6_ADDR_UNREACH
                || icmp6h->icmp6_code == ICMPV6_PORT_UNREACH || icmp6h->icmp6_code == ICMPV6_POLICY_FAIL
                || icmp6h->icmp6_code == ICMPV6_REJECT_ROUTE);
    }

    return (icmp6h->icmp6_type == NDISC_ROUTER_SOLICITATION || icmp6h->icmp6_type == NDISC_ROUTER_ADVERTISEMENT
            || icmp6h->icmp6_type == NDISC_NEIGHBOUR_SOLICITATION || icmp6h->icmp6_type ==  NDISC_NEIGHBOUR_ADVERTISEMENT
            || icmp6h->icmp6_type == NDISC_REDIRECT || icmp6h->icmp6_type == ICMPV6_PKT_TOOBIG
            || icmp6h->icmp6_type == ICMPV6_TIME_EXCEED || icmp6h->icmp6_type == ICMPV6_PARAMPROB);
}

/*排除mDNS、DHCPv6报文的影响*/
static bool is_dhcpv6_or_mdns_pkg(struct sk_buff *skb)
{
    static unsigned short cli_port = htons(547);
    static unsigned short srv_port = htons(546);
    static unsigned short mdns_port = htons(5353);

    if (ipv6_hdr(skb)->nexthdr != IPPROTO_UDP)
        return false;

    return (udp_hdr(skb)->source == cli_port && udp_hdr(skb)->dest == srv_port)
           || (udp_hdr(skb)->source == srv_port && udp_hdr(skb)->dest == cli_port)
           || udp_hdr(skb)->dest == mdns_port;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static u_int32_t kmwan6_pre_hook(void *priv,
                                 struct sk_buff *skb,
                                 const struct nf_hook_state *state)
#else
static u_int32_t kmwan6_pre_hook(unsigned int hook,
                                 struct sk_buff *skb,
                                 const struct net_device *in,
                                 const struct net_device *out,
                                 int (*okfn)(struct sk_buff *))
#endif
{
    struct ipv6hdr *ip6h;

    if (!IS_IPV6(skb))
        return NF_ACCEPT;

    ip6h = ipv6_hdr(skb);
    if (check_ipv6_addr(&ip6h->saddr) || check_ipv6_addr(&ip6h->daddr)
            || check_ipv6pkg(ip6h) || is_dhcpv6_or_mdns_pkg(skb)) {
        return NF_ACCEPT;
    }
    /*
      In passthrough mode, the upper level of the router under test will send an icmpv6
      request packet to the test router, thereby affecting the state transition. */
    if (ip6h->nexthdr == IPPROTO_ICMPV6) {
        struct icmp6hdr *icmp6h = icmp6_hdr(skb);
        if (icmp6h->icmp6_type == ICMPV6_ECHO_REQUEST && icmp6h->icmp6_code == 0)
            return NF_ACCEPT;
    }

    if (ip6h->nexthdr == IPPROTO_UDP && udp_hdr(skb)->source == DNS_PORT)
        return NF_ACCEPT;

    if (skb->dev) {
        GL_DEBUG("%s Get RX packet: %pI6 send to %pI6\n", skb->dev->name, &ip6h->saddr, &ip6h->daddr);
        increase_package_by_name(skb, GL_CELL_RX, TYPE_V6, ICMPV6_ECHO_REPLY);
    }

    return NF_ACCEPT;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static u_int32_t kmwan6_post_hook(void *priv,
                                  struct sk_buff *skb,
                                  const struct nf_hook_state *state)
#else
static u_int32_t kmwan6_post_hook(unsigned int hook,
                                  struct sk_buff *skb,
                                  const struct net_device *in,
                                  const struct net_device *out,
                                  int (*okfn)(struct sk_buff *))
#endif
{
    struct ipv6hdr *ip6h;
    if (!IS_IPV6(skb))
        return NF_ACCEPT;

    ip6h = ipv6_hdr(skb);

    if (check_ipv6_addr(&ip6h->daddr) ||   check_ipv6pkg(ip6h)
            || is_dhcpv6_or_mdns_pkg(skb)) {
        return NF_ACCEPT;
    }

    if (ip6h->nexthdr == IPPROTO_UDP && udp_hdr(skb)->dest == DNS_PORT)
        return NF_ACCEPT;

    if (skb->dev) {
        GL_DEBUG("%s Get TX packet: %pI6 send to %pI6\n", skb->dev->name, &ip6h->saddr, &ip6h->daddr);
        increase_package_by_name(skb, GL_CELL_TX, TYPE_V6, ICMPV6_ECHO_REQUEST);
    }

    return NF_ACCEPT;
}

static struct nf_hook_ops kmwan6_ops[] __read_mostly = {
    {
        .hook = kmwan6_pre_hook,
        .pf = NFPROTO_IPV6,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
        .owner = THIS_MODULE,
#endif
        .hooknum = NF_INET_PRE_ROUTING,
        .priority = NF_IP6_PRI_FIRST + 2,
    },
    {
        .hook = kmwan6_post_hook,
        .pf = NFPROTO_IPV6,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
        .owner = THIS_MODULE,
#endif
        .hooknum = NF_INET_POST_ROUTING,
        .priority = NF_IP6_PRI_FIRST + 2,
    },
};

int gl_kmwan6_init(void)
{
    int ret = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
    ret = nf_register_net_hooks(&init_net, kmwan6_ops, ARRAY_SIZE(kmwan6_ops));
#else
    ret = nf_register_hooks(kmwan6_ops, ARRAY_SIZE(kmwan6_ops));
#endif

    return ret;
}

void gl_kmwan6_exit(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
    nf_unregister_net_hooks(&init_net, kmwan6_ops, ARRAY_SIZE(kmwan6_ops));
#else
    nf_unregister_hooks(kmwan6_ops, ARRAY_SIZE(kmwan6_ops));
#endif
}

