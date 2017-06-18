#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <gweb/config.h>

#include "json-c/json.h"
#include "libs3.h"

#define CFG_BUFSZ    (4096)

#define log printf

struct config_text {
    char config_buffer[CFG_BUFSZ];
    int  config_loaded;

    struct json_object *root;
};

static struct config_text g_config;

struct mysql_config *config_load_mysqldb (void)
{
    struct mysql_config *mysql;
    struct json_object *root = g_config.root, *obj, *elem, *cfgnode;
    const char *ptr;
    int count, idx, found_mysql_cfg = 0;

    if (!json_object_object_get_ex(root, "db_config", &obj)) {
        log("no database config nodes found!\n");
        return NULL;
    }

    if (json_object_get_array(obj) == NULL) {
        log("invalid storage format for database node!\n");
        return NULL;
    }

    count = json_object_array_length(obj);
    for (idx = 0; idx < count; idx++) {
        elem = json_object_array_get_idx(obj, idx);
        if (!elem) {
            break;
        }
        if (json_object_object_get_ex(elem, "type", &cfgnode)) {
            ptr = json_object_get_string(cfgnode);
            if (strcasecmp(ptr, "mysql") == 0) {
                found_mysql_cfg = 1;
                break;
            }
        }
    }

    if (!found_mysql_cfg) {
        log("no mysql db config found!\n");
        return NULL;
    }

    mysql = calloc(sizeof(struct mysql_config), 1);
    if (mysql == NULL) {
        log("memory allocation failed!\n");
        return NULL;
    }

    if (json_object_object_get_ex(elem, "host", &cfgnode)) {
        ptr = json_object_get_string(cfgnode);
        mysql->host = strndup(ptr, strlen(ptr));
    }

    if (json_object_object_get_ex(elem, "username", &cfgnode)) {
        ptr = json_object_get_string(cfgnode);
        mysql->username = strndup(ptr, strlen(ptr));
    }

    if (json_object_object_get_ex(elem, "password", &cfgnode)) {
        ptr = json_object_get_string(cfgnode);
        mysql->password = strndup(ptr, strlen(ptr));
    }

    if (json_object_object_get_ex(elem, "database", &cfgnode)) {
        ptr = json_object_get_string(cfgnode);
        mysql->database = strndup(ptr, strlen(ptr));
    }

    return mysql;
}

static int
config_load_avatardb_cache (struct avatardb_config *cfg, struct json_object *elem)
{
    struct json_object *cfgnode;
    const char *ptr;

    if (json_object_object_get_ex(elem, "location", &cfgnode)) {
        ptr = json_object_get_string(cfgnode);
        cfg->loc_cache = strndup(ptr, strlen(ptr));
        return 0;
    }

    log("no location information for avatar-cache!\n");
    return -1;
}

static int
config_load_avatardb_fuse_mountpoint (struct avatardb_config *cfg,
                                      struct json_object *elem)
{
    struct json_object *cfgnode;
    const char *ptr;

    if (json_object_object_get_ex(elem, "location", &cfgnode)) {
        ptr = json_object_get_string(cfgnode);
        cfg->loc_fmount = strndup(ptr, strlen(ptr));
        return 0;
    }

    log("no location information for avatar-s3-fuse!\n");
    return -1;
}

static int
config_load_avatardb_s3 (struct avatardb_config *cfg, struct json_object *elem)
{
    S3BucketContext *s3ctx;
    struct json_object *cfgnode;
    const char *ptr;

    if (!(s3ctx = calloc(sizeof(S3BucketContext), 1))) {
        log("memory allocation failed!\n");
        return -1;
    }

    if (json_object_object_get_ex(elem, "bucket", &cfgnode)) {
        ptr = json_object_get_string(cfgnode);
        s3ctx->bucketName = strndup(ptr, strlen(ptr));
    }

    if (json_object_object_get_ex(elem, "access-key-id", &cfgnode)) {
        ptr = json_object_get_string(cfgnode);
        s3ctx->accessKeyId = strndup(ptr, strlen(ptr));
    }

    if (json_object_object_get_ex(elem, "secret", &cfgnode)) {
        ptr = json_object_get_string(cfgnode);
        s3ctx->secretAccessKey = strndup(ptr, strlen(ptr));
    }

    if (json_object_object_get_ex(elem, "region", &cfgnode)) {
        ptr = json_object_get_string(cfgnode);
        s3ctx->authRegion = strndup(ptr, strlen(ptr));
    }

    if (json_object_object_get_ex(elem, "host", &cfgnode)) {
        ptr = json_object_get_string(cfgnode);
        s3ctx->hostName = strndup(ptr, strlen(ptr));
    }

    if (json_object_object_get_ex(elem, "folder", &cfgnode)) {
        ptr = json_object_get_string(cfgnode);
        cfg->folder = strndup(ptr, strlen(ptr));
    }

    s3ctx->protocol = S3ProtocolHTTPS;
    s3ctx->uriStyle = S3UriStyleVirtualHost;

    cfg->s3ctx = s3ctx;

    return 0;
}

struct avatardb_config *config_load_avatardb (void)
{
    struct json_object *obj, *elem, *cfgnode, *root = g_config.root;
    struct avatardb_config *cfg;
    const char *ptr;
    int count, idx;

    if (!json_object_object_get_ex(root, "avatar_storage", &obj)) {
        log("no avatar storage node found!\n");
        return NULL;
    }

    if (json_object_get_array(obj) == NULL) {
        log("invalid avatar storage node!\n");
        return NULL;
    }

    if ((cfg = calloc(sizeof(struct avatardb_config), 1)) == NULL) {
        log("allocating memory failed!\n");
        return NULL;
    }

    count = json_object_array_length(obj);
    for (idx = 0; idx < count; idx++) {
        if (!(elem = json_object_array_get_idx(obj, idx)))
            break;
        if (json_object_object_get_ex(elem, "type", &cfgnode)) {
            ptr = json_object_get_string(cfgnode);
            if (strcasecmp(ptr, "aws-s3") == 0) {
                config_load_avatardb_s3(cfg, elem);

            } else if (strcasecmp(ptr, "aws-s3-fuse") == 0) {
                config_load_avatardb_fuse_mountpoint(cfg, elem);

            } else if (strcasecmp(ptr, "cache") == 0) {
                config_load_avatardb_cache(cfg, elem);
            }
        }
    }

    return cfg;
}

int config_load_dotrc (const char *rc_file)
{
    int fd;

    if ((fd = open(rc_file, O_RDONLY)) < 0) {
        log("opening config file failed!\n");
        return -1;
    }

    g_config.config_loaded = 0;

    if (read(fd, g_config.config_buffer, CFG_BUFSZ) <= 0) {
        log("config file read error!\n");
        return -1;
    }

    close(fd);

    /* Parse JSON */
    if ((g_config.root = json_tokener_parse(g_config.config_buffer)) == NULL) {
        log("config file format error!\n");
        return -1;
    }

    g_config.config_loaded = 1;
        
    return 0;
}

#define MAX_CONFIG_FILE_LEN   256

int config_parse_and_load (int argc, char *argv[])
{
    char *buf, rc_file[MAX_CONFIG_FILE_LEN];
    char *env_val;
    int len = 0;

    buf = rc_file;
    if ((env_val = getenv("GWEBRC_CONFIG")) != NULL) {
        return config_load_dotrc(env_val);
    } else {
        if ((env_val = getenv("HOME")) != NULL) {
            len = snprintf(buf+len, MAX_CONFIG_FILE_LEN, "%s/", env_val);
        }
        if (MAX_CONFIG_FILE_LEN-len < 8)  {
            return -1;
        }
        len += snprintf(buf+len, MAX_CONFIG_FILE_LEN-len, ".gwebrc");
        buf[len] = '\0';
    }
    return config_load_dotrc(buf);
}
