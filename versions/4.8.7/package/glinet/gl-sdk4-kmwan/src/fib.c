#include <linux/bug.h>
#include <linux/list.h>
#include <linux/rtnetlink.h>
#include <linux/inetdevice.h>
#include <net/ip6_route.h>
#include "kmwan.h"
#include <net/ip_fib.h>
#include <net/ip6_fib.h>
#include <net/net_namespace.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
#include <net/fib_notifier.h>
#endif

/************************************ ipv4 ************************************/
/* caller must hold either rtnl or rcu read lock */
struct fib_table *__fib_get_table(struct net *net, u32 id)
{
    struct fib_table *tb;
    struct hlist_head *head;
    unsigned int h;

    if (id == 0)
        id = RT_TABLE_MAIN;

    h = id & (FIB_TABLE_HASHSZ - 1);

    head = &net->ipv4.fib_table_hash[h];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
    hlist_for_each_entry_rcu(tb, head, tb_hlist, lockdep_rtnl_is_held())
#else
    hlist_for_each_entry_rcu(tb, head, tb_hlist)
#endif
    {
        if (tb && tb->tb_id == id)
            return tb;
    }

    return NULL;
}

static int __set_fib_nh(char flag, int ifindex)
{
    struct fib_table *tb;
    struct fib_result res;
    struct net *net = &init_net;
    int lookup_ret = 0;

    struct flowi4 fl4 = {
        .daddr = (__be32)0x0,
        .flowi4_flags = FLOWI_FLAG_KNOWN_NH,
        .flowi4_oif = ifindex,
    };
    rtnl_lock();
    rcu_read_lock();
    tb = __fib_get_table(net, RT_TABLE_MAIN);
    if (!tb) {
        rcu_read_unlock();
        rtnl_unlock();
        return -ETABLE;
    }

    if ((lookup_ret = fib_table_lookup(tb, &fl4, &res,
                                       FIB_LOOKUP_NOREF | FIB_LOOKUP_IGNORE_DEAD | FIB_LOOKUP_IGNORE_LINKSTATE))) {
        GL_DEBUG("lookup_ret:%d\n", lookup_ret);
        rcu_read_unlock();
        rtnl_unlock();
        return -ELOOKUP;
    }
    rcu_read_unlock();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
    if (!(res.nhc && res.nhc->nhc_dev)) {
        rtnl_unlock();
        return -ERESFI;
    }

    if (flag == 1) {
        if (!(res.nhc->nhc_flags & RTNH_F_DEAD)) {
            fib_sync_down_dev(res.nhc->nhc_dev, NETDEV_DEAD, 0);
        }
    } else {
        fib_sync_up(res.nhc->nhc_dev, RTNH_F_DEAD);
    }
#else
    if (!res.fi) {
        rtnl_unlock();
        return -ERESFI;
    }

    struct fib_nh *nh = &res.fi->fib_nh[res.nh_sel];
    if (!(nh && nh->nh_dev)) {
        rtnl_unlock();
        return -ERESFI;
    }

    if (flag == 1) {
        if (!(nh->nh_flags & RTNH_F_DEAD)) {
            fib_sync_down_dev(nh->nh_dev, NETDEV_DEAD, 0);
        }
    } else {
        fib_sync_up(nh->nh_dev,  RTNH_F_DEAD);
    }
#endif

    rtnl_unlock();
    atomic_inc(&net->ipv4.rt_genid);
    GL_DEBUG("[%s %d]ifindex:%d route_flag:%d\n", __FUNCTION__, __LINE__, ifindex, flag);

    return 0;
}

/************************************ ipv6 ************************************/

static int __set_fib6_nh(char flag, int ifindex)
{
    struct fib6_table *tb_id;
    struct net *net = &init_net;
    int ret = 0;
    struct flowi6 fl6 = {
        .daddr = IN6ADDR_ANY_INIT,
        .flowi6_flags = FLOWI_FLAG_KNOWN_NH,
        .flowi6_oif = ifindex,
    };

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
    struct fib6_result res;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
    struct fib6_info *res;
#else
    struct rt6_info *res;
#endif
    rtnl_lock();
    rcu_read_lock();
    tb_id = fib6_get_table(net, RT6_TABLE_MAIN);
    if (!tb_id) {
        ret = -ETABLE6;
        goto out;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
    fib6_table_lookup(net, tb_id, ifindex, &fl6, &res,
                      RT6_LOOKUP_F_IFACE | RT6_LOOKUP_F_DST_NOREF | RT6_LOOKUP_F_IGNORE_DEAD | RT6_LOOKUP_F_IGNORE_LINKSTATE);
    if (!res.nh) {
        ret = -ERESFI6;
        goto out;
    }

    if (flag == 1)
        rt6_sync_down_dev(res.nh->nh_common.nhc_dev, NETDEV_DEAD);
    else
        rt6_sync_up(res.nh->nh_common.nhc_dev, RTNH_F_DEAD);

    rt_genid_bump_ipv6(net);

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
    res = fib6_table_lookup(net, tb_id, ifindex, &fl6,
                            RT6_LOOKUP_F_IFACE | RT6_LOOKUP_F_DST_NOREF | RT6_LOOKUP_F_IGNORE_DEAD);
    if (!res) {
        ret = -ERESFI6;
        goto out;
    }
#else
    res = (void *)ip6_route_lookup(net, &fl6, RT6_LOOKUP_F_IFACE);
    if (res->dst.error) {
        dst_release(&res->dst);
        ret = -ERESFI6;
        goto out;
    }

    if (flag == 1)
        res->rt6i_flags |= RTF_EXPIRES;
    else
        res->rt6i_flags &= ~RTF_EXPIRES;

    dst_release(&res->dst);
#endif
    GL_DEBUG("[%s %d]ifindex:%d set route_flag:%d\n", __FUNCTION__, __LINE__, ifindex, flag);

out:
    rcu_read_unlock();
    rtnl_unlock();
    return 0;
}

int set_fib_nh(char flag, gl_net_cell_t *node)
{
    if (node->addr_type == TYPE_V4)
        return  __set_fib_nh(flag, node->ifindex);

    return __set_fib6_nh(flag, node->ifindex);
}
