#ifndef __GL_MPTUN__H
#define __GL_MPTUN__H

#include <linux/if.h>
#include <linux/list.h>
#include <linux/version.h>
#include <linux/timekeeping.h>

#define STAT_TIME 30  //Unit:min
#define WAN_NUMS   10

#define MDEV_RX      (1)
#define MDEV_TX      (2)

#define MDEV_MASK (MDEV_RX | MDEV_TX)

struct gl_monitor_dev {
    struct list_head list;
    char netdev[IFNAMSIZ];
    int flag;
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
    unsigned long long tx_rate[STAT_TIME];
    unsigned long long rx_rate[STAT_TIME];
    time64_t time;
};

extern rwlock_t mpflow_lock;
extern rwlock_t mpdead_lock;

extern struct list_head mpflow_active;
extern struct list_head mpflow_deading;

void parse_str(char *buf);
void flush_monitor_dev(void);
bool need_statistics(struct net_device *dev);

static inline void clean_rate_flag(struct gl_monitor_dev *mdev)
{
    mdev->flag &= 0xfffffffc;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
static inline time64_t ktime_get_boottime_seconds(void)
{
    return ktime_divns(ktime_get_boottime(), NSEC_PER_SEC);
}
#endif

#endif
