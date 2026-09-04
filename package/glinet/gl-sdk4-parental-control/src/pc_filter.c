#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <net/tcp.h>
#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <linux/skbuff.h>
#include <net/ip.h>
#include <linux/types.h>
#include <net/sock.h>
#include <linux/etherdevice.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include "pc_policy.h"
#include "pc_utils.h"

static int check_special_ip(__be32 ip)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
    bool flag = ipv4_is_all_snoopers(ip);
#else
    bool flag = false;
#endif

    return ipv4_is_private_10(ip) || ipv4_is_private_172(ip)
           || ipv4_is_private_192(ip) || ipv4_is_linklocal_169(ip)
           || ipv4_is_loopback(ip) || ipv4_is_test_192(ip)
           || ipv4_is_test_198(ip) || ipv4_is_anycast_6to4(ip)
           || ipv4_is_multicast(ip) || ipv4_is_local_multicast(ip)
           || ipv4_is_lbcast(ip) || flag || ipv4_is_zeronet(ip);
}

static int parse_flow_proto(struct sk_buff *skb, flow_info_t *flow)
{
    struct tcphdr *tcph = NULL;
    struct udphdr *udph = NULL;
    struct iphdr *iph = NULL;

    if (!skb)
        return -1;

    iph = ip_hdr(skb);
    if (!iph)
        return -1;

    flow->src = iph->saddr;
    flow->dst = iph->daddr;
    flow->l4_protocol = iph->protocol;

    switch (iph->protocol) {
        case IPPROTO_TCP:
            tcph = (struct tcphdr *)(iph + 1);
            flow->l4_data = skb->data + iph->ihl * 4 + tcph->doff * 4;
            flow->l4_len = ntohs(iph->tot_len) - iph->ihl * 4 - tcph->doff * 4;
            flow->dport = htons(tcph->dest);
            flow->sport = htons(tcph->source);
            return 0;
        case IPPROTO_UDP:
            udph = (struct udphdr *)(iph + 1);
            flow->l4_data = skb->data + iph->ihl * 4 + 8;
            flow->l4_len = ntohs(udph->len) - 8;
            flow->dport = htons(udph->dest);
            flow->sport = htons(udph->source);
            return 0;
        case IPPROTO_ICMP:
            break;
        default:
            return -1;
    }

    return -1;
}

static uint8_t *__parse_server_list(uint8_t *data, int len, int *hostlist_len)
{
    tls_svr_list_length  list_len;
    tls_svr_name_type    sntype;
    tls_svr_name_length  svr_name_len;

    PC_DEBUG("[__parse_server_list] svr list tot len  %d!\n", len);
    while (len > 0) {
        GET_TLS_HDR(list_len, tls_svr_list_length, data, len);
        list_len = ntohs(list_len);
        PC_DEBUG("[__parse_server_list] svr list len  %d!\n", list_len);

        if (list_len > 0) {
            GET_TLS_HDR(sntype, tls_svr_name_type, data, list_len);
            GET_TLS_HDR(svr_name_len, tls_svr_name_length, data, list_len);
            svr_name_len = ntohs(svr_name_len);
            PC_DEBUG("[__parse_server_list] svr name len  %d!\n", svr_name_len);

            /*Type only have host_name,and only have one host field */
            if ((SNI_SVR_NAME_TYPE == sntype) && (svr_name_len == list_len)) {
                *hostlist_len = svr_name_len;
                return data;
            }
        }
        SKIP_LEN(data, len, list_len);
    }

    return NULL;
}

static uint8_t *__find_extension_server_host(uint8_t *data, int len, int *hostlist_len)
{
    tls_ext_length  node_len;
    tls_ext_type    type;

    while (len > 0) {
        GET_TLS_HDR(type, tls_ext_type, data, len);
        GET_TLS_HDR(node_len, tls_ext_length, data, len);
        node_len = ntohs(node_len);
        PC_DEBUG("[__find_extension_server_host] extension node_len %d, %02x!\n", node_len, type);

        if (EXTENSION_SVR_NAME_TYPE == type) {
            return __parse_server_list(data, node_len, hostlist_len);
        }

        SKIP_LEN(data, len, node_len);
        PC_DEBUG("ext len remain %d\n", len);
    }

    return NULL;
}

static uint8_t *__parse_hand_shake(uint8_t *data, int len, int *hostlist_len)
{
    /* struct tls_hand_shake_head *pshake */
    int tlen, tlen_r;
    uint8_t *result = NULL, *tdata = NULL;
    tls_content_type         type;
    /* handshake hello length use 3 bytes, need deal with it */
    char *p_clen;
    tls_content_uint24        clen = 0;
    tls_content_major_ver     major_ver;
    tls_content_minor_ver     minior_ver;
    tls_content_gmt           gmt_unix_time;
    uint8_t random_bytes[28];
    tls_session_id_length     session;
    tls_cipher_suite_length   suite;
    tls_comp_mthods_length    methods;
    tls_ext_length            tot_len;

    PC_DEBUG("[__parse_hand_shake] handshake len %d!\n", len);

    while (len > 0) {
        GET_TLS_HDR(type, tls_content_type, data, len);
        p_clen = (char *)(&clen);
        COPY_TLS_HDR(p_clen + 1, 3, data, len);

        tlen = ntohl(clen);
        PC_DEBUG("[__parse_hand_shake] handshake tlen %d!\n", tlen);
        GET_TLS_HDR(major_ver, tls_content_major_ver, data, len);
        GET_TLS_HDR(minior_ver, tls_content_minor_ver,  data, len);
        GET_TLS_HDR(gmt_unix_time, tls_content_gmt,  data, len);
        COPY_TLS_HDR(random_bytes, sizeof(random_bytes), data, len);

        tdata = data;
        tlen_r = tlen - sizeof(tls_content_major_ver) - sizeof(tls_content_minor_ver)
                 - sizeof(tls_content_gmt) - sizeof(random_bytes);

        PC_DEBUG("[__parse_hand_shake] handshake tlen_r %d!\n", tlen_r);
        if (TLS_HANDSHAKE_CLIENT_HELLO == type) {
            GET_TLS_HDR(session, tls_session_id_length, tdata, tlen_r);
            SKIP_LEN(tdata, tlen_r, session);
            PC_DEBUG("[__parse_hand_shake] handshake session %02d!\n", session);
            GET_TLS_HDR(suite, tls_cipher_suite_length, tdata, tlen_r);
            SKIP_LEN(tdata, tlen_r, ntohs(suite));
            PC_DEBUG("[__parse_hand_shake] handshake suite %02d!\n", ntohs(suite));
            GET_TLS_HDR(methods, tls_comp_mthods_length, tdata, tlen_r);
            SKIP_LEN(tdata, tlen_r, methods);
            PC_DEBUG("[__parse_hand_shake] handshake methods %02d!\n", methods);
            if (tlen_r >= sizeof(tls_ext_length)) {
                GET_TLS_HDR(tot_len, tls_ext_length, tdata, tlen_r);
                PC_DEBUG("[__parse_hand_shake] handshake ext len %d!\n", tlen_r);
                if (tlen_r == ntohs(tot_len))
                    result = __find_extension_server_host(tdata, tlen_r, hostlist_len);
            }
            goto out;
        }
        SKIP_LEN(data, len, tlen_r);
    }

out:
    return result;
}

static uint8_t *__parse_record_layer(uint8_t *data, int len, int *hostlist_len)
{
    tls_content_type         type;
    tls_content_major_ver    major_ver;
    tls_content_minor_ver    minior_ver;
    tls_content_length       clen;
    int tlen;
    uint8_t *result = NULL;

    while (len > 0) {
        GET_TLS_HDR(type, tls_content_type, data, len);
        GET_TLS_HDR(major_ver, tls_content_major_ver, data, len);
        GET_TLS_HDR(minior_ver, tls_content_minor_ver, data, len);
        GET_TLS_HDR(clen, tls_content_length, data, len);

        tlen = ntohs(clen);
        PC_DEBUG("[__parse_record_layer] tlen %d, type %02x, major_ver %02x, minior_ver %02x\n", tlen, type, major_ver, minior_ver);
        if (TLS_CT_HANDSHAKE == type) {
            PC_DEBUG("[__parse_record_layer] is handshake packet!\n");
            result = __parse_hand_shake(data, tlen, hostlist_len);
            goto out;
        }
        SKIP_LEN(data, len, tlen);
    }

out:
    return result;
}

static uint8_t *parse_tls_server_host_list(uint8_t *data, int len, int *hostlist_len)
{
    if (!data || !hostlist_len)
        return NULL;

    *hostlist_len = 0;
    return __parse_record_layer(data, len, hostlist_len);
}

static void dpi_https_proto(flow_info_t *flow)
{
    uint8_t *hostlist = NULL;
    int host_len;

    if (NULL == flow) {
        PC_ERROR("flow is NULL\n");
        return;
    }

    /* Uses this packet to resolve HTTPS's domain name */
    hostlist = parse_tls_server_host_list(flow->l4_data, flow->l4_len, &host_len);
    if (hostlist) {
        PC_DEBUG("[%s %d]  https match: : sport = %d dport = %d  \n", __func__, __LINE__, flow->sport, flow->dport);
        flow->https.match = PC_TRUE;
        flow->https.url_pos = hostlist;
        flow->https.url_len = ntohs(host_len);
        PC_DEBUG(" https: hostlist = %s  host_len = %d\n",  hostlist, host_len);
        //printk("[%s %d] ### debug ###  flow->https.url_pos= %s  url_len = %d  sport = %d  dport = %d \n"
        //, __func__, __LINE__, flow->https.url_pos, flow->https.url_len, flow->sport, flow->dport);
        return;
    }

    return;
}
static void dpi_http_proto(flow_info_t *flow)
{
    int i = 0;
    int start = 0;
    char *data = NULL;
    int data_len = 0;

    if (!flow) {
        PC_ERROR("flow is null\n");
        return;
    }

    if (flow->l4_protocol != IPPROTO_TCP)
        return;

    data = flow->l4_data;
    data_len = flow->l4_len;
    if (data_len < MIN_HTTP_DATA_LEN)
        return;

    if (flow->sport != 80 && flow->dport != 80)
        return;

    for (i = 0; i < data_len; i++) {
        if (data[i] == 0x0d && data[i + 1] == 0x0a) {
            if (0 == memcmp(&data[start], "POST ", 5)) {
                flow->http.match = PC_TRUE;
                flow->http.method = HTTP_METHOD_POST;
                flow->http.url_pos = data + start + 5;
                flow->http.url_len = i - start - 5;
            } else if (0 == memcmp(&data[start], "GET ", 4)) {
                flow->http.match = PC_TRUE;
                flow->http.method = HTTP_METHOD_GET;
                flow->http.url_pos = data + start + 4;
                flow->http.url_len = i - start - 4;
            } else if (0 == memcmp(&data[start], "Host:", 5)) {
                flow->http.host_pos = data + start + 6;
                flow->http.host_len = i - start - 6;
            }

            if (data[i + 2] == 0x0d && data[i + 3] == 0x0a) {
                flow->http.data_pos = data + i + 4;
                flow->http.data_len = data_len - i - 4;
                break;
            }
            // 0x0d 0x0a
            start = i + 2;
        }
    }
}

static int pc_match_port(port_info_t *info, int port)
{
    int i;
    int with_not = 0;

    if (info->num == 0)
        return 1;

    for (i = 0; i < info->num; i++) {
        if (info->range_list[i].not) {
            with_not = 1;
            break;
        }
    }

    for (i = 0; i < info->num; i++) {
        if (with_not) {
            if (info->range_list[i].not && port >= info->range_list[i].start
                    && port <= info->range_list[i].end) {
                return 0;
            }
        } else {
            if (port >= info->range_list[i].start
                    && port <= info->range_list[i].end) {
                return 1;
            }
        }
    }

    if (with_not)
        return 1;
    else
        return 0;
}

static int pc_match_by_pos(flow_info_t *flow, pc_app_t *node)
{
    int i;
    unsigned int pos = 0;

    if (!flow || !node)
        return PC_FALSE;

    if (node->pos_num > 0) {
        for (i = 0; i < node->pos_num; i++) {
            // -1
            if (node->pos_info[i].pos < 0) {
                pos = flow->l4_len + node->pos_info[i].pos;
            } else {
                pos = node->pos_info[i].pos;
            }
            if (pos >= flow->l4_len) {
                return PC_FALSE;
            }
            if (flow->l4_data[pos] != node->pos_info[i].value) {
                return PC_FALSE;
            }
        }
        PC_DEBUG("match by pos, appid=%d\n", node->app_id);
        return PC_TRUE;
    }

    return PC_FALSE;
}

static int pc_match_by_url(flow_info_t *flow, pc_app_t *node)
{
    char reg_url_buf[MAX_URL_MATCH_LEN] = {0};

    if (!flow || !node)
        return PC_FALSE;

    // match host or https url
    if (flow->https.match == PC_TRUE && flow->https.url_pos) {
        if (flow->https.url_len >= MAX_URL_MATCH_LEN)
            strncpy(reg_url_buf, flow->https.url_pos, MAX_URL_MATCH_LEN - 1);
        else
            strncpy(reg_url_buf, flow->https.url_pos, flow->https.url_len);
    } else if (flow->http.match == PC_TRUE && flow->http.host_pos) {
        if (flow->http.host_len >= MAX_URL_MATCH_LEN)
            strncpy(reg_url_buf, flow->http.host_pos, MAX_URL_MATCH_LEN - 1);
        else
            strncpy(reg_url_buf, flow->http.host_pos, flow->http.host_len);
    }

    if (strlen(reg_url_buf) > 0 && strlen(node->host_url) > 0 && regexp_match(node->host_url, reg_url_buf)) {
        PC_DEBUG("match url %s   reg = %s, appid=%d\n",
                 reg_url_buf, node->host_url, node->app_id);
        return PC_TRUE;
    }

    // match request url
    if (flow->http.match == PC_TRUE && flow->http.url_pos) {
        memset(reg_url_buf, 0x0, sizeof(reg_url_buf));
        if (flow->http.url_len >= MAX_URL_MATCH_LEN)
            strncpy(reg_url_buf, flow->http.url_pos, MAX_URL_MATCH_LEN - 1);
        else
            strncpy(reg_url_buf, flow->http.url_pos, flow->http.url_len);

        if (strlen(reg_url_buf) > 0 && strlen(node->request_url) && regexp_match(node->request_url, reg_url_buf)) {
            PC_DEBUG("match request:%s   reg:%s appid=%d\n",
                     reg_url_buf, node->request_url, node->app_id);
            return PC_TRUE;
        }
    }

    return PC_FALSE;
}

static int pc_match_by_domain(flow_info_t *flow, pc_domain_t *node)
{
    char reg_url_buf[MAX_URL_MATCH_LEN] = {0};

    if (!flow || !node)
        return PC_FALSE;

    // match host or https url
    if (flow->https.match == PC_TRUE && flow->https.url_pos) {
        if (flow->https.url_len >= MAX_URL_MATCH_LEN)
            strncpy(reg_url_buf, flow->https.url_pos, MAX_URL_MATCH_LEN - 1);
        else
            strncpy(reg_url_buf, flow->https.url_pos, flow->https.url_len);
    } else if (flow->http.match == PC_TRUE && flow->http.host_pos) {
        if (flow->http.host_len >= MAX_URL_MATCH_LEN)
            strncpy(reg_url_buf, flow->http.host_pos, MAX_URL_MATCH_LEN - 1);
        else
            strncpy(reg_url_buf, flow->http.host_pos, flow->http.host_len);
    }

    if (strlen(reg_url_buf) > 0 && strlen(node->domain) > 0 && regexp_match(node->domain, reg_url_buf)) {
        PC_DEBUG("match url %s   reg = %s\n",
                 reg_url_buf, node->domain);
        return PC_TRUE;
    }

    return PC_FALSE;
}

static int pc_match_one(flow_info_t *flow, pc_app_t *node)
{
    int ret = PC_FALSE;

    if (!flow || !node) {
        PC_ERROR("node or flow is NULL\n");
        return PC_FALSE;
    }

    if (node->proto > 0 && flow->l4_protocol != node->proto)
        return PC_FALSE;

    if (flow->l4_len == 0)
        return PC_FALSE;

    if (node->sport != 0 && flow->sport != node->sport)
        return PC_FALSE;

    if (!pc_match_port(&node->dport_info, flow->dport))
        return PC_FALSE;

    if (strlen(node->request_url) > 0 ||
            strlen(node->host_url) > 0) {
        ret = pc_match_by_url(flow, node);
    } else if (node->pos_num > 0) {
        ret = pc_match_by_pos(flow, node);
    } else {
        PC_DEBUG("node is empty, match sport:%d,dport:%d, appid = %d\n",
                 node->sport, node->dport, node->app_id);
        return PC_TRUE;
    }

    return ret;
}

static int app_in_rule(u_int32_t app, pc_rule_t *rule)
{
    pc_app_index_t *node = NULL;

    if (app < MAX_APP_IN_CLASS)
        return PC_FALSE;

    list_for_each_entry(node, &rule->applist, head) {
        if (app == node->app_id)
            return PC_TRUE;
    }

    app = app / MAX_APP_IN_CLASS; //如果单个应用不匹配，进一步检查是否匹配应用类型
    list_for_each_entry(node, &rule->applist, head) {
        if (app == node->app_id)
            return PC_TRUE;
    }

    return PC_FALSE;
}

static int match_blist_app(flow_info_t *flow, pc_rule_t *rule)
{
    pc_app_t *app = NULL;
    pc_domain_t *domain = NULL;

    list_for_each_entry(app, &rule->blist, head) {
        if (pc_match_one(flow, app)) {
            PC_LMT_DEBUG("rule %s match blist app %s from mac %pM\n", rule->id, app->app_name, flow->smac);
            return PC_TRUE;
        }
    }

    list_for_each_entry(domain, &rule->domain_list, head) {
        if (pc_match_by_domain(flow, domain)) {
            PC_LMT_DEBUG("rule %s match blist from mac %pM\n", rule->id, flow->smac);
            return PC_TRUE;
        }
    }

    return PC_FALSE;
}

static int app_filter_match(flow_info_t *flow, pc_rule_t *rule)
{
    pc_app_t *node = NULL;

    rcu_read_lock();

    if (rule == NULL || flow == NULL)
        goto EXIT;

    if (match_blist_app(flow, rule)) {
        flow->drop = PC_TRUE;
        PC_LMT_DEBUG("match blist from mac %pM, policy is %s\n", flow->smac, flow->drop ? "DROP" : "ACCEPT");
        goto EXIT;
    }

    list_for_each_entry_rcu(node, &pc_app_head, head) {
        if (!app_in_rule(node->app_id, rule))
            continue;

        if (pc_match_one(flow, node)) {
            if (rule->action == PC_POLICY_DROP) {
                flow->drop = PC_TRUE;
            } else {
                flow->drop = PC_FALSE;
            }
            strncpy(flow->app_name, node->app_name, sizeof(flow->app_name) - 1);
            flow->app_id = node->app_id;
            PC_LMT_DEBUG("match app %d from mac %pM, policy is %s\n", node->app_id, flow->smac, flow->drop ? "DROP" : "ACCEPT");
            goto EXIT;
        }
    }
    flow->drop = PC_FALSE;

EXIT:
    rcu_read_unlock();
    return 0;
}

static int dpi_main(struct sk_buff *skb, flow_info_t *flow)
{
    dpi_http_proto(flow);
    dpi_https_proto(flow);
    /*if (TEST_MODE())
        dump_flow_info(flow);*/

    return 0;
}

static void pc_get_smac(struct sk_buff *skb,  u8 smac[ETH_ALEN])
{
    struct ethhdr *ethhdr = NULL;
    ethhdr = eth_hdr(skb);

    if (ethhdr)
        memcpy(smac, ethhdr->h_source, ETH_ALEN);
    /*else
        memcpy(smac, &skb->cb[40], ETH_ALEN);*/
}

static int check_source_net_dev(struct sk_buff *skb)
{
    struct net_device *netdev = skb->dev;
    struct monitor_dev *dev = NULL;

    if (!netdev)
        return PC_FALSE;
    PC_LMT_DEBUG("get package from %s\n", netdev->name);

    if (0 == strlen(pc_src_dev)) {
        PC_LMT_DEBUG("match any netdev\n");
        return PC_TRUE;
    }

    list_for_each_entry_rcu(dev, &monitor_dev_list, dev_head) {
        if (netdev->ifindex == dev->ifidex) {
            PC_LMT_DEBUG("match net dev %s\n", netdev->name);
            return PC_TRUE;
        }
    }

    return PC_FALSE;
}

static u_int32_t pc_filter_hook_handle(struct sk_buff *skb, struct net_device *dev)
{
    u_int32_t ret;
    flow_info_t flow;
    pc_rule_t *rules[MAX_GROUPS_PER_MAC];
    int rule_count, i;
    enum ip_conntrack_info ctinfo;
    struct nf_conn *ct = NULL;
    enum pc_action action;

    if (!check_source_net_dev(skb)) {
        ret = NF_ACCEPT;
        goto EXIT;
    }

    ct = nf_ct_get(skb, &ctinfo);
    /*if (ct) {
        PC_LMT_DEBUG("ctinfo %d\n", ctinfo);
    } else {
        PC_LMT_DEBUG("no ctinfo found\n");
    }*/

    memset((char *)&flow, 0x0, sizeof(flow_info_t));
    pc_get_smac(skb,  flow.smac);
    if (is_zero_ether_addr(flow.smac) || is_broadcast_ether_addr(flow.smac)) {
        ret = NF_ACCEPT;
        goto EXIT;
    }

    rule_count = get_rule_by_mac(flow.smac, rules, MAX_GROUPS_PER_MAC, &action);

    if (action != PC_POLICY_DROP) {
        struct iphdr *iph = ip_hdr(skb);

        if (check_special_ip(iph->saddr) && check_special_ip(iph->daddr)) {
            ret = NF_ACCEPT;
            goto EXIT;
        }
    }

    switch (action) {
        case PC_DROP:
            PC_LMT_DEBUG("from mac %pM action is DROP\n", flow.smac);
            ret = NF_DROP;
            goto EXIT;
        case PC_ACCEPT:
            PC_LMT_DEBUG("from mac %pM action is ACCEPT\n", flow.smac);
            ret = NF_ACCEPT;
            goto EXIT;
        case PC_DROP_ANONYMOUS:
            if (ctinfo == IP_CT_ESTABLISHED || ctinfo == IP_CT_RELATED || ctinfo == IP_CT_IS_REPLY) {
                PC_LMT_DEBUG("from mac %pM action is match ct\n", flow.smac);
                ret = NF_ACCEPT;
            } else {
                PC_LMT_DEBUG("from mac %pM action is ANONYMOUS DROP\n", flow.smac);
                ret = NF_DROP;
            }
            goto EXIT;
        case PC_POLICY_DROP:
            PC_LMT_DEBUG("from mac %pM action is POLICY DROP\n", flow.smac);
            break;
        case PC_POLICY_ACCEPT:
            break;
        default:
            ret = NF_ACCEPT;
            goto EXIT;
    }

    if (rule_count == 0) {
        PC_LMT_DEBUG("from mac %pM rule is NULL,ACCEPT\n", flow.smac);
        ret = NF_ACCEPT;
        goto EXIT;
    }

    if (parse_flow_proto(skb, &flow) < 0) {
        PC_LMT_DEBUG("from mac %pM parese proto failed, ACCEPT\n", flow.smac);
        ret = NF_ACCEPT;
        goto EXIT;
    }

    if (0 != dpi_main(skb, &flow)) {
        PC_LMT_DEBUG("from mac %pM dpi failed, ACCEPT\n", flow.smac);
        ret = NF_ACCEPT;
        goto EXIT;
    }

    for (i = 0; i < rule_count; i++) {
        if (rules[i]) {
            app_filter_match(&flow, rules[i]);
            if (flow.drop)
                break;
        }
    }

    if (flow.app_id != 0) {
        PC_LMT_DEBUG("match %s %pI4(%d)--> %pI4(%d) len = %d, %d\n ", IPPROTO_TCP == flow.l4_protocol ? "tcp" : "udp",
                     &flow.src, flow.sport, &flow.dst, flow.dport, skb->len, flow.app_id);
    }

    if (flow.drop) {
        PC_LMT_DEBUG("Drop app %s flow, appid is %d\n", flow.app_name, flow.app_id);
        ret =  NF_DROP;
        goto EXIT;
    }
    ret = NF_ACCEPT;

EXIT:
    return ret;
}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static u_int32_t pc_filter_hook(void *priv,
                                struct sk_buff *skb,
                                const struct nf_hook_state *state)
{
#else
static u_int32_t pc_filter_hook(unsigned int hook,
                                struct sk_buff *skb,
                                const struct net_device *in,
                                const struct net_device *out,
                                int (*okfn)(struct sk_buff *))
{
#endif
    return pc_filter_hook_handle(skb, skb->dev);
}

static bool is_valid_dns_query_header(unsigned char *dns_header, __be16 *qd)
{
    __be16 id, flag;

    if (unlikely(!dns_header)) {
        //printk("dns header is null.\n");
        return false;
    }

    memcpy((void *)&id, dns_header, 2);
    memcpy((void *)&flag, dns_header + 2, 2);
    memcpy((void *)qd, dns_header + 4, 2);

    if (ntohs(*qd) < 1) //问题数
        return false;
    if (ntohs(flag) & 0x8000) //QR: 0表示查询报文, 1表示响应报文
        return false;
    if ((ntohs(flag) & 0x7100) >> 11) //opcode, 只解析标准查询(0)
        return false;
    if ((ntohs(flag) & 0x70)) //必须为0
        return false;

    PC_LMT_DEBUG("id is %hu\n", ntohs(id));

    return true;
}

static void update_domain(char *domain, char *src, int len, bool first)
{
    if (!first)
        strcat(domain, ".");
    strncat(domain, src, len);
}

static void __parse_dns_query(unsigned char *payload, int *len, int *consume, pc_rule_t *rule, int *ret)
{
    char domain[256] = {0};
    char token[128];
    int offset = 0;
    int token_len = 0;
    pc_app_t *app = NULL;
    pc_domain_t *node = NULL;

    if (!payload || *len < 0)
        return;

    while (*len > 0) {
        memset(token, 0, sizeof(token));
        token_len = payload[offset];

        if (token_len > 0 && token_len <= *len &&
                token_len < (int)sizeof(token) &&
                (strlen(domain) + (domain[0] == 0 ? 0 : 1) + token_len) < (int)sizeof(domain)) {
            int copy_len = token_len < (int)sizeof(token) ? token_len : (int)sizeof(token) - 1;
            strncpy(token, payload + offset + 1, copy_len);
            token[copy_len] = '\0';

            update_domain(domain, token, token_len, domain[0] == 0);
            *len -= token_len + 1;
            offset += token_len + 1;
        } else {
            //offset此时指向域名信息的下一个字节(该字节为0,表示域名信息结束).
            //加4是跳过dns查询报文的固定4字节(type 2 + class 2)
            *consume += offset + 4 + 1;
            break;
        }
    }

    if (strlen(domain) == 0)
        return;

    rcu_read_lock();
    list_for_each_entry_rcu(app, &rule->blist, head) {
        *ret = regexp_match(app->host_url, domain) == 1 ? 1 : 0;
        if (*ret == 1) {
            PC_LMT_DEBUG("domain:%s\n", domain);
            rcu_read_unlock();
            return;
        }
    }
    list_for_each_entry_rcu(node, &rule->domain_list, head) {
        *ret = regexp_match(node->domain, domain) == 1 ? 1 : 0;
        if (*ret == 1) {
            PC_LMT_DEBUG("domain:%s\n", domain);
            rcu_read_unlock();
            return;
        }
    }
    rcu_read_unlock();
}

static void parse_dns_query(unsigned char *payload, int len, __be16 qd, pc_rule_t *rule, int *ret)
{
    int i, consume = 0;

    //printk("qd is %hu, len=%d\n", ntohs(qd), len);
    for (i = 0; i < ntohs(qd); i++) {
        __parse_dns_query(payload, &len, &consume, rule, ret);
        if (*ret == 1)
            return;
        if (!consume || len == 0)
            break;
        //printk("consume = %d\n", consume);
        payload = payload + consume;
        len -= consume;
        consume = 0;
    }

    return;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static u_int32_t pc_filter_dns_hook(void *priv,
                                    struct sk_buff *skb,
                                    const struct nf_hook_state *state)
{
#else
static u_int32_t pc_filter_dns_hook(unsigned int hook,
                                    struct sk_buff *skb,
                                    const struct net_device *in,
                                    const struct net_device *out,
                                    int (*okfn)(struct sk_buff *))
{
#endif
    static unsigned short dns_port = htons(53);
    struct iphdr *iph = ip_hdr(skb);
    struct udphdr *uh;
    unsigned char dns_payload[512];
    int dns_payload_len;
    __be16 qd = 0;
    u8 smac[ETH_ALEN];
    pc_rule_t *rules[MAX_GROUPS_PER_MAC];
    int rule_count, i;
    enum pc_action action;
    int drop = 0;

    if (!check_source_net_dev(skb))
        return NF_ACCEPT;

    if (iph->protocol != IPPROTO_UDP || udp_hdr(skb)->dest != dns_port)
        return NF_ACCEPT;

    pc_get_smac(skb, smac);

    rule_count = get_rule_by_mac(smac, rules, MAX_GROUPS_PER_MAC, &action);
    if (action != PC_POLICY_DROP || rule_count == 0)
        return NF_ACCEPT;

    uh = udp_hdr(skb);
    memset(dns_payload, 0, sizeof(dns_payload));

    // 24 = udp头部长度(8) + dns头部长度(12) + dns请求固定字段type(2) + class(2)
    if (ntohs(uh->len) - 24 <= 1 || !is_valid_dns_query_header((unsigned char *)(uh + 1), &qd)) {
        //printk("len not enough.\n");
        return NF_ACCEPT;
    }

    dns_payload_len = ntohs(uh->len) - sizeof(struct udphdr) - 12;
    if (dns_payload_len > 512) {
        dns_payload_len = 512;
    }
    memcpy((void *)dns_payload, (unsigned char *)(uh + 1) + 12, dns_payload_len);

    for (i = 0; i < rule_count; i++) {
        if (rules[i]) {
            parse_dns_query(dns_payload, dns_payload_len, qd, rules[i], &drop);
            if (drop)
                break;
        }
    }

    if (drop)
        return NF_DROP;

    return NF_ACCEPT;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static struct nf_hook_ops pc_filter_ops[] __read_mostly = {
    {
        .hook = pc_filter_hook,
        .pf = PF_INET,
        .hooknum = NF_INET_FORWARD,
        .priority = NF_IP_PRI_MANGLE + 1,
    },
    {
        .hook = pc_filter_dns_hook,
        .pf = PF_INET,
        .hooknum = NF_INET_PRE_ROUTING,
        .priority = NF_IP_PRI_NAT_DST - 1,
    },
};
#else
static struct nf_hook_ops pc_filter_ops[] __read_mostly = {
    {
        .hook = pc_filter_hook,
        .owner = THIS_MODULE,
        .pf = PF_INET,
        .hooknum = NF_INET_FORWARD,
        .priority = NF_IP_PRI_MANGLE + 1,
    },
    {
        .hook = pc_filter_dns_hook,
        .owner = THIS_MODULE,
        .pf = PF_INET,
        .hooknum = NF_INET_PRE_ROUTING,
        .priority = NF_IP_PRI_NAT_DST - 1,
    },
};
#endif

#ifdef CONFIG_SHORTCUT_FE
extern int (*gl_parental_control_handle)(struct sk_buff *skb);
extern int (*athrs_fast_nat_recv)(struct sk_buff *skb);
static int pc_handle_shortcut_fe(struct sk_buff *skb)
{
    int (*fast_recv)(struct sk_buff * skb);
    const struct iphdr *iph = ip_hdr(skb);

    if (iph->version != 4)
        return NET_RX_SUCCESS;

    if (ipv4_is_loopback(iph->daddr))
        return NET_RX_SUCCESS;

    rcu_read_lock();
    fast_recv = rcu_dereference(athrs_fast_nat_recv);
    rcu_read_unlock();
    if (!fast_recv)//no shortcut module installed
        return NET_RX_SUCCESS;

    switch (pc_filter_hook_handle(skb, skb->dev)) {
        case NF_ACCEPT:
            return NET_RX_SUCCESS;
        case NF_DROP:
            return NET_RX_DROP;
        default:
            return NET_RX_SUCCESS;
    }
}

static void pc_rpc_pointer_init(void)
{
    int (*test)(struct sk_buff * skb);

    rcu_read_lock();
    test = rcu_dereference(gl_parental_control_handle);
    rcu_read_unlock();
    if (!test) {
        RCU_INIT_POINTER(gl_parental_control_handle, pc_handle_shortcut_fe);
    }
}

static void pc_rpc_pointer_exit(void)
{
    RCU_INIT_POINTER(gl_parental_control_handle, NULL);
    //wait for all rcu call complete
    rcu_barrier();
}
#else
static void pc_rpc_pointer_init(void) {}
static void pc_rpc_pointer_exit(void) {}
#endif

static void rcu_free_monitor_dev(struct rcu_head *head)
{
    struct monitor_dev *dev = container_of(head, struct monitor_dev, rcu);
    kfree(dev);
}

static void clean_monitor_dev(void)
{
    struct monitor_dev *dev, *n;

    write_lock(&monitor_dev_lock);
    list_for_each_entry_safe(dev, n, &monitor_dev_list, dev_head) {
        list_del_rcu(&dev->dev_head);
        call_rcu(&dev->rcu, rcu_free_monitor_dev);
    }
    write_unlock(&monitor_dev_lock);
    rcu_barrier();
}

static void update_ifindex(char *ifname)
{
    struct monitor_dev *dev;

    if (!ifname)
        return;

    list_for_each_entry(dev, &monitor_dev_list, dev_head) {
        if (!strncmp(dev->ifname, ifname, IFNAMSIZ)) {
            write_lock(&monitor_dev_lock);
            dev->ifidex = get_ifindex_by_ifname(ifname);
            write_unlock(&monitor_dev_lock);
        }
    }
}

static int pc_device_event(struct notifier_block *unused,
                           unsigned long event, void *ptr)
{
    struct net_device *dev = netdev_notifier_info_to_dev(ptr);

    switch (event) {
        case NETDEV_CHANGENAME:
        case NETDEV_UNREGISTER:
        case NETDEV_REGISTER:
            update_ifindex(dev->name);
            break;
    }

    return 0;
}

static struct notifier_block pc_notifier_block = {
    .notifier_call = pc_device_event,
};

int pc_filter_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
    nf_register_net_hooks(&init_net, pc_filter_ops, ARRAY_SIZE(pc_filter_ops));
#else
    nf_register_hooks(pc_filter_ops, ARRAY_SIZE(pc_filter_ops));
#endif
    pc_rpc_pointer_init();
    register_netdevice_notifier(&pc_notifier_block);

    return 0;
}

void pc_filter_exit(void)
{
    unregister_netdevice_notifier(&pc_notifier_block);
    clean_monitor_dev();
    pc_rpc_pointer_exit();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
    nf_unregister_net_hooks(&init_net, pc_filter_ops, ARRAY_SIZE(pc_filter_ops));
#else
    nf_unregister_hooks(pc_filter_ops, ARRAY_SIZE(pc_filter_ops));
#endif

    return;
}

