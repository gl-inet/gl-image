#ifndef _GUCI_H
#define _GUCI_H

#ifdef __cplusplus
"C" {
#endif

    /**
     * Init and free guci context
     * guci_init()
     * guci_free()
     */
    struct uci_context * guci_init();
    int guci_free(struct uci_context * ctx);

    /**
     * guci_set("wireless.public.ssid",ssid)
     * guci_set("wireless.guest","wifi-iface")
     */
    int guci_set(struct uci_context * ctx, const char *key, const char *value);
    int guci_set_idx(struct uci_context * ctx, const char *section, int index, const char *key, char *value);
    int guci_set_name(struct uci_context * ctx, const char *section, const char *name, const char *key, const char *value);

    /**
     * guci_get("wireless.public.ssid", ssid)
     * return UCI_OK when success
     */
    int guci_get(struct uci_context * ctx, const char *section_or_key, char value[], int len);
    int guci_get_idx(struct uci_context * ctx, const char *section, int index, const char *key, char value[], int len);
    int guci_get_name(struct uci_context * ctx, const char *section, const char *name, const char *key, char value[], int len);

    /**
     * guci_get("wireless.public", private)
     * return UCI_OK when success
     */
    int guci_rename(struct uci_context * ctx, const char *section_or_key, char value[]);

    /**
     * guci_commit("wireless")
     */
    int guci_commit(struct uci_context * ctx, const char *config);

    /**
      * guci_delete("wireless.public.key")
      * guci_delete("wireless.public")
     */
    int guci_delete(struct uci_context * ctx, const char *section_or_key);

    /**
     * guci_delete_name(ctx,"wireless", "public", "key")
     * guci_delete_name(ctx, "wireless", "public", NULL)
    */
    int guci_delete_name(struct uci_context * ctx, const char *section, const char *name, const char *key);

    /**
     * guci_delete_section_index("wireless.interface",0)
     */
    int guci_delete_section_index(struct uci_context * ctx, char *section, int index);

    /**
     * guci_add("wireless.public", "interface")
     */
    int guci_add(struct uci_context * ctx, const char *section, const char *type);

    int guci_add_anonymous(const char *config, const char *session);
    /**
     * guci_add_list("network.wan.dns", "192.168.1.1")
     */
    int guci_add_list(struct uci_context * ctx, char *key, char *value);

    /**
     * guci_delete_list_value("network.wan.dns","192.168.1.1")
     */
    int guci_delete_list_value(struct uci_context * ctx, char *key, char *value);

    /**
     * guci_section_count("wireless.@wifi-iface");
     * @return 2, number of wifi-iface's
     */
    int guci_section_count(struct uci_context * ctx, const char *section_type);

    /**
     * get the section name by index
     * guci_section_name("wireless.wifi-iface",0)
     * @return section name, e.g. "public"
     */
    char *guci_section_name(struct uci_context * ctx, const char *section_type, int index);

    /**
     * guci_find_section("wireless.@wifi-iface.mode", "ap");
     * @return section name, e.g. "public"
     */
    char *guci_find_section(struct uci_context * ctx, const char *section_key, char *value);
    /**
     * guci_find_section(ctx,"wireless.@wifi-iface.mac", "e4:95:6e:24:30:25");
     * @return section name, e.g. "public"
     */
    char *guci_find_list_member(struct uci_context * ctx, const char *section_key, char *value);
    /**
     * guci_get_list_index(ctx,"wireless.@wifi-iface.mac", 1);
     * @return section name, e.g. "public"
     */
    char *guci_get_list_index(struct uci_context * ctx, const char *section_key, int index, char *buf, int len);

    int guci_get_list_count(struct uci_context * ctx, const char *section_key);

#ifdef __cplusplus
}
#endif

#endif
