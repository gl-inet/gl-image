#ifndef __MPTCP_COMPAT_H
#define __MPTCP_COMPAT_H


#ifdef CONFIG_SKB_EXTENSIONS
struct skb_ext *skb_ext_alloc(void);
#define __skb_ext_alloc(x) skb_ext_alloc()
void *__skb_ext_set(struct sk_buff *skb, enum skb_ext_id id,
		    struct skb_ext *ext);
#endif

#if IS_ENABLED(CONFIG_MPTCP_IPV6)
int inet6_sendmsg(struct socket *sock, struct msghdr *msg, size_t size);
int inet6_recvmsg(struct socket *sock, struct msghdr *msg, size_t size,
		  int flags);
#endif

static inline void inet_csk_prepare_for_destroy_sock(struct sock *sk)
{
	/* The below has to be done to allow calling inet_csk_destroy_sock */
	sock_set_flag(sk, SOCK_DEAD);
	percpu_counter_inc(sk->sk_prot->orphan_count);
}

void tcp_cleanup_rbuf(struct sock *sk, int copied);
void tcp_remove_empty_skb(struct sock *sk, struct sk_buff *skb);
void tcp_update_recv_tstamps(struct sk_buff *skb,
				    struct scm_timestamping_internal *tss);
void tcp_recv_timestamp(struct msghdr *msg, const struct sock *sk,
			       struct scm_timestamping_internal *tss);

void __lock_sock(struct sock *sk);
void __tcp_close(struct sock *sk, long timeout);

void sk_error_report(struct sock *sk);

//TO-DO
struct sk_buff *tcp_build_frag(struct sock *sk, int size_goal, int flags,
			       struct page *page, int offset, size_t *size);


#ifdef CONFIG_COMPAT
int inet6_compat_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg);
#endif

#ifdef CONFIG_CGROUPS
static inline u64 cgroup_id(const struct cgroup *cgrp)
{
	return cgrp->kn->id.id;
}
#else
static inline u64 cgroup_id(const struct cgroup *cgrp) { return 1; }
#endif

extern const struct inet_connection_sock_af_ops ipv6_specific;

bool __lock_sock_fast(struct sock *sk);

/* fast socket lock variant for caller already holding a [different] socket lock */
static inline bool lock_sock_fast_nested(struct sock *sk)
{
	mutex_acquire(&sk->sk_lock.dep_map, SINGLE_DEPTH_NESTING, 0, _RET_IP_);

	return __lock_sock_fast(sk);
}

static inline int copy_from_sockptr_offset(void *dst, char __user *src,
		size_t offset, size_t size)
{
	return copy_from_user(dst, src + offset, size);
	return 0;
}

static inline long strncpy_from_sockptr(char *dst, char __user *src, size_t count)
{
	return strncpy_from_user(dst, src, count);
}

static inline void sock_valbool_flag(struct sock *sk, enum sock_flags bit,
				     int valbool)
{
	if (valbool)
		sock_set_flag(sk, bit);
	else
		sock_reset_flag(sk, bit);
}

void sock_set_timestamp(struct sock *sk, int optname, bool valbool);

static inline int copy_from_sockptr(void *dst, char __user *src, size_t size)
{
	return copy_from_sockptr_offset(dst, src, 0, size);
}

void sk_stop_timer_sync(struct sock *sk, struct timer_list *timer);

/**
 * struct so_timestamping - SO_TIMESTAMPING parameter
 *
 * @flags:	SO_TIMESTAMPING flags
 * @bind_phc:	Index of PTP virtual clock bound to sock. This is available
 *		if flag SOF_TIMESTAMPING_BIND_PHC is set.
 */
struct so_timestamping {
	int flags;
	int bind_phc;
};

int sock_set_timestamping(struct sock *sk, int optname,
			  struct so_timestamping timestamping);

#endif