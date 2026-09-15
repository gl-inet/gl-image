#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdbool.h>
#include <uci.h>

#include "guci.h"

enum {
    /* section cmds */
    CMD_GET,
    CMD_SET,
    CMD_ADD_LIST,
    CMD_DEL_LIST,
    CMD_DEL,
    CMD_RENAME,
    CMD_REVERT,
    CMD_REORDER,
    /* package cmds */
    CMD_SHOW,
    CMD_CHANGES,
    CMD_EXPORT,
    CMD_COMMIT,
    /* other cmds */
    CMD_ADD,
    CMD_IMPORT,
    CMD_HELP,
};


#define safe_free(__x) do { if (__x) { free((void *)__x); __x = NULL;}} while(0)
#define MAX_LENGTH 1024

struct uci_type_list {
    unsigned int idx;
    const char *name;
    struct uci_type_list *next;
};

void uci2_reset_typelist()
{
}

static void uci_show_value(struct uci_option *o, char value[], int len)
{
    struct uci_element *e;

    switch (o->type) {
        case UCI_TYPE_STRING:
            snprintf(value, len, "%s", o->v.string);
            break;
        case UCI_TYPE_LIST:
            uci_foreach_element(&o->v.list, e) {
                strncat(value, e->name, len - strlen(value) - 1);
                strncat(value, " ", len - strlen(value) - 1);
            }
            int value_len = strlen(value);
            value[value_len - 1] = 0;
            break;
        default:
            value[0] = '\0';
            break;
    }
}

struct uci_context *guci_init()
{
    struct uci_context *ctx = uci_alloc_context();

    return ctx;
}

int guci_free(struct uci_context *ctx)
{
    uci_free_context(ctx);
    return 0;
}

int guci_rename(struct uci_context *ctx, const char *section_or_key, char value[])
{
    if (value == NULL)
        return -1;
    struct uci_ptr ptr;
    int ret = UCI_OK;
    char *str = (char *)malloc(strlen(section_or_key) + strlen(value) + 3); //must not use a const value

    //FIXME: deal with ' in value
    snprintf(str, strlen(section_or_key) + strlen(value) + 3, "%s=%s", section_or_key, value);
    if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
        ret = -1;
        goto out;
    }

    ret = uci_rename(ctx, &ptr);
    /* save changes, but don't commit them yet */
    if (ret == UCI_OK)
        ret = uci_save(ctx, ptr.p);

out:
    free(str);
    return ret;
}

int guci_get(struct uci_context *ctx, const char *section_or_key, char value[], int len)
{
    struct uci_ptr ptr;
    struct uci_element *e;
    int ret = UCI_OK;
    char *str = (char *)calloc(1, strlen(section_or_key) + 1); //must not use a const value
    strncpy(str, section_or_key, strlen(section_or_key));
    if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
        ret = -1;
        value[0] = '\0';
        goto out;
    }
    if (!(ptr.flags & UCI_LOOKUP_COMPLETE)) {
        ctx->err = UCI_ERR_NOTFOUND;
        ret = -1;
        value[0] = '\0';
        goto out;
    }
    e = ptr.last;
    switch (e->type) {
        case UCI_TYPE_SECTION:
            snprintf(value, len, "%s", ptr.s->type);
            break;
        case UCI_TYPE_OPTION:
            uci_show_value(ptr.o, value, len);
            break;
        default:
            value[0] = '\0';
            ret = -1;
            goto out;
            break;
    }
out:
    free(str);
    return ret;
}


/**
 * guci_get_idx("wireless.@wifi-iface",0,ssid, value)
 */
int guci_get_idx(struct uci_context *ctx, const char *section, int index, const char *key, char value[], int len)
{
    char s[MAX_LENGTH] = {0};
    if (key != NULL)
        snprintf(s, MAX_LENGTH, "%s[%d].%s", section, index, key);
    else
        snprintf(s, MAX_LENGTH, "%s[%d]", section, index);
    return guci_get(ctx, s, value, len);
}

int guci_get_name(struct uci_context *ctx, const char *section, const char *name, const char *key, char value[], int len)
{
    char s[MAX_LENGTH] = {0};
    if (key != NULL)
        snprintf(s, MAX_LENGTH, "%s.%s.%s", section, name, key);
    else
        snprintf(s, MAX_LENGTH, "%s.%s", section, name);
    return guci_get(ctx, s, value, len);
}

int guci_set(struct uci_context *ctx, const char *section_or_key, const char *value)
{

    if (value == NULL) return -1;
    struct uci_ptr ptr;
    int ret = UCI_OK;
    char *str = (char *)malloc(strlen(section_or_key) + strlen(value) + 3); //must not use a const value
    if (!str) {
        return -2;
    }

    //FIXME: deal with ' in value
    snprintf(str, strlen(section_or_key) + strlen(value) + 3, "%s=%s", section_or_key, value);
    if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
        ret = -3;
        goto out;
    }

    ret = uci_set(ctx, &ptr);
    /* save changes, but don't commit them yet */
    if (ret == UCI_OK)
        ret = uci_save(ctx, ptr.p);

out:
    free(str);
    return ret;
}

/**
 * guci_set_idx("wireless.@wifi-iface", 0, "ssid", "something");
 */
int guci_set_idx(struct uci_context *ctx, const char *section, int index, const char *key, char *value)
{
    char s[MAX_LENGTH] = {0};
    if (key != NULL)
        snprintf(s, MAX_LENGTH, "%s[%d].%s", section, index, key);
    else
        snprintf(s, MAX_LENGTH, "%s[%d]", section, index);
    return guci_set(ctx, s, value);
}

int guci_set_name(struct uci_context *ctx, const char *section, const char *name, const char *key, const char *value)
{
    char s[MAX_LENGTH] = {0};
    if (key != NULL)
        snprintf(s, MAX_LENGTH, "%s.%s.%s", section, name, key);
    else
        snprintf(s, MAX_LENGTH, "%s.%s", section, name);
    return guci_set(ctx, s, value);
}

int guci_commit(struct uci_context *ctx, const char *config)
{
    int ret = 1;
    struct uci_ptr ptr;
    char *str = (char *)calloc(1, strlen(config) + 1); //must not use a const value
    strncpy(str, config, strlen(config));
    if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
        free(str);
        return 1;
    }

    if (uci_commit(ctx, &ptr.p, false) != UCI_OK) {
        goto out;
    }
    ret = 0;
out:
    free(str);
    return ret;
}

int guci_add_list(struct uci_context *ctx, char *key, char *value)
{
    int ret = -1;
    struct uci_ptr ptr;
    char *str = (char *)malloc(strlen(key) + strlen(value) + 2);
    if (str == NULL) {
        return -1;
    }
    snprintf(str, strlen(key) + strlen(value) + 2, "%s=%s", key, value);
    if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
        ret = -1;
        goto out;
    }
    ret = uci_add_list(ctx, &ptr); //return 0 success

out:
    free(str);
    return ret;
}

int guci_delete(struct uci_context *ctx, const char *key)
{
    struct uci_ptr ptr;
    int ret = 0;
    char *str = (char *)calloc(1, strlen(key) + 1); //must not use a const value
    strncpy(str, key, strlen(key));
    if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
        ret = -1;
        goto out;
    }
    ret = uci_delete(ctx, &ptr);
    /* save changes, but don't commit them yet */
    if (ret == UCI_OK)
        ret = uci_save(ctx, ptr.p);

out:
    free(str);
    return ret;
}

int guci_delete_name(struct uci_context *ctx, const char *section, const char *name, const char *key)
{
    char s[MAX_LENGTH] = {0};
    if (key != NULL)
        snprintf(s, MAX_LENGTH, "%s.%s.%s", section, name, key);
    else
        snprintf(s, MAX_LENGTH, "%s.%s", section, name);
    return guci_delete(ctx, s);
}

int guci_delete_list_value(struct uci_context *ctx, char *key, char *value)
{
    int ret = -1;
    struct uci_ptr ptr;
    char *str = (char *)malloc(strlen(key) + strlen(value) + 2);
    snprintf(str, strlen(key) + strlen(value) + 2, "%s=%s", key, value);
    if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
        ret = -1;
        goto out;
    }
    ret = uci_del_list(ctx, &ptr); //return 0 success

out:
    free(str);
    return ret;
}

int guci_add(struct uci_context *ctx, const char *section, const char *type)
{
    return guci_set(ctx, section, type);
}

int guci_add_anonymous(const char *config, const char *session)
{
    int ret = 0;

    struct uci_context *ctx = NULL;
    struct uci_package *pkg = NULL;

    ctx = uci_alloc_context();
    if (!ctx) {
        return -1;
    }

    ret = uci_load(ctx, config, &pkg);
    if (ret != UCI_OK) {
        goto out;
    }

    pkg = uci_lookup_package(ctx, config);
    if (pkg) {
        struct uci_ptr ptr = {
            .p = pkg
        };

        uci_add_section(ctx, pkg, session, &ptr.s);

        uci_commit(ctx, &ptr.p, false);
        uci_unload(ctx, ptr.p);
    } else {
        ret = -1;
    }

out:
    if (ctx) {
        uci_free_context(ctx);
    }

    return ret;
}


/**
 * guci_section_count("wireless.@wifi-iface")
 */
int guci_section_count(struct uci_context *ctx, const char *section_type)
{
    struct uci_element *e;
    struct uci_ptr ptr;
    int ret = 0;
    char str[MAX_LENGTH] = {0}, str1[MAX_LENGTH] = {0};

    snprintf(str1, MAX_LENGTH, "%s", section_type);

    char *end;
    char *section = strtok_r(str1, ".@", &end);
    char *type = strtok_r(NULL, ".@", &end);
    snprintf(str, MAX_LENGTH, "%s", section);

    struct uci_package *p = NULL;

    if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
        ret = 0;
        goto out;
    }

    uci2_reset_typelist();
    p = ptr.p;

    uci_foreach_element(&p->sections, e) {
        struct uci_section *s = uci_to_section(e);
        if (strcmp(type, s->type) == 0) ret++;
    }
    uci2_reset_typelist();

out:
    return ret;
}

/**
 * get the section name by index
 * guci_section_name("wireless.@wifi-iface",0)
 * @return section name, e.g. "public"
 */
char *guci_section_name(struct uci_context *ctx, const char *section_type, int index)
{
    char full_type[MAX_LENGTH] = {0};
    snprintf(full_type, MAX_LENGTH, "%s[%d]", section_type, index);

    struct uci_ptr ptr;
    struct uci_section *s = NULL;
    char *name = NULL;

    if (uci_lookup_ptr(ctx, &ptr, full_type, true) != UCI_OK) {
        goto out;
    }

    s = ptr.s;
    name = s->e.name;

out:
    return name;
}
/**
 * guci_find_section("firewall.@zone.name","wan")
 */
char *guci_find_section(struct uci_context *ctx, const char *section_key, char *value)
{
    struct uci_element *e;
    struct uci_element *e1;
    struct uci_ptr ptr;
    char *ret = NULL;
    char str[MAX_LENGTH] = {0}, str1[MAX_LENGTH] = {0};

    snprintf(str1, MAX_LENGTH, "%s", section_key);

    char *end;
    char *section = strtok_r(str1, ".@", &end);
    char *type = strtok_r(NULL, ".@", &end);
    char *key = strtok_r(NULL, ".", &end);

    snprintf(str, MAX_LENGTH, "%s", section);

    struct uci_package *p = NULL;

    if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
        ret = NULL;
        goto out;
    }

    uci2_reset_typelist();
    p = ptr.p;

    uci_foreach_element(&p->sections, e) {
        struct uci_section *s = uci_to_section(e);

        if (strcmp(type, s->type) == 0) {
            //fixme: this maybe wrong because of typelist
            uci_foreach_element(&s->options, e1) {
                if (strcmp(key, e1->name) == 0) {
                    char key_value[128] = {0};
                    struct uci_option *o = uci_to_option(e1);
                    uci_show_value(o, key_value, sizeof(key_value));
                    if (strcmp(key_value, value) == 0) {
                        ret = e->name;
                        goto out;
                    }
                }
            }
        }
    }
    uci2_reset_typelist();

out:
    return ret;
}

char *guci_find_list_member(struct uci_context *ctx, const char *section_key, char *value)
{
    char *ret = NULL;
    struct uci_ptr ptr;
    struct uci_element *e;
    char tmp_v[2048] = {0};
    char *str = (char *)malloc(strlen(section_key) + 1);
    memcpy(str, section_key, strlen(section_key) + 1);
    if (UCI_OK == guci_get(ctx, str, tmp_v, sizeof(tmp_v))) {
        if (UCI_OK == uci_lookup_ptr(ctx, &ptr, str, true)) {
            struct uci_option *o = ptr.o;
            switch (o->type) {
                case UCI_TYPE_STRING:
                    break;
                case UCI_TYPE_LIST:
                    uci_foreach_element(&o->v.list, e) {
                        if (strcmp(e->name, value) == 0) {
                            ret = e->name;
                            goto out;
                        }
                    }
                    break;
            }
        }
    }
out:
    free(str);
    return ret;
}

char *guci_get_list_index(struct uci_context *ctx, const char *section_key, int index, char *buf, int len)
{
    char *ret = NULL;
    struct uci_ptr ptr;
    struct uci_element *e;
    char tmp_v[2048] = {0};
    int i = 0;
    char *str = (char *)malloc(strlen(section_key) + 1);
    memcpy(str, section_key, strlen(section_key) + 1);
    if (UCI_OK == guci_get(ctx, str, tmp_v, sizeof(tmp_v))) {
        if (UCI_OK == uci_lookup_ptr(ctx, &ptr, str, true)) {
            struct uci_option *o = ptr.o;
            switch (o->type) {
                case UCI_TYPE_STRING:
                    break;
                case UCI_TYPE_LIST:
                    uci_foreach_element(&o->v.list, e) {
                        if (i == index) {
                            snprintf(buf, len, "%s", e->name);
                            goto out;
                        }
                        i++;
                    }
                    break;
            }
        }
    }
out:
    free(str);
    return ret;
}

int guci_get_list_count(struct uci_context *ctx, const char *section_key)
{
    struct uci_ptr ptr;
    struct uci_element *e;
    char tmp_v[2048] = {0};
    int i = 0;
    char *str = (char *)malloc(strlen(section_key) + 1);
    memcpy(str, section_key, strlen(section_key) + 1);
    if (UCI_OK == guci_get(ctx, str, tmp_v, sizeof(tmp_v))) {
        if (UCI_OK == uci_lookup_ptr(ctx, &ptr, str, true)) {
            struct uci_option *o = ptr.o;
            switch (o->type) {
                case UCI_TYPE_STRING:
                    break;
                case UCI_TYPE_LIST:
                    uci_foreach_element(&o->v.list, e) {
                        i++;
                    }
                    break;
            }
        }
    }

    free(str);
    return i;
}
