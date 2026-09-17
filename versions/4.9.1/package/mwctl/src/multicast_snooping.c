/* Copyright (C) 2021 Mediatek Inc. */
#define _GNU_SOURCE

#include "mtk_vendor_nl80211.h"
#include "mt76-vendor.h"
#include "mwctl.h"

DECLARE_SECTION(set);

#define MULTICAST_SNOOPING_OPTIONS \
	"\nfor multi phy:\n"\
	"  [enable=<0 or 1>][policy=<flood|drop>]\n"\
	"  [add=<group id as IP or MAC addr>[-<member MAC addr>]]\n"\
	"  [deny=<group IP addr>][floodingcidr=<option>-<IP addr>/<prefix>]\n"\
	"for sig phy:\n"\
	"  [m2uenable=<0 or 1> or <1-1>][b2uenable=<0 or 1>][unknownpolicy=<flood|drop>]\n"\
	"  [mwdscloneenable=<0 or 1>]"\
	"  [addfilter=<group id as IP>[-<member MAC addr>]]\n"\
	"  [delfilter=<group id as IP>[-<member MAC addr>]]\n"\
	"  [addblack=<IP addr>/<prefix>-type]\n"\
	"  [delblack=<IP addr>/<prefix>-type]\n"\
	"  [addflood=<IP addr>/<prefix>-type]\n"\
	"  [delflood=<IP addr>/<prefix>-type]\n"\
	"  [radiosize=<band0size>-<band1size>-<band2size>]\n"\
	"  [pktvlanenable=<0 or 1>]\n"\
	"  [status | count | table]\n"

#define MAX_MULTICAST_SNOOPING_PARAM_LEN 32
#define MAX_COMMAND_LEN			64
struct igmpsn_multiphy_option {
	char option_name[MAX_MULTICAST_SNOOPING_PARAM_LEN];
	int (*attr_put)(struct nl_msg *msg, char *value);
};
struct igmpsn_sigphy_get_option {
	char option_name[MAX_MULTICAST_SNOOPING_PARAM_LEN];
	int (*attr_put)(char *value);
};

int get_igmp_status_callback(struct nl_msg *msg, void *data)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *vndr_tb[MTK_NL80211_VENDOR_MCAST_SNOOP_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	u8 igmp_status;
	int err = 0;
	//u16 acl_result_len = 0;

	err = nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
			genlmsg_attrlen(gnlh, 0), NULL);
	if (err < 0)
		return err;

	if (tb[NL80211_ATTR_VENDOR_DATA]) {
		err = nla_parse_nested(vndr_tb, MTK_NL80211_VENDOR_MCAST_SNOOP_ATTR_MAX,
			tb[NL80211_ATTR_VENDOR_DATA], NULL);
		if (err < 0)
			return err;

		if (vndr_tb[MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_ENABLE]) {
			igmp_status = nla_get_u8(vndr_tb[MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_ENABLE]);
			if (igmp_status == 0) {
				printf("disabled\n");
			} else {
				printf("enabled\n");
			}
		}
	}

	return 0;
}


static int mcast_snoop_enable_attr_put(struct nl_msg *msg, char *value)
{
	unsigned char mcsnoop_enable;

	if (!value)
		return -EINVAL;

	if (*value == '0')
		mcsnoop_enable = 0;
	else if (*value == '1')
		mcsnoop_enable = 1;
	else if (*value == 's') {
		register_handler(get_igmp_status_callback, NULL);
		mcsnoop_enable = 0xf;
	} else
		return -EINVAL;

	if (nla_put_u8(msg, MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_ENABLE, mcsnoop_enable))
		return -EMSGSIZE;

	return 0;
}

static int mcast_snoop_unknown_policy_attr_put(struct nl_msg *msg, char *value)
{
	unsigned char mcsnoop_plcy;

	if (!value)
		return -EINVAL;

	if (strlen(value) == strlen("drop") && !strncmp(value, "drop", strlen("drop")))
		mcsnoop_plcy = 0;
	else if (strlen(value) == strlen("flood") && !strncmp(value, "flood", strlen("flood")))
		mcsnoop_plcy = 1;
	else
		return -EINVAL;

	if (nla_put_u8(msg, MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_UNKNOWN_PLCY, mcsnoop_plcy))
		return -EMSGSIZE;

	return 0;
}

static int mcast_snoop_entry_add_attr_put(struct nl_msg *msg, char *value)
{
	if (nla_put_string(msg, MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_ENTRY_ADD, value))
		return -EMSGSIZE;

	return 0;
}

static int mcast_snoop_entry_del_attr_put(struct nl_msg *msg, char *value)
{
	if (nla_put_string(msg, MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_ENTRY_DEL, value))
		return -EMSGSIZE;

	return 0;
}

static int mcast_snoop_deny_list_attr_put(struct nl_msg *msg, char *value)
{
	if (nla_put_string(msg, MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_DENY_LIST, value))
		return -EMSGSIZE;

	return 0;
}

static int mcast_snoop_floodingcidr_attr_put(struct nl_msg *msg, char *value)
{
	if (nla_put_string(msg, MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_FLOODINGCIDR, value))
		return -EMSGSIZE;

	return 0;
}

struct igmpsn_multiphy_option igmpsn_multiphy_ops[] = {
	{"enable",			mcast_snoop_enable_attr_put},
	{"policy",			mcast_snoop_unknown_policy_attr_put},
	{"add",				mcast_snoop_entry_add_attr_put},
	{"del",				mcast_snoop_entry_del_attr_put},
	{"deny",			mcast_snoop_deny_list_attr_put},
	{"floodingcidr",		mcast_snoop_floodingcidr_attr_put},
};

int get_igmp_status_info_callback(char *data)
{
	u8 status = 0;
	u8 ipv4enable, ipv6enable;
	char *data_end;

	errno = 0; // Clear errno before calling strtol
	status = (u8)strtol(data, &data_end, 10);
	if (errno == 0 && *data_end == '\0') {
		ipv4enable = status & 0x01;
		ipv6enable = (status & 0x02) >> 1;
		printf("%s,str2num success = 0x%02x, ipv4enable = %d, ipv6enable = %d\n",
			__func__, status, ipv4enable, ipv6enable);
	} else
		printf("str2num fail = %s\n", data_end);
	return 0;
}
int get_igmp_table_info_callback(char *data)
{
	return 0;
}
int get_igmp_count_info_callback(char *data)
{
	return 0;
}
struct igmpsn_sigphy_get_option igmpsn_sigphy_get_ops[] = {
	{"status", 	get_igmp_status_info_callback},
	{"table", 	get_igmp_table_info_callback},
	{"count",	get_igmp_count_info_callback},
};
int get_igmp_info_callback(struct nl_msg *msg, void *data)
{
	int err = 0;
	char *igmp_status;
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *vndr_tb[MTK_NL80211_VENDOR_MCAST_SNOOP_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

	err = nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
			genlmsg_attrlen(gnlh, 0), NULL);

	if (err < 0)
		return err;


	if (tb[NL80211_ATTR_VENDOR_DATA]) {
		err = nla_parse_nested(vndr_tb, MTK_NL80211_VENDOR_MCAST_SNOOP_ATTR_MAX,
			tb[NL80211_ATTR_VENDOR_DATA], NULL);
		if (err < 0)
			return err;

		if (vndr_tb[MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_GET_OPTION]) {
			u16 j;
			char *ptr, *param_str, *val_str;

			igmp_status = nla_get_string(vndr_tb[MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_GET_OPTION]);

			if (!igmp_status) {
				printf("%s: igmp_status is NULL\n", __func__);
				return err;
			}

			ptr = igmp_status;//status=3
			param_str = ptr;
			val_str = strchr(ptr, '=');

			if (!val_str)
				return err;

			*val_str++ = 0;
			/* such logan return string is: status=3
			ptr=status,len=6,val_str=3
			=>means ipv4 and ipv6 enable */
			printf("%s: ptr:%s,param_str:%s, val_str:%s\n",
				__func__, ptr, param_str, val_str);

			for (j = 0; j < (sizeof(igmpsn_sigphy_get_ops) / sizeof(igmpsn_sigphy_get_ops[0])); j++) {
				if (strlen(igmpsn_sigphy_get_ops[j].option_name) == strlen(param_str) &&
					!strncmp(igmpsn_sigphy_get_ops[j].option_name, param_str, strlen(param_str)))
					break;
			}

			if (j != sizeof(igmpsn_sigphy_get_ops) / sizeof(igmpsn_sigphy_get_ops[0])) {
				if (igmpsn_sigphy_get_ops[j].attr_put(val_str) < 0) {
					printf("invalid getinfo\n");
					return -EINVAL;
				}
			} else
				return -EINVAL;
		}
	}

	return 0;
}

static int mcast_snoop_set_cmd_attr_put(struct nl_msg *msg, char *value)
{
	if (nla_put_string(msg, MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_SET_OPTION, value))
		return -EMSGSIZE;

	return 0;
}

static int mcast_snoop_get_cmd_attr_put(struct nl_msg *msg, char *value)
{
	u16 j;

	for (j = 0; j < ARRAY_SIZE(igmpsn_sigphy_get_ops); j++) {
		if (strlen(igmpsn_sigphy_get_ops[j].option_name) == strlen(value) &&
			!strncmp(igmpsn_sigphy_get_ops[j].option_name, value, strlen(value)))
			break;
	}

	if (j != ARRAY_SIZE(igmpsn_sigphy_get_ops)) {
		register_handler(get_igmp_info_callback, NULL);
		if (nla_put_string(msg, MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_GET_OPTION, igmpsn_sigphy_get_ops[j].option_name))
			return -EMSGSIZE;
	} else
		return -EINVAL;

	return 0;
}

int handle_multicast_snooping_set(struct nl_msg *msg,
				int argc,
				char **argv,
				void *ctx)
{
	int i, j, ret = 0, arg_size=0;
	void *data;
	char *ptr, invalid = 0;
	char *param_str, *val_str;
	char dst_ptr[MAX_COMMAND_LEN] ={0}, *pdst_ptr = dst_ptr;

	if (!argc)
		return -EINVAL;
	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!data)
		return -ENOMEM;

	/*handler link_id*/
	ret = add_msg_band_link_id(msg, get_band_link_id(), MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_BAND_LINK_ID);

	if (ret < 0)
		return ret;

	for (i = 0; i < argc; i++) {
		ptr = argv[i];
		arg_size = strlen(ptr);
		/* check cmd length */
		if (arg_size > MAX_COMMAND_LEN - 1)
			continue;
		memcpy(pdst_ptr, ptr, arg_size);
		pdst_ptr[arg_size] = '\0';
		param_str = pdst_ptr;

		val_str = strchr(pdst_ptr, '=');

		/* check whether is wifi8 get cmd */
		if (!val_str) {
			if (mcast_snoop_get_cmd_attr_put(msg, param_str) < 0) {
				printf("invalid argument %p, ignore it\n", param_str);
			} else
				invalid = 1;
			printf("val_str=NULL, param_str= %s, for wifi8 get \n", param_str);
			continue;
		}
		*val_str++ = 0;

		/* check whether is wifi7 cmd */
		for (j = 0; j < (sizeof(igmpsn_multiphy_ops) / sizeof(igmpsn_multiphy_ops[0])); j++) {
			if (strlen(igmpsn_multiphy_ops[j].option_name) == strlen(param_str) &&
				!strncmp(igmpsn_multiphy_ops[j].option_name, param_str, strlen(param_str)))
				break;
		}
		if (j != sizeof(igmpsn_multiphy_ops) / sizeof(igmpsn_multiphy_ops[0])) {
			if (igmpsn_multiphy_ops[j].attr_put(msg, val_str) < 0)
				printf("invalid argument %s=%s, ignore it\n", param_str, val_str);
			else
				invalid = 1;

			continue;
		}
		/* check whether is wifi8 set cmd */
		param_str = ptr;

		if (mcast_snoop_set_cmd_attr_put(msg, param_str) < 0)
			printf("invalid aargument %p, ignore it\n", param_str);
		else
			invalid = 1;
	}

	nla_nest_end(msg, data);

	if(!invalid)
		return -EINVAL;

	return 0;
}

COMMAND(set, multicast_snooping, MULTICAST_SNOOPING_OPTIONS,
	MTK_NL80211_VENDOR_SUBCMD_SET_MULTICAST_SNOOPING, 0,
	CIB_NETDEV, handle_multicast_snooping_set,
	"Set the multicast snooping related configurations to specific bss\n");
