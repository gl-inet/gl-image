#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/version.h>
#include <uapi/linux/icmp.h>
#include <uapi/linux/icmpv6.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <net/addrconf.h>
#include <net/ip6_fib.h>
#include <net/ip6_route.h>
#include <net/arp.h>
#include <net/ip.h>
#include "kmwan.h"

extern char rtmode;

__be32 get_ip_by_netdev(struct net_device *dev)
{
    struct in_device *indev;
    __be32 ip = 0;
    rcu_read_lock();
    indev = __in_dev_get_rcu(dev);

    if (indev) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
        for_primary_ifa(indev) {
#else
        const struct in_ifaddr *ifa;
        in_dev_for_each_ifa_rcu(ifa, indev) {
#endif
            //IFA_ADDRESS is prefix address, rather than local interface address.
            ip = ifa->ifa_local;
            break;
        }
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
        endfor_ifa(indev);
#endif
    }
    rcu_read_unlock();

    return ip;
}

int get_ifindex_by_ifname(const char *ifname)
{
    struct net_device *dev = dev_get_by_name(&init_net, ifname);
    int ret = -1;

    if (dev) {
        ret = dev->ifindex;
        dev_put(dev);
    }

    return ret;
}

static int get_gw_by_ifindex(int ifindex, __be32 *gw)
{
    struct fib_table *tb;
    struct fib_result res;

    struct flowi4 fl4 = {
        .daddr = 0x0,
        .flowi4_oif = ifindex,
        .flowi4_flags = FLOWI_FLAG_KNOWN_NH,
    };

    tb = __fib_get_table(&init_net, RT_TABLE_MAIN);
    if (!tb)
        return -ETABLE;

    if (fib_table_lookup(tb, &fl4, &res, FIB_LOOKUP_NOREF | FIB_LOOKUP_IGNORE_DEAD | FIB_LOOKUP_IGNORE_LINKSTATE))
        return -ELOOKUP;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
    if (!res.nhc)
        return -ERESFI;

    *gw = res.nhc->nhc_gw.ipv4;
#else
    if (!res.fi)
        return -ERESFI;

    *gw = res.fi->fib_nh[res.nh_sel].nh_gw;
#endif

    return 0;
}

static int send_icmp_echo(int ifindex, __be32 gwip, __be32 force_ip, __be32 dip, __be16 seq)
{
    struct sk_buff *skb;
    struct icmphdr *icmp;
    struct iphdr *h;
    __be32 sip, csum32;
    int hlen, tlen;
    struct neighbour *n;
    unsigned char *daddr = NULL;
    u16 *p;
    u16 ilen, i;
    static u16 id = 0;

    struct net_device *dev = dev_get_by_index(&init_net, ifindex);
    if (!dev)
        return -EDEVICE;

    if (rtmode)
        sip = force_ip;
    else
        sip = get_ip_by_netdev(dev);

    if (!sip) {
        dev_put(dev);
        return -ESADDR;
    }

    hlen = LL_RESERVED_SPACE(dev);
    tlen = dev->needed_tailroom;

    if (!(dev->flags & IFF_NOARP)) {
        n = __ipv4_neigh_lookup_noref(dev, gwip);
        if (!n) {
            dev_put(dev);
            return -ENEIGH;
        }
        daddr = n->ha;
    }

    /* Allocate packet */
    skb = alloc_skb(sizeof(struct icmphdr) + hlen + tlen + 15, GFP_ATOMIC);
    if (!skb) {
        GL_ERROR("[%s %d] alloc skb failed.\n", __FUNCTION__, __LINE__);
        goto err;
    }

    skb_reserve(skb, hlen);

    /* IP header*/
    h = skb_put_zero(skb, sizeof(struct iphdr));
    skb_reset_network_header(skb);
    /* ICMP header*/
    icmp = skb_put_zero(skb, sizeof(struct icmphdr));

    /* Construct IP header */
    h->version = 4;
    h->ihl = 5;
    h->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
    h->frag_off = htons(IP_DF);
    h->ttl = 64;
    h->protocol = IPPROTO_ICMP;
    h->saddr = sip;
    h->daddr = dip;
    h->check = ip_fast_csum((unsigned char *) h, h->ihl);

    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->un.echo.sequence = htons(seq);

    if (0 == id) {
        get_random_bytes(&id, 2);
        id = htons(id);
    }
    icmp->un.echo.id = id;

    p = (u16 *)icmp;
    ilen = (sizeof(struct icmphdr)) / 2;

    csum32 = 0;
    for (i = 0; i < ilen; i++) {
        csum32 = csum32 + (__be32)(*(p + i));
    }

    while (csum32 & 0xffff0000) {
        csum32 = (csum32 & 0xffff) + (csum32 >> 16);
    }
    icmp->checksum = htons(~(htons(csum32)));

    /* Chain packet down the line... */
    skb->dev = dev;
    skb->protocol = htons(ETH_P_IP);

    if (dev_hard_header(skb, dev, ntohs(skb->protocol),
                        daddr, dev->dev_addr, skb->len) < 0) {
        kfree_skb(skb);
        GL_ERROR("init hard header failed\n");
        goto err;
    }

    if (dev_queue_xmit(skb) < 0) {
        GL_ERROR("[%s %d]dev_queue_xmit failed.\n", __FUNCTION__, __LINE__);
        goto err;
    }

    dev_put(dev);
    return 0;

err:
    dev_put(dev);
    return -EOTHER;
}

static void netdev_send_icmp(int ifindex, __be32 force_ip, __be32 dip, __be16 seq)
{
    __be32 gw;
    int err;

    err = get_gw_by_ifindex(ifindex, &gw);
    if (err) {
        GL_DEBUG("[%s %d]get ipv4 gateway failed. Ecode:%d\n", __FUNCTION__, __LINE__, err);
        return;
    }

    GL_DEBUG("[%s %d]ifindex:%d send from %pI4 to %pI4\n", __FUNCTION__, __LINE__, ifindex, &dip, &gw);

    err = send_icmp_echo(ifindex, gw, force_ip, dip, seq);
    if (err)
        GL_DEBUG("[%s %d]send icmp pkg failed. Ecode:%d\n", __FUNCTION__, __LINE__, err);
}

static unsigned short calc_icmp6_cksum(struct icmp6hdr *icmp6h, struct ipv6hdr *ip6h)
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

    icmp6h->icmp6_cksum = 0;
    p = (u16 *)icmp6h;

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

static int send_icmpv6_echo(struct net_device *dev, struct in6_addr gw, struct in6_addr dip, __be16 seq)
{
    struct sk_buff *skb;
    struct icmp6hdr *icmp6h;
    struct ipv6hdr *ip6h;
    struct in6_addr sip;
    int hlen, tlen;
    struct neighbour *n;
    unsigned char *daddr = NULL;

    if (ipv6_dev_get_saddr(&init_net, dev, &dip, IPV6_PREFER_SRC_PUBTMP_DEFAULT, &sip)) {
        dev_put(dev);
        return -ESADDR;
    }

    //printk("[%s %d] dev:%s, gw:%pI6, sip:%pI6\n", __FUNCTION__, __LINE__, dev->name, &gw, &sip.s6_addr);

    hlen = LL_RESERVED_SPACE(dev);
    tlen = dev->needed_tailroom;

    if (!(dev->flags & IFF_NOARP)) {
        n = __ipv6_neigh_lookup_noref(dev, &gw);
        if (!n) {
            dev_put(dev);
            return -ENEIGH;
        }
        daddr = n->ha;
    }

    skb = alloc_skb(sizeof(struct ipv6hdr) + sizeof(struct icmp6hdr) + hlen + tlen + 15, GFP_ATOMIC);
    if (!skb) {
        GL_ERROR("[%s %d] alloc skb failed.\n", __FUNCTION__, __LINE__);
        goto err;
    }

    skb_reserve(skb, hlen);
    ip6h = skb_put_zero(skb, sizeof(struct ipv6hdr));
    skb_reset_network_header(skb);
    icmp6h = skb_put_zero(skb, sizeof(struct icmp6hdr));

    ip6h->version = 6;
    ip6h->nexthdr = IPPROTO_ICMPV6;
    ip6h->hop_limit = 64;
    ip6h->saddr = sip;
    ip6h->daddr = dip;
    ip6h->payload_len = htons(sizeof(struct icmp6hdr));

    icmp6h->icmp6_type = ICMPV6_ECHO_REQUEST;
    icmp6h->icmp6_code = 0;
    icmp6h->icmp6_dataun.u_echo.identifier = htons(0x0309);
    icmp6h->icmp6_dataun.u_echo.sequence = htons(seq);
    icmp6h->icmp6_cksum = htons(calc_icmp6_cksum(icmp6h, ip6h));

    skb->dev = dev;
    skb->protocol = htons(ETH_P_IPV6);

    if (dev_hard_header(skb, dev, ntohs(skb->protocol),
                        daddr, dev->dev_addr, skb->len) < 0) {
        kfree_skb(skb);
        GL_ERROR("init hard header failed\n");
        goto err;
    }

    if (dev_queue_xmit(skb) < 0) {
        GL_ERROR("[%s %d] dev xmit failed.\n", __FUNCTION__, __LINE__);
        goto err;
    }

    dev_put(dev);
    return 0;
err:
    dev_put(dev);
    return -EOTHER;
}

static int get_gw6_by_netdev(int ifindex, struct in6_addr *gw)
{
    struct in6_addr ip6 = IN6ADDR_ANY_INIT;
    struct fib6_table *tb;
    struct flowi6 fl6 = {
        .daddr = IN6ADDR_ANY_INIT,
        .flowi6_flags = FLOWI_FLAG_KNOWN_NH,
        .flowi6_oif = ifindex,
    };

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
    struct fib6_result res;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
    struct fib6_nh *nh;
    struct fib6_info *res;
#else
    struct rt6_info *res;
#endif

    tb = fib6_get_table(&init_net, RT6_TABLE_MAIN);
    if (!tb)
        return -ETABLE6;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
    fib6_table_lookup(&init_net, tb, ifindex, &fl6, &res,
                      RT6_LOOKUP_F_IFACE | RT6_LOOKUP_F_DST_NOREF | RT6_LOOKUP_F_IGNORE_DEAD);

    if (!res.nh)
        return -ERESFI6;

    ip6 = res.nh->fib_nh_gw6;

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
    res = fib6_table_lookup(&init_net, tb, ifindex, &fl6,
                            RT6_LOOKUP_F_IFACE | RT6_LOOKUP_F_DST_NOREF | RT6_LOOKUP_F_IGNORE_DEAD);
    if (!res)
        return -ERESFI6;

    nh = &res.f6i->fib6_nh[0];
    ip6 = nh->nh_gw;
#else
    res = (void *)ip6_route_lookup(&init_net, &fl6, RT6_LOOKUP_F_IFACE);
    if (res->dst.error) {
        dst_release(&res->dst);
        return -ERESFI6;
    }

    ip6 = res->rt6i_gateway;
    dst_release(&res->dst);
#endif

    *gw = ip6;

    return 0;
}

static void netdev_send_icmpv6(int ifindex, struct in6_addr dip, __be16 seq)
{
    struct in6_addr gw;
    int err;
    struct net_device *dev = dev_get_by_index(&init_net, ifindex);

    if (dev == NULL) {
        GL_ERROR("[%s %d]get dev failed.\n", __FUNCTION__, __LINE__);
        return;
    }

    err = get_gw6_by_netdev(ifindex, &gw);
    if (err) {
        GL_DEBUG("[%s %d]get ipv6 gateway failed. Ecode:%d\n", __FUNCTION__, __LINE__, err);
        dev_put(dev);
        return;
    }

    GL_DEBUG("[%s %d]ifindex:%d send %pI6 to %pI6\n", __FUNCTION__, __LINE__, ifindex, &dip.s6_addr, &gw.s6_addr);

    err = send_icmpv6_echo(dev, gw, dip, seq);
    if (err)
        GL_DEBUG("[%s %d]send icmpv6 pkg failed. Ecode:%d\n", __FUNCTION__, __LINE__, err);
}

void send_pkg_by_ifindex(gl_net_cell_t *node, int idx, __be16 seq)
{
    switch (node->tracks[idx].type) {
        case PING:
            if (node->addr_type == TYPE_V4) {
                netdev_send_icmp(node->ifindex, node->force_ip, node->tracks[idx].ipaddr.ip, seq);
                return;
            } else {
                netdev_send_icmpv6(node->ifindex, node->tracks[idx].ipaddr.ip6, seq);
                return;
            }
        default:
            break;
    }
}

