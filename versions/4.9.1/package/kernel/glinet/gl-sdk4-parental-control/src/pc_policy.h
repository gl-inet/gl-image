#ifndef __PC_POLICY_H__
#define __PC_POLICY_H__

#include "cJSON.h"

#define PC_FEATURE_CONFIG_FILE "/tmp/pc_app_feature.cfg"
#define NF_DROP_BIT 0x80000000
#define NF_ACCEPT_BIT 0x40000000
#define BLIST_ID 0xffffffff
#define MAX_HC_CLIENT_HASH_SIZE 128
#define MAX_DPI_PKT_NUM 64
#define MIN_HTTP_DATA_LEN 16
#define MAX_APP_NAME_LEN 64
#define MAX_FEATURE_NUM_PER_APP 16
#define MIN_FEATURE_STR_LEN 8
#define MAX_FEATURE_STR_LEN 128
#define MAX_HOST_URL_LEN 254
#define MAX_REQUEST_URL_LEN 128
#define MAX_FEATURE_BITS 16
#define MAX_POS_INFO_PER_FEATURE 16
#define MAX_FEATURE_LINE_LEN 256
#define MIN_FEATURE_LINE_LEN 16
#define MAX_URL_MATCH_LEN 64
#define MAX_BYPASS_DPI_PKT_LEN 600
#define RULE_ID_SIZE 32
#define GROUP_ID_SIZE 32
#define MAX_PORT_RANGE_NUM 5
#define MAX_APP_IN_CLASS 1000
#define MAX_SRC_DEVNAME_SIZE 129

#define PC_TRUE 1
#define PC_FALSE 0

#define HTTP_GET_METHOD_STR "GET"
#define HTTP_POST_METHOD_STR "POST"
#define HTTP_HEADER "HTTP"

#define HTTPS_URL_OFFSET        9
#define HTTPS_LEN_OFFSET        7

#define MAX_GROUPS_PER_MAC 10


extern u8 pc_drop_anonymous;
extern u8 PC_LOG_LEVEL;
extern char pc_src_dev[129];
extern struct list_head pc_app_head;
extern struct list_head monitor_dev_list;
extern rwlock_t pc_app_lock;
extern rwlock_t pc_policy_lock;
extern rwlock_t monitor_dev_lock;

#define pc_app_read_lock() read_lock_bh(&pc_app_lock);
#define pc_app_read_unlock() read_unlock_bh(&pc_app_lock);
#define pc_app_write_lock() write_lock_bh(&pc_app_lock);
#define pc_app_write_unlock() write_unlock_bh(&pc_app_lock);

#define pc_policy_read_lock() read_lock_bh(&pc_policy_lock);
#define pc_policy_read_unlock() read_unlock_bh(&pc_policy_lock);
#define pc_policy_write_lock() write_lock_bh(&pc_policy_lock);
#define pc_policy_write_unlock() write_unlock_bh(&pc_policy_lock);

struct monitor_dev {
    char ifname[IFNAMSIZ];
    int ifidex;
    struct list_head dev_head;
    struct rcu_head rcu;
};

enum e_http_method {
    HTTP_METHOD_GET = 1,
    HTTP_METHOD_POST,
};
typedef struct http_proto {
    int match;
    int method;
    char *url_pos;
    int url_len;
    char *host_pos;
    int host_len;
    char *data_pos;
    int data_len;
} http_proto_t;

typedef struct https_proto {
    int match;
    char *url_pos;
    int url_len;
} https_proto_t;

typedef struct flow_info {
    struct nf_conn *ct;
    u8 smac[ETH_ALEN];
    u_int32_t src;
    u_int32_t dst;
    int l4_protocol;
    u_int16_t sport;
    u_int16_t dport;
    unsigned char *l4_data;
    int l4_len;
    http_proto_t http;
    https_proto_t https;
    u_int32_t app_id;
    u_int8_t app_name[MAX_APP_NAME_LEN];
    u_int8_t drop;
    u_int8_t dir;
    u_int16_t total_len;
} flow_info_t;

enum {
    PC_PROTO_PARAM_INDEX,
    PC_SRC_PORT_PARAM_INDEX,
    PC_DST_PORT_PARAM_INDEX,
    PC_HOST_URL_PARAM_INDEX,
    PC_REQUEST_URL_PARAM_INDEX,
    PC_DICT_PARAM_INDEX,
};

typedef struct pc_pos_info {
    int pos;
    unsigned char value;
} pc_pos_info_t;

typedef struct range_value {
    int not ;
    int start;
    int end;
} range_value_t;

typedef struct port_info {
    u_int8_t mode; // 0: match, 1: not match
    int num;
    range_value_t range_list[MAX_PORT_RANGE_NUM];
} port_info_t;

typedef struct pc_app {
    struct list_head        head;
    u_int32_t app_id;
    char app_name[MAX_APP_NAME_LEN];
    char feature_str[MAX_FEATURE_NUM_PER_APP][MAX_FEATURE_STR_LEN];
    u_int32_t proto;
    u_int32_t sport;
    u_int32_t dport;
    port_info_t dport_info;
    char host_url[MAX_HOST_URL_LEN];
    char request_url[MAX_REQUEST_URL_LEN];
    int pos_num;
    pc_pos_info_t pos_info[MAX_POS_INFO_PER_FEATURE];
    struct rcu_head rcu;
} pc_app_t;

typedef struct pc_domain {
    struct list_head head;
    char domain[MAX_HOST_URL_LEN];
} pc_domain_t;

typedef struct pc_app_index {
    struct list_head        head;
    u_int32_t app_id;
} pc_app_index_t;

enum pc_action {
    PC_DROP = 0,
    PC_ACCEPT,
    PC_POLICY_DROP,
    PC_POLICY_ACCEPT,
    PC_DROP_ANONYMOUS,
};

typedef struct pc_rule {
    struct list_head head;
    char id[RULE_ID_SIZE];
    unsigned int refer_count;
    enum pc_action action;
    struct list_head        blist;
    struct list_head        applist;
    struct list_head        domain_list;
    struct rcu_head rcu;
} pc_rule_t;

typedef struct pc_mac {
    struct list_head        head;
    u8 mac[ETH_ALEN];
} pc_mac_t;

typedef struct pc_group {
    struct list_head head;
    char id[GROUP_ID_SIZE];
    struct list_head macs;
    pc_rule_t *rule;
    struct rcu_head rcu;
} pc_group_t;

#define LOG(level, fmt, ...) do { \
    if ((level) <= PC_LOG_LEVEL) { \
        printk(fmt, ##__VA_ARGS__); \
    } \
} while (0)

#define LLOG(level, fmt, ...) do { \
    if ((level) <= PC_LOG_LEVEL) { \
        pr_info_ratelimited(fmt, ##__VA_ARGS__); \
    } \
} while (0)


#define PC_ERROR(...)           LOG(0, ##__VA_ARGS__)
#define PC_WARN(...)            LOG(1, ##__VA_ARGS__)
#define PC_INFO(...)            LOG(2, ##__VA_ARGS__)
#define PC_DEBUG(...)           LOG(3, ##__VA_ARGS__)

#define PC_LMT_ERROR(...)       LLOG(0, ##__VA_ARGS__)
#define PC_LMT_WARN(...)        LLOG(1, ##__VA_ARGS__)
#define PC_LMT_INFO(...)        LLOG(2, ##__VA_ARGS__)
#define PC_LMT_DEBUG(...)       LLOG(3, ##__VA_ARGS__)



//HTTPS--tls
#define tls_content_type            uint8_t
#define tls_content_major_ver       uint8_t
#define tls_content_minor_ver       uint8_t
#define tls_content_length          uint16_t
#define tls_content_uint24          uint32_t        // i used 32 bit, but only copy 24 bit to value
#define tls_content_gmt             uint32_t
#define tls_cipher_suite_length     uint16_t
#define tls_session_id_length       uint8_t
#define tls_comp_mthods_length      uint8_t
#define tls_ext_length              uint16_t
#define tls_ext_type                uint16_t
#define tls_svr_list_length         uint16_t
#define tls_svr_name_type           uint8_t
#define tls_svr_name_length         uint16_t

#define GET_TLS_HDR(x, type, data, len)    \
    ((x) = *((type *)(data)), (data) += sizeof(type), (len) -= sizeof(type))
#define COPY_TLS_HDR(x, size, data, len)    \
    (memcpy((x), (data), (size)), (data) += (size), (len) -= (size))
#define SKIP_LEN(data, data_len, len)    ((data) += (len) , (data_len) -=( len))

enum {
    TLS_CT_CHANGE_CIPHER_SPEC   = 20,
    TLS_CT_ALERT                = 21,
    TLS_CT_HANDSHAKE            = 22,
    TLS_CT_APPLICATION_DATA     = 23,
};

enum {
    TLS_HANDSHAKE_HELLO_REQUEST         = 0,
    TLS_HANDSHAKE_CLIENT_HELLO          = 1,
    TLS_HANDSHAKE_SERVER_HELLO          = 2,
    TLS_HANDSHAKE_CERTIFICATE           = 11,
    TLS_HANDSHAKE_SERVER_KEY_EXCHANGE   = 12,
    TLS_HANDSHAKE_CERTIFICATE_REQUEST   = 13,
    TLS_HANDSHAKE_SERVER_HELLO_DONE     = 14,
    TLS_HANDSHAKE_CERTIFICATE_VERIFY    = 15,
    TLS_HANDSHAKE_CLIENT_KEY_EXCHANGE   = 16,
    TLS_HANDSHAKE_FINISHED              = 20,
    TLS_HANDSHAKE_MAX                   = 255,
};

#define EXTENSION_SVR_NAME_TYPE 0x0000
#define SNI_SVR_NAME_TYPE 0x0

extern int add_pc_rule(const char *id, cJSON *applist, enum pc_action action, cJSON *blist);
extern int set_pc_rule(const char *id, cJSON *applist, enum pc_action action, cJSON *blist);
extern int clean_pc_rule(void);

extern int add_pc_group(const char *id,  cJSON *macs, const char *rule_id);
extern int set_pc_group(const char *id,  cJSON *macs, const char *rule_id);
extern int clean_pc_group(void);

extern int get_rule_by_mac(u8 mac[ETH_ALEN], pc_rule_t **rules, int max_rules, enum pc_action *action);


extern int pc_register_dev(void);
extern void pc_unregister_dev(void);

extern int pc_set_app_by_str(pc_app_t *app, int appid, const char *name, const char *feature);
extern int pc_load_app_feature_list(void);
extern void pc_clean_app_feature_list(void);
extern int app_proc_show(struct seq_file *s, void *v);

extern int pc_filter_init(void);
extern void pc_filter_exit(void);

extern int regexp_match(char *reg, char *text);
extern int get_ifindex_by_ifname(const char *ifname);

#endif
