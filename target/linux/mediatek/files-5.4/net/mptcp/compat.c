#include <linux/refcount.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <net/xfrm.h>
#include <linux/version.h>

#ifdef CONFIG_COMPAT
#include <net/ip6_route.h>
#endif

#include <net/mptcp.h>
#include "compat.h"
#include "sha2.h"


#ifdef CONFIG_COMPAT
struct compat_in6_rtmsg {
	struct in6_addr		rtmsg_dst;
	struct in6_addr		rtmsg_src;
	struct in6_addr		rtmsg_gateway;
	u32			rtmsg_type;
	u16			rtmsg_dst_len;
	u16			rtmsg_src_len;
	u32			rtmsg_metric;
	u32			rtmsg_info;
	u32			rtmsg_flags;
	s32			rtmsg_ifindex;
};

static int inet6_compat_routing_ioctl(struct sock *sk, unsigned int cmd,
		struct compat_in6_rtmsg __user *ur)
{
	struct in6_rtmsg rt;

	if (copy_from_user(&rt.rtmsg_dst, &ur->rtmsg_dst,
			3 * sizeof(struct in6_addr)) ||
	    get_user(rt.rtmsg_type, &ur->rtmsg_type) ||
	    get_user(rt.rtmsg_dst_len, &ur->rtmsg_dst_len) ||
	    get_user(rt.rtmsg_src_len, &ur->rtmsg_src_len) ||
	    get_user(rt.rtmsg_metric, &ur->rtmsg_metric) ||
	    get_user(rt.rtmsg_info, &ur->rtmsg_info) ||
	    get_user(rt.rtmsg_flags, &ur->rtmsg_flags) ||
	    get_user(rt.rtmsg_ifindex, &ur->rtmsg_ifindex))
		return -EFAULT;


	return ipv6_route_ioctl(sock_net(sk), cmd, &rt);
}

int inet6_compat_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	void __user *argp = compat_ptr(arg);
	struct sock *sk = sock->sk;

	switch (cmd) {
	case SIOCADDRT:
	case SIOCDELRT:
		return inet6_compat_routing_ioctl(sk, cmd, argp);
	default:
		return -ENOIOCTLCMD;
	}
}

#endif

void sk_error_report(struct sock *sk)
{
	sk->sk_error_report(sk);

	switch (sk->sk_family) {
	case AF_INET:
		fallthrough;
	case AF_INET6:
		//trace_inet_sk_error_report(sk);
		break;
	default:
		break;
	}
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 238)
void sk_stop_timer_sync(struct sock *sk, struct timer_list *timer)
{
	if (del_timer_sync(timer))
		__sock_put(sk);
}
#endif

static void __sock_set_timestamps(struct sock *sk, bool val, bool new, bool ns)
{
	if (val)  {
		sock_valbool_flag(sk, SOCK_TSTAMP_NEW, new);
		sock_valbool_flag(sk, SOCK_RCVTSTAMPNS, ns);
		sock_set_flag(sk, SOCK_RCVTSTAMP);
		sock_enable_timestamp(sk, SOCK_TIMESTAMP);
	} else {
		sock_reset_flag(sk, SOCK_RCVTSTAMP);
		sock_reset_flag(sk, SOCK_RCVTSTAMPNS);
	}
}

void sock_set_timestamp(struct sock *sk, int optname, bool valbool)
{
	switch (optname) {
	case SO_TIMESTAMP_OLD:
		__sock_set_timestamps(sk, valbool, false, false);
		break;
	case SO_TIMESTAMP_NEW:
		__sock_set_timestamps(sk, valbool, true, false);
		break;
	case SO_TIMESTAMPNS_OLD:
		__sock_set_timestamps(sk, valbool, false, true);
		break;
	case SO_TIMESTAMPNS_NEW:
		__sock_set_timestamps(sk, valbool, true, true);
		break;
	}
}

int sock_set_timestamping(struct sock *sk, int optname,
			  struct so_timestamping timestamping)
{
// TODO
	pr_err("mptcp debug: %s,%s,%d",__FILE__,__func__,__LINE__);
	return 0;
}

void sha256(const u8 *data, unsigned int len, u8 *out)
{
	struct sha256_state sctx;

	sha256_init(&sctx);
	sha256_update(&sctx, data, len);
	sha256_final(&sctx, out);
}

bool __lock_sock_fast(struct sock *sk) __acquires(&sk->sk_lock.slock)
{
	might_sleep();
	spin_lock_bh(&sk->sk_lock.slock);

	if (!sk->sk_lock.owned) {
		/*
		 * Fast path return with bottom halves disabled and
		 * sock::sk_lock.slock held.
		 *
		 * The 'mutex' is not contended and holding
		 * sock::sk_lock.slock prevents all other lockers to
		 * proceed so the corresponding unlock_sock_fast() can
		 * avoid the slow path of release_sock() completely and
		 * just release slock.
		 *
		 * From a semantical POV this is equivalent to 'acquiring'
		 * the 'mutex', hence the corresponding lockdep
		 * mutex_release() has to happen in the fast path of
		 * unlock_sock_fast().
		 */
		return false;
	}

	__lock_sock(sk);
	sk->sk_lock.owned = 1;
	__acquire(&sk->sk_lock.slock);
	spin_unlock_bh(&sk->sk_lock.slock);
	return true;
}
