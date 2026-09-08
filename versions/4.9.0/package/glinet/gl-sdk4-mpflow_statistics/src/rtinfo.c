#define pr_fmt(fmt) "gl-mpflow: " fmt

#include <linux/netdevice.h>
#include <net/ip_fib.h>
#include <net/nexthop.h>

#include "common.h"

#define DEVINDEX_HASHBITS 8

#ifdef CONFIG_IP_ROUTE_MULTIPATH

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)
#define for_nexthops(fi) {                          \
    int nhsel; const struct fib_nh *nh;             \
    for (nhsel = 0, nh = (fi)->fib_nh;              \
         nhsel < fib_info_num_path((fi));           \
         nh++, nhsel++)

#define change_nexthops(fi) {                                       \
    int nhsel; struct fib_nh *nexthop_nh;                           \
    for (nhsel = 0, nexthop_nh = (struct fib_nh *)((fi)->fib_nh);   \
         nhsel < fib_info_num_path((fi));                           \
         nexthop_nh++, nhsel++)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0) */

#define for_nexthops(fi) {                          \
    int nhsel; const struct fib_nh *nh;             \
    for (nhsel = 0, nh = (fi)->fib_nh;              \
         nhsel < (fi)->fib_nhs;                     \
         nh++, nhsel++)

#define change_nexthops(fi) {                                       \
    int nhsel; struct fib_nh *nexthop_nh;                           \
    for (nhsel = 0, nexthop_nh = (struct fib_nh *)((fi)->fib_nh);   \
         nhsel < (fi)->fib_nhs;                                     \
         nexthop_nh++, nhsel++)
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0) */

#else /* CONFIG_IP_ROUTE_MULTIPATH */

/* Hope, that gcc will optimize it to get rid of dummy loop */

#define for_nexthops(fi) {                                  \
    int nhsel; const struct fib_nh *nh = (fi)->fib_nh;      \
    for (nhsel = 0; nhsel < 1; nhsel++)

#define change_nexthops(fi) {                                       \
    int nhsel;                                                      \
    struct fib_nh *nexthop_nh = (struct fib_nh *)((fi)->fib_nh);    \
    for (nhsel = 0; nhsel < 1; nhsel++)

#endif /* CONFIG_IP_ROUTE_MULTIPATH */

#define endfor_nexthops(fi) }

bool need_statistics(struct net_device *dev)
{
    struct hlist_head *head = fib_info_devhash_bucket(dev);
    struct fib_info *prev_fi = NULL;
    int scope = RT_SCOPE_NOWHERE;
    struct fib_nh *nh;

    if (!(dev->flags & IFF_UP))
        return false;

    hlist_for_each_entry(nh, head, nh_hash) {
        struct fib_info *fi = nh->nh_parent;

        BUG_ON(!fi->fib_nhs);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
        if (nh->fib_nh_dev != dev || fi == prev_fi)
#else
        if (nh->nh_dev != dev || fi == prev_fi)
#endif
            continue;
        prev_fi = fi;
        change_nexthops(fi) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
            if (nexthop_nh->fib_nh_dev == dev &&
                nexthop_nh->fib_nh_scope != scope &&
                fi->fib_scope == RT_SCOPE_UNIVERSE &&
                nexthop_nh->fib_nh_flags & (RTNH_F_DEAD | RTNH_F_LINKDOWN))
#else
            if (nexthop_nh->nh_dev == dev &&
                nexthop_nh->nh_scope != scope &&
                fi->fib_scope == RT_SCOPE_UNIVERSE &&
                nexthop_nh->nh_flags & (RTNH_F_DEAD | RTNH_F_LINKDOWN))
#endif
               return false;
       } endfor_nexthops(fi)
   }

    return true;
}

