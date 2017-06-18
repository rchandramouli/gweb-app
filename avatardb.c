/*
 * Handle storing of Avatar images
 *
 * At present, we fuse mount S3 bucket and when the multipart upload
 * arrives, save a copy in local cache and then copy the file to S3
 * mounted partition. This needs to be revisited, using libs3 later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>

#include <gweb/common.h>
#include <gweb/config.h>
#include <gweb/json_api.h>
#include <gweb/mysqldb_api.h>

#include "json-c/json.h"
#include "libs3.h"

#define MAX_UID_LEN           (16)
#define MAX_FILENAME_LEN      (1024) /* including the path */
#define MAX_COPY_BLKSZ        (4096)
#define MAX_AVATAR_URL_LEN    (128)

enum {
    IMAGE_ENCODE_RAW,
    IMAGE_ENCODE_BASE64,
};

/*
 * File upload private data
 */
struct http_upload_avatar {
    char id[MAX_UID_LEN];
    int  image_fd;
    int  encoding;
    int  data_size;
    int  valid;
};

struct avatardb_config *g_avatardb_cfg;

static int
save_data_block (struct http_upload_avatar *meta, const char *data,
                 uint64_t off, size_t size)
{
    if (lseek(meta->image_fd, off, SEEK_SET) != off) {
        log_debug("seek to fd (%d) for block (%"PRIu64", %zd) failed!\n",
                  meta->image_fd, off, size);
        return -1;
    }
    if (write(meta->image_fd, data, size) != size) {
        log_debug("writing fd (%d) block (%"PRIu64", %zd) failed!\n",
                  meta->image_fd, off, size);
        return -1;
    }

    meta->data_size += size;

    return 0;
}

/* Copy all blocks from one fd to another till EOF. Assumes the FD is
 * lseek capable.
 */
static int copy_fd_blocks (int fd_from, int fd_to)
{
    char block[MAX_COPY_BLKSZ];
    int len;

    lseek(fd_from, 0, SEEK_SET);
    lseek(fd_to, 0, SEEK_SET);

    do {
        if ((len = read(fd_from, block, MAX_COPY_BLKSZ)) < 0) {
            log_debug("[copy] from fd: %d, to fd: %d failed! (%s)\n",
                      fd_from, fd_to, strerror(errno));
            return -1;
        }

        if (len == 0)
            break;

        if (write(fd_to, block, len) != len) {
            log_debug("[copy] block from fd: %d to fd: %d (size: %d) failed! (%s)\n",
                      fd_from, fd_to, len, strerror(errno));
            return -1;
        }
    } while (1);

    return 0;
}

static int
get_image_fd (struct http_upload_avatar *meta, int cache)
{
    char avfile[MAX_FILENAME_LEN];
    int fd;

    if (cache) {
        snprintf(avfile, MAX_FILENAME_LEN, "%s/av_%s.dat",
                 g_avatardb_cfg->loc_cache,
                 meta->id);
    } else {
        snprintf(avfile, MAX_FILENAME_LEN, "%s/%s/av_%s.dat",
                 g_avatardb_cfg->loc_fmount,
                 g_avatardb_cfg->folder,
                 meta->id);
    }

    fd = open(avfile, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        log_debug("cannot open avatar file @ %s (%s)\n",
                  avfile, strerror(errno));
        return -1;
    }

    return fd;
}

int
avatardb_handle_upload_cleanup (void **priv)
{
    struct http_upload_avatar *meta;

    if (priv && *priv) {
        meta = *priv;

        if (meta->image_fd >= 0) {
            close(meta->image_fd);
            meta->image_fd = -1;
        }
        free(meta);
        *priv = NULL;
    }

    return 0;
}

int
avatardb_handle_upload_complete (void **priv, char **response, int *status)
{
    int s3_fd;
    char json_msg[256];
    struct http_upload_avatar *meta = NULL;

    if (!priv || !*priv || !response || !status)
        return -1;

    meta = *priv;

    if (!meta->valid) {
        return -1;
    }

    /* Copy to S3 mount point. Note: this could be done as background
     * task asynchronously
     */
    if ((s3_fd = get_image_fd(meta, 0)) < 0) {
        log_debug("[s3] opening image file failed!\n");
        return -1;
    }

    /* Decode BASE64 image */
    if (meta->encoding == IMAGE_ENCODE_BASE64) {
        /* TBD for now */
    }

    if (copy_fd_blocks(meta->image_fd, s3_fd)) {
        close(s3_fd);
        return -1;
    }
    close(s3_fd);

    /* Update MySQL DB */
    snprintf(json_msg, sizeof(json_msg),  "{\"update_avatar\""
             ":{\"id\":\"%s\",\"url\":\"%s/av_%s.dat\"}}",
             meta->id, g_avatardb_cfg->url, meta->id);
    if (gweb_json_post_processor(json_msg, strlen(json_msg), response, status)) {
        log_error("JSON post processor failed to handle update_avatar API\n");
        return -1;
    }

    /* Send response back */
    snprintf(json_msg, sizeof(json_msg), "{\"avatar_query\":{\"id\":\"%s\"}}",
             meta->id);
    if (gweb_json_post_processor(json_msg, strlen(json_msg), response, status)) {
        log_error("JSON post processor failed to handle API\n");
        return -1;
    }

    return 0;
}

int
avatardb_handle_uploaded_block (void **priv, const char *key, const char *data,
                                size_t size, uint64_t off, const char *content_type,
                                const char *encoding, char **response,
                                int *status)
{
    struct http_upload_avatar *meta = NULL;

    if (!priv)
        return -1;

    if (*priv == NULL) {
        meta = calloc(sizeof(struct http_upload_avatar), 1);
        if (!meta) {
            log_debug("avatardb: unable to allocate memory!\n");
            return -1;
        }
        meta->image_fd = -1;
        *priv = meta;
    } else {
        meta = *priv;
    }

    if (strcmp(key, "id") == 0) {
        log_debug("[avatardb] key-id, data = %s\n", data);
        snprintf(meta->id, MAX_UID_LEN, "%s", data);

        /*
         * Check if UID is valid before downloading the image to
         * cache. *TBD* Abstract the DB fetch APIs.
         */
        if (!gweb_mysql_check_uid_email(meta->id, NULL)) {
            log_debug("[avatardb] invalid UID: %s", meta->id);
            return -1;
        }

        /* Mark the upload as valid to accept further data */
        meta->valid = 1;

    } else if (strcmp(key, "image") == 0 && meta->valid) {
        if (meta->image_fd == -1) {
            /* Save image to cache */
            if ((meta->image_fd = get_image_fd(meta, 1)) < 0) {
                log_debug("[cache] opening cache image failed!\n");
                return -1;
            }
        }

        /* Parse encoding of block (at first offset) */
        if (off == 0) {
            if (encoding && (strcasecmp(encoding, "image/base64") == 0)) {
                meta->encoding = IMAGE_ENCODE_BASE64;
            } else {
                meta->encoding = IMAGE_ENCODE_RAW;
            }
        }

        /* Store the block of image on cache */
        if (save_data_block(meta, data, off, size)) {
            log_debug("[cache] saving image block (%"PRIu64", %zu) failed!\n",
                      off, size);
            return -1;
        }
    }

    return 0;
}

int
avatardb_init (void)
{
    S3BucketContext *s3ctx;
    char url[MAX_AVATAR_URL_LEN];

    if ((g_avatardb_cfg = config_load_avatardb()) == NULL) {
        log_error("Invalid AvatarDB configuration, bailing out\n");
        return -1;
    }

    s3ctx = g_avatardb_cfg->s3ctx;

    snprintf(url, MAX_AVATAR_URL_LEN, "http://%s/%s", s3ctx->hostName,
             g_avatardb_cfg->folder);

    g_avatardb_cfg->url = strndup(url, strlen(url));

    return 0;
}
