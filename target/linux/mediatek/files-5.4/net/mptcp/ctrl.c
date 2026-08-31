// SPDX-License-Identifier: GPL-2.0
/* Multipath TCP
 *
 * Copyright (c) 2019, Tessares SA.
 */

#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif

#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include "protocol.h"

#define MPTCP_SYSCTL_PATH "net/mptcp"

static int mptcp_pernet_id;
struct gl_mptcp_info gl_msg[8];
int gl_ratio;
unsigned int gl_rtt_limit;

struct mptcp_pernet {
#ifdef CONFIG_SYSCTL
	struct ctl_table_header *ctl_table_hdr;
#endif

	unsigned int add_addr_timeout;
	unsigned int stale_loss_cnt;
	u8 mptcp_enabled;
	u8 checksum_enabled;
	u8 allow_join_initial_addr_port;
	int ratio;
	unsigned int rtt_limit;
};

static struct mptcp_pernet *mptcp_get_pernet(const struct net *net)
{
	return net_generic(net, mptcp_pernet_id);
}

int mptcp_get_ratio(const struct net *net)
{
	return mptcp_get_pernet(net)->ratio;
}

unsigned int mptcp_get_rtt_limit(const struct net * net)
{
	return mptcp_get_pernet(net)->rtt_limit;
}

int mptcp_is_enabled(const struct net *net)
{
	return mptcp_get_pernet(net)->mptcp_enabled;
}

unsigned int mptcp_get_add_addr_timeout(const struct net *net)
{
	return mptcp_get_pernet(net)->add_addr_timeout;
}

int mptcp_is_checksum_enabled(const struct net *net)
{
	return mptcp_get_pernet(net)->checksum_enabled;
}

int mptcp_allow_join_id0(const struct net *net)
{
	return mptcp_get_pernet(net)->allow_join_initial_addr_port;
}

unsigned int mptcp_stale_loss_cnt(const struct net *net)
{
	return mptcp_get_pernet(net)->stale_loss_cnt;
}

static void mptcp_pernet_set_defaults(struct mptcp_pernet *pernet)
{
	pernet->mptcp_enabled = 1;
	pernet->add_addr_timeout = TCP_RTO_MAX;
	pernet->checksum_enabled = 0;
	pernet->allow_join_initial_addr_port = 1;
	pernet->stale_loss_cnt = 4;
	pernet->ratio = 8;
	pernet->rtt_limit = 3000;
}

#ifdef CONFIG_SYSCTL
static struct ctl_table mptcp_sysctl_table[] = {
	{
		.procname = "enabled",
		.maxlen = sizeof(u8),
		.mode = 0644,
		/* users with CAP_NET_ADMIN or root (not and) can change this
		 * value, same as other sysctl or the 'net' tree.
		 */
		.proc_handler = proc_dou8vec_minmax,
		.extra1       = SYSCTL_ZERO,
		.extra2       = SYSCTL_ONE
	},
	{
		.procname = "add_addr_timeout",
		.maxlen = sizeof(unsigned int),
		.mode = 0644,
		.proc_handler = proc_dointvec_jiffies,
	},
	{
		.procname = "checksum_enabled",
		.maxlen = sizeof(u8),
		.mode = 0644,
		.proc_handler = proc_dou8vec_minmax,
		.extra1       = SYSCTL_ZERO,
		.extra2       = SYSCTL_ONE
	},
	{
		.procname = "allow_join_initial_addr_port",
		.maxlen = sizeof(u8),
		.mode = 0644,
		.proc_handler = proc_dou8vec_minmax,
		.extra1       = SYSCTL_ZERO,
		.extra2       = SYSCTL_ONE
	},
	{
		.procname = "stale_loss_cnt",
		.maxlen = sizeof(unsigned int),
		.mode = 0644,
		.proc_handler = proc_douintvec_minmax,
	},
	{
		.procname = "ratio",
		.maxlen = sizeof(int),
		.mode = 0644,
		.proc_handler = proc_dointvec_minmax,
	},
	{
		.procname = "rtt_limit",
		.maxlen = sizeof(unsigned int),
		.mode = 0644,
		.proc_handler = proc_douintvec_minmax,
	},
	{}
};

static int mptcp_pernet_new_table(struct net *net, struct mptcp_pernet *pernet)
{
	struct ctl_table_header *hdr;
	struct ctl_table *table;

	table = mptcp_sysctl_table;
	if (!net_eq(net, &init_net)) {
		table = kmemdup(table, sizeof(mptcp_sysctl_table), GFP_KERNEL);
		if (!table)
			goto err_alloc;
	}

	table[0].data = &pernet->mptcp_enabled;
	table[1].data = &pernet->add_addr_timeout;
	table[2].data = &pernet->checksum_enabled;
	table[3].data = &pernet->allow_join_initial_addr_port;
	table[4].data = &pernet->stale_loss_cnt;
	table[5].data = &pernet->ratio;
	table[6].data = &pernet->rtt_limit;

	hdr = register_net_sysctl(net, MPTCP_SYSCTL_PATH, table);
	if (!hdr)
		goto err_reg;

	pernet->ctl_table_hdr = hdr;

	return 0;

err_reg:
	if (!net_eq(net, &init_net))
		kfree(table);
err_alloc:
	return -ENOMEM;
}

static void mptcp_pernet_del_table(struct mptcp_pernet *pernet)
{
	struct ctl_table *table = pernet->ctl_table_hdr->ctl_table_arg;

	unregister_net_sysctl_table(pernet->ctl_table_hdr);

	kfree(table);
}

#else

static int mptcp_pernet_new_table(struct net *net, struct mptcp_pernet *pernet)
{
	return 0;
}

static void mptcp_pernet_del_table(struct mptcp_pernet *pernet) {}

#endif /* CONFIG_SYSCTL */

static int proc_show(struct seq_file *s, void *v)
{
	int i = 0;
	for (; i < 2; i++) {
		seq_printf(s, "[%d] send=%llu acked=%llu\n", i, gl_msg[i].send, gl_msg[i].acked);
		seq_printf(s, "[%d] rtt=%u snd_cwnd=%u\n", i, gl_msg[i].rtt_us, gl_msg[i].scwnd);
		seq_printf(s, "[%d] lost=%u lost_total=%u\n", i, gl_msg[i].lost_out, gl_msg[i].lost);
		seq_printf(s, "[%d] advmess=%hu rcv_wnd=%u\n",i, gl_msg[i].advmss, gl_msg[i].rcv_wnd);
		seq_printf(s, "[%d] rcv=%llu\n", i, gl_msg[i].rcv);

		if (gl_msg[0].send && gl_msg[1].send) {
			seq_printf(s, "[%d] send[%d]/send[%d]=%llu ack[%d]/ack[%d]=%llu\n", i, i, 1-i,
				gl_msg[i].send/gl_msg[1-i].send, i, 1-i, gl_msg[i].acked/gl_msg[1-i].acked);
		}
		if (gl_msg[0].rcv && gl_msg[1].rcv) {
			seq_printf(s, "[%d] rcv[%d]/rcv[%d]=%llu\n", i, i, 1-i, gl_msg[i].rcv/gl_msg[1-i].rcv);
		}
	}
	return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show, NULL);
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 5, 0)
static const struct file_operations gl_proc_ops = {
	.owner = THIS_MODULE,
	.read = seq_read,
	.open = proc_open,
	.llseek = seq_lseek,
	.release = seq_release_private,
};
#else
static const struct proc_ops gl_proc_ops = {
	.proc_flags = PROC_ENTRY_PERMANENT,
	.proc_read = seq_read,
	.proc_open = proc_open,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release_private,
};
#endif

static int __net_init mptcp_net_init(struct net *net)
{
	struct mptcp_pernet *pernet = mptcp_get_pernet(net);
	struct proc_dir_entry *proc = proc_mkdir("gl-mptcp", NULL);
	proc_create("list", 0644, proc, &gl_proc_ops);

	mptcp_pernet_set_defaults(pernet);

	return mptcp_pernet_new_table(net, pernet);
}

/* Note: the callback will only be called per extra netns */
static void __net_exit mptcp_net_exit(struct net *net)
{
	struct mptcp_pernet *pernet = mptcp_get_pernet(net);

	mptcp_pernet_del_table(pernet);
	remove_proc_subtree("gl-mptcp", NULL);
}

static struct pernet_operations mptcp_pernet_ops = {
	.init = mptcp_net_init,
	.exit = mptcp_net_exit,
	.id = &mptcp_pernet_id,
	.size = sizeof(struct mptcp_pernet),
};

void __init mptcp_init(void)
{
	mptcp_join_cookie_init();
	mptcp_proto_init();

	if (register_pernet_subsys(&mptcp_pernet_ops) < 0)
		panic("Failed to register MPTCP pernet subsystem.\n");
}

#if IS_ENABLED(CONFIG_MPTCP_IPV6)
int __init mptcpv6_init(void)
{
	int err;

	err = mptcp_proto_v6_init();

	return err;
}
#endif
