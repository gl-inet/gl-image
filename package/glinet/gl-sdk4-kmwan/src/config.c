#include <linux/inet.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <uapi/linux/if.h>
#include <linux/in6.h>
#include <linux/netdevice.h>

#include "kmwan.h"
#include "cJSON.h"

enum config_op {
    CMD_SET_BASE = 0,
    CMD_CLEAN_CFG,
    CMD_ADD_DEV,
    CMD_RM_DEV,
    CMD_ENABLE_PROBE,
    CMD_DISABLE_PROBE,
    CMD_SYNC_ROUTE,
    CMD_FORCE_DEAD,
    CMD_RESTORE_DETECT,
    CMD_MAX
};

s64 g_if_detect_time = 3000000000LL;
s64 g_offset_time = 0;
char rtmode = 0; //0:非passthrough模式 1:passthrough模式
unsigned long g_delay = HZ;
struct list_head gl_netcell_head = LIST_HEAD_INIT(gl_netcell_head);
DEFINE_RWLOCK(gl_netcell_lock);
DEFINE_RWLOCK(detect_time_lock);

static gl_net_cell_t *check_node_in_list(const char *str)
{
    gl_net_cell_t *node = NULL;

    rcu_read_lock();
    list_for_each_entry_rcu(node, &gl_netcell_head, head) {
        if (!strncmp(node->interface, str, IFNAMSIZ)) {
            rcu_read_unlock();
            return node;
        }
    }
    rcu_read_unlock();

    return NULL;
}

#if 0
/*
    @flag:
        0: print ipv4 and ipv6 node info
        1: print ipv4 node info
        2: print ipv6 node info
*/
static void print_node_info(gl_net_cell_t *node, int flag)
{
    int i;
    if ((node->addr_type == TYPE_V4 && flag == 2)
            || (node->addr_type != TYPE_V4 && flag == 1))
        return;

    printk("state:%s\ninterface:%s\n ifindex:%d\ntrack_mode:%s\ntrack_size:%d\nonline:%s\n",
           node->state == IDEL ? "IDEL" : node->state == DEAD ? "DEAD" :
           node->state == ACTIVE ? "ACTIVE" : "ERROR",
           node->interface, node->ifindex,
           node->track_mode == FORCE ? "FORCE" : "PASSIVE",
           node->track_size,
           node->online == true ? "true" : "false");
    if (node->addr_type == TYPE_V4) {
        for (i = 0; i < node->track_size; i++) {
            printk("ping,%pI4\n", &node->tracks[i].ipaddr.ip);
        }
    } else {
        for (i = 0; i < node->track_size; i++) {
            printk("ping,%pI6\n", &node->tracks[i].ipaddr.ip6);
        }
    }
}

static void print_list(void)
{
    gl_net_cell_t *node, *n;
    int flag = 0, i = 0;
    if (!list_empty(&gl_netcell_head)) {
        list_for_each_entry_safe(node, n, &gl_netcell_head, head) {
            printk("============ %d ==========\n", ++i);
            print_node_info(node, flag);
            printk("\n");
        }
    } else {
        printk("the list is empty.\n");
    }
}
#endif

static void recovery_route(gl_net_cell_t *arr, int size)
{
    int i;

    for (i = 0; i < size; i++)
        set_fib_nh(ACTIVE, &arr[i]);
}

static void netcell_rcu_free(struct rcu_head *head)
{
    struct gl_netcell *node = container_of(head, struct gl_netcell, rcu);
    free_percpu(node->stats);
    kfree(node);
}

static void cancel_kmwan_work(struct work_struct *kmwan_work[], int size)
{
    int i;

    for (i = 0; i < size; i++)
        cancel_work_sync(kmwan_work[i]);
}

void clean_netcell(void)
{
    gl_net_cell_t *node, *n;
    int cnt = 0, size = 0;
    gl_net_cell_t arr[10];
    //the frame size of 1048 bytes is larger than 1024 bytes, So using static storage
    static struct work_struct *kmwan_work[10];

    if (!g_stop_flag)
        cancel_delayed_work_sync(&poll_work);

    gl_netcell_write_lock();
    if (!list_empty(&gl_netcell_head)) {
        list_for_each_entry_safe(node, n, &gl_netcell_head, head) {
            kmwan_work[size++] = &node->work;
            list_del_rcu(&node->head);
            if (node->state == DEAD) {
                arr[cnt].addr_type = node->addr_type;
                arr[cnt].ifindex = node->ifindex;
                ++cnt;
            }
            call_rcu(&node->rcu, netcell_rcu_free);
        }
    }
    gl_netcell_write_unlock();

    cancel_kmwan_work(kmwan_work, size);
    recovery_route(arr, cnt);

    set_active_node_zero(&g_active_node);

    if (!g_stop_flag) {
        INIT_DELAYED_WORK(&poll_work, netcell_poll_work);
        mod_delayed_work(system_long_wq, &poll_work, 0);
    }
    rcu_barrier();  /* Wait for completion of call_rcu()'s */
    GL_INFO("kmwan: All nodes are deleted.\n");
}


static void remove_netcell(char *interface)
{
    gl_net_cell_t *node = NULL, *n;
    bool flag = false;
    gl_net_cell_t tmp_node;
    struct work_struct *hotpulg_work = NULL;

    gl_netcell_write_lock();
    list_for_each_entry_safe(node, n, &gl_netcell_head, head) {
        if (strncmp(node->interface, interface, IFNAMSIZ) == 0) {
            if (node->state != DEAD)
                active_node_dec(&g_active_node, node->addr_type, true);
            else {
                tmp_node.addr_type = node->addr_type;
                tmp_node.ifindex = node->ifindex;
                flag = true;
                active_node_total_dec(&g_active_node, node->addr_type);
            }

            hotpulg_work = &node->work;
            list_del_rcu(&node->head);
            GL_INFO("kmwan: Delete node:%s\n", node->interface);
            call_rcu(&node->rcu, netcell_rcu_free);
            break;
        }
    }
    gl_netcell_write_unlock();

    if (hotpulg_work != NULL)
        cancel_work_sync(hotpulg_work);

    if (flag)
        set_fib_nh(ACTIVE, &tmp_node);
}

static gl_net_cell_t *create_netcell(int track_size)
{
    gl_net_cell_t *node = NULL;
    int i;

    node = kzalloc(sizeof(gl_net_cell_t) + sizeof(gl_net_track_t) * track_size, GFP_KERNEL);
    if (node == NULL) {
        GL_ERROR("malloc gl_net_cell_t memory error\n");
        return NULL;
    }

    node->stats = alloc_percpu_gfp(struct ifstats, GFP_ATOMIC);
    if (!node->stats) {
        kfree(node);
        return NULL;
    }

    for_each_possible_cpu(i) {
        struct ifstats *st = per_cpu_ptr(node->stats, i);
        u64_stats_init(&st->syncp);
    }

    return node;
}

static void add_dev_config(cJSON *data)
{
    cJSON *cells_arr = NULL;
    cJSON *cells = NULL;
    cJSON *netdev = NULL;
    cJSON *interface = NULL;
    cJSON *addr_type = NULL;
    cJSON *track_mode = NULL;
    cJSON *tracks_arr = NULL;
    cJSON *tracks = NULL;
    cJSON *type = NULL;
    cJSON *ip = NULL;
    cJSON *force_ip = NULL;
    __be32 addr = 0;
    int i, j;

    if (!data) {
        GL_WARN("[%s %d] data is null.\n", __FUNCTION__, __LINE__);
        return;
    }

    cells_arr = cJSON_GetObjectItem(data, "cells");
    if (!cells_arr || cells_arr->type != cJSON_Array) {
        GL_WARN("[%s %d] cells is null or cells type error\n", __FUNCTION__, __LINE__);
        return;
    }

    for (i = 0; i < cJSON_GetArraySize(cells_arr); i++) {
        int track_size = 0;
        gl_net_cell_t *node = NULL;
        int ifindex;
        struct net_device *dev;
        cells = cJSON_GetArrayItem(cells_arr, i);
        if (cells) {
            netdev = cJSON_GetObjectItem(cells, "netdev");
            if (!netdev || netdev->type != cJSON_String) {
                GL_WARN("[%s %d] netdev is null or netdev type error\n", __FUNCTION__, __LINE__);
                continue;
            }

            interface = cJSON_GetObjectItem(cells, "interface");
            if (!interface || interface->type != cJSON_String) {
                GL_WARN("[%s %d] interface is null or interface type error\n", __FUNCTION__, __LINE__);
                continue;
            }

            if ((node = check_node_in_list(interface->valuestring)) != NULL) {
                ifindex = get_ifindex_by_ifname(netdev->valuestring);
                if (unlikely(ifindex == -1)) {
                    remove_netcell(node->interface);
                } else {
                    gl_netcell_write_lock();
                    node->ifindex = ifindex;
                    gl_netcell_write_unlock();
                }
                continue;
            }

            addr_type = cJSON_GetObjectItem(cells, "addr_type");
            if (!addr_type || addr_type->type != cJSON_Number) {
                GL_WARN("[%s %d] addr_type is null or addr_type type error\n", __FUNCTION__, __LINE__);
                continue;
            }

            track_mode = cJSON_GetObjectItem(cells, "track_mode");
            if (!track_mode || track_mode->type != cJSON_String) {
                GL_WARN("[%s %d] track_mode is null or track_mode type error\n", __FUNCTION__, __LINE__);
                continue;
            }

            tracks_arr = cJSON_GetObjectItem(cells, "tracks");
            if (!tracks_arr || tracks_arr->type != cJSON_Array) {
                GL_WARN("[%s %d] tracks is null or tracks type error\n", __FUNCTION__, __LINE__);
                continue;
            }

            track_size = cJSON_GetArraySize(tracks_arr);
            if (!track_size) {
                GL_WARN("[%s %d] tracks size is zero\n", __FUNCTION__, __LINE__);
                continue;
            }

            if (rtmode) {
                force_ip = cJSON_GetObjectItem(cells, "force_ip");
                if (!force_ip) {
                    GL_WARN("[%s %d] force_ip is null.\n", __FUNCTION__, __LINE__);
                    continue;
                }
                addr = in_aton(force_ip->valuestring);
            }

            node = create_netcell(track_size);
            if (!node)
                return; /* Not enough memory, return directly */

            dev = dev_get_by_name(&init_net, netdev->valuestring);
            if (dev == NULL) {
                GL_ERROR("[%s %d]get net_device '%s' failed.\n", __FUNCTION__, __LINE__, netdev->valuestring);
                free_percpu(node->stats);
                kfree(node);
                continue;
            }
            GL_INFO("[%s %d]add node success. iface:%s, dev:%s, ifindex:%d\n", __FUNCTION__, __LINE__, interface->valuestring, netdev->valuestring, dev->ifindex);

            strncpy(node->interface, interface->valuestring, IFNAMSIZ);
            strncpy(node->netdev, netdev->valuestring, IFNAMSIZ);
            node->ip = get_ip_by_netdev(dev);
            node->state = DEAD;
            node->addr_type = addr_type->valueint;
            node->ifindex = dev->ifindex;
            node->trigger_mark = 0;
            node->track_mode = strncmp(track_mode->valuestring, "passive", 7) == 0 ? PASSIVE :
                               strncmp(track_mode->valuestring, "force", 5) == 0 ? FORCE : STRICT;
            node->track_size = track_size;
            node->online = false;
            node->probe_enable = true;
            node->sync = 0;
            node->rt_chg = false;
            node->force_ip = addr;

            for (j = 0; j < track_size; j++) {
                tracks = cJSON_GetArrayItem(tracks_arr, j);
                if (!tracks || tracks->type != cJSON_Object) {
                    GL_WARN("[%s %d] tracks is null or tracks type error\n", __FUNCTION__, __LINE__);
                    continue;
                }
                type = cJSON_GetObjectItem(tracks, "type");
                ip = cJSON_GetObjectItem(tracks, "ip");
                if (!type || !ip || type->type != cJSON_String || ip->type != cJSON_String) {
                    GL_WARN("[%s %d] type or ip is null or type type or ip type error\n", __FUNCTION__, __LINE__);
                    continue;
                }
                node->tracks[j].type = strncmp(type->valuestring, "ping", 4) == 0 ? PING : PING;
                if (node->addr_type == TYPE_V4)
                    node->tracks[j].ipaddr.ip = in_aton(ip->valuestring);
                else
                    in6_pton(ip->valuestring, -1, (void *)&node->tracks[j].ipaddr.ip6.s6_addr, -1, NULL);
            }

            dev_put(dev);
            gl_netcell_write_lock();
            list_add_rcu(&node->head, &gl_netcell_head);
            gl_netcell_write_unlock();

            active_node_total_inc(&g_active_node, addr_type->valueint);
            set_fib_nh(ACTIVE, node);//确保节点初始路由状态是active
            kmwan_hotplug_init(node);
            kmwan_hotplug(node);
        }
    }
}

//"{"op":0,"data":{"sensitivity":1000, "mode":"failover"}}"
static void set_base_config(cJSON *data)
{
    cJSON *mode = NULL;
    cJSON *sensitivity = NULL;
    cJSON *rt_mode = NULL;
    gl_net_cell_t *node;

    if (!data) {
        GL_ERROR("[%s %d] data is null.\n", __FUNCTION__, __LINE__);
        return;
    }

    mode = cJSON_GetObjectItem(data, "mode");
    if (!mode) {
        GL_ERROR("[%s %d] mode is null.\n", __FUNCTION__, __LINE__);
        return;
    }

    sensitivity = cJSON_GetObjectItem(data, "sensitivity");
    if (!sensitivity) {
        GL_ERROR("[%s %d] sensitivity is null.\n", __FUNCTION__, __LINE__);
        return;
    }

    rt_mode = cJSON_GetObjectItem(data, "rtmode");
    if (!rt_mode) {
        GL_ERROR("[%s %d] rtmode is null.\n", __FUNCTION__, __LINE__);
        return;
    }

    rtmode = rt_mode->valueint;

    write_lock_bh(&detect_time_lock);
    g_if_detect_time = (s64)sensitivity->valueint * 1000000ULL;
    /* 一个探测周期(g_if_detect_time)发送三次心跳包 计算心跳包ttl，除以3000是因为上层传下来的时间单位是ms*/
    g_delay = (unsigned long)(sensitivity->valueint * HZ / 3000);
    /* 计算发送3次心跳包所用时间与探测周期的差值 */
    g_offset_time = g_if_detect_time - (s64)((g_delay * 3000 / HZ) * 1000000ULL);
    write_unlock_bh(&detect_time_lock);

    rcu_read_lock_bh();
    list_for_each_entry_rcu(node, &gl_netcell_head, head) {
        set_new_period(node);
    }
    rcu_read_unlock_bh();
    GL_INFO("kmwan: sensitivity and mode are set.\n");
}

static void parse_probe_cfg(char *interface, int op)
{
    gl_net_cell_t *node = NULL;
    bool probe_enable = (op == CMD_ENABLE_PROBE);
    bool enable_rt = false;

    gl_netcell_write_lock();
    list_for_each_entry(node, &gl_netcell_head, head) {
        if (!strncmp(node->interface, interface, IFNAMSIZ)) {
            node->probe_enable = probe_enable;
            if (!probe_enable) {
                node->online = !(node->state == DEAD);
                enable_rt = node->state == DEAD && node->sync == 1;
                node->rt_chg = enable_rt;
            }
            break;
        }
    }
    gl_netcell_write_unlock();

    if (enable_rt) {
        GL_DEBUG("kmwan: Restoring route.\n");
        set_fib_nh(ACTIVE, node);
    }
}

static void force_dead_cfg(char *interface)
{
    gl_net_cell_t *node = NULL;
    gl_net_cell_t tmp_node;
    bool found = false;
    bool need_route = false;
    bool need_hotplug = false;

    gl_netcell_write_lock();
    list_for_each_entry(node, &gl_netcell_head, head) {
        if (!strncmp(node->interface, interface, IFNAMSIZ)) {
            found = true;
            node->force_dead = true;

            if (node->state == ACTIVE) {
                active_node_dec(&g_active_node, node->addr_type, false);
                if (get_active_node_num(&g_active_node, node->addr_type) > 0) {
                    node->sync = 1;
                    need_route = true;
                } else {
                    node->sync = 0;
                }
            }

            if (node->state != DEAD || node->online)
                need_hotplug = true;

            node->state = DEAD;
            node->online = false;
            set_new_period(node);

            tmp_node.addr_type = node->addr_type;
            tmp_node.ifindex = node->ifindex;

            if (need_hotplug)
                kmwan_hotplug(node);

            break;
        }
    }
    gl_netcell_write_unlock();

    if (!found)
        return;

    if (need_route)
        set_fib_nh(DEAD, &tmp_node);

    GL_INFO("kmwan: %s force dead.\n", interface);
}

static void restore_detect_cfg(char *interface)
{
    gl_net_cell_t *node = NULL;
    bool found = false;

    gl_netcell_write_lock();
    list_for_each_entry(node, &gl_netcell_head, head) {
        if (!strncmp(node->interface, interface, IFNAMSIZ)) {
            found = true;
            node->force_dead = false;
            set_new_period(node);
            break;
        }
    }
    gl_netcell_write_unlock();

    if (found)
        GL_INFO("kmwan: %s restore detect.\n", interface);
}

static void cells_handles(cJSON *data, int op)
{
    cJSON *cells_arr = NULL;
    cJSON *cells = NULL;
    int i;

    if (!data) {
        GL_WARN("[%s %d] data is null.\n", __FUNCTION__, __LINE__);
        return;
    }

    cells_arr = cJSON_GetObjectItem(data, "cells");
    if (!cells_arr) {
        GL_WARN("cells obj is null\n");
        return;
    }

    for (i = 0; i < cJSON_GetArraySize(cells_arr); i++) {
        cells = cJSON_GetArrayItem(cells_arr, i);
        if (!cells) {
            continue;
        }
        switch (op) {
            //{"op":3,"data":{"cells":["wan","wwan"]}}
            case CMD_RM_DEV:
                remove_netcell(cells->valuestring);
                break;
            case CMD_ENABLE_PROBE:
            case CMD_DISABLE_PROBE:
                parse_probe_cfg(cells->valuestring, op);
                break;
            case CMD_FORCE_DEAD:
                force_dead_cfg(cells->valuestring);
                break;
            case CMD_RESTORE_DETECT:
                restore_detect_cfg(cells->valuestring);
                break;
            default:
                break;
        }
    }
}

static void sync_route(void)
{
    gl_net_cell_t *node = NULL;

    gl_netcell_write_lock();
    list_for_each_entry(node, &gl_netcell_head, head) {
        if (node->state == DEAD) {
            node->rt_chg = true;
            node->sync = 1;
        }
    }
    gl_netcell_write_unlock();
}

void config_handle(char *json, size_t len)
{
    cJSON *root = NULL;
    cJSON *op = NULL;
    cJSON *data = NULL;

    if (!json || !len) {
        GL_WARN("[%s %d] json or size is invaild\n", __FUNCTION__, __LINE__);
        return;
    }

    root = cJSON_Parse(json);
    if (!root) {
        GL_WARN("[%s %d] json is null\n", __FUNCTION__, __LINE__);
        return;
    }

    op = cJSON_GetObjectItem(root, "op");
    if (!op || op->type != cJSON_Number) {
        GL_WARN("[%s %d] not find op object or op type is invaild\n", __FUNCTION__, __LINE__);
        goto out;
    }

    switch (op->valueint) {
        case CMD_SET_BASE:
            data = cJSON_GetObjectItem(root, "data");
            //The detection of 'data' is done in the function it executes
            set_base_config(data);
            break;
        case CMD_ADD_DEV:
            data = cJSON_GetObjectItem(root, "data");
            add_dev_config(data);
            break;
        case CMD_RM_DEV:
        case CMD_ENABLE_PROBE:
        case CMD_DISABLE_PROBE:
        case CMD_FORCE_DEAD:
        case CMD_RESTORE_DETECT:
            data = cJSON_GetObjectItem(root, "data");
            cells_handles(data, op->valueint);
            break;
        case CMD_CLEAN_CFG:
            clean_netcell();
            break;
        case CMD_SYNC_ROUTE:
            sync_route();
            break;
        default:
            GL_WARN("[%s %d] the op:%d is invalid\n", __FUNCTION__, __LINE__, op->valueint);
    }

out:
    cJSON_Delete(root);
}
