#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <syslog.h>
#include <microhttpd.h>
#include <mysql.h>

#include <gweb/common.h>
#include <gweb/server.h>
#include <gweb/json_api.h>
#include <gweb/mysqldb_api.h>
#include <gweb/config.h>
#include <gweb/avatardb.h>

/* Global structures */
enum {
    HTTP_REQ_INVAL       = 0,
    HTTP_REQ_POST        = 1,
    HTTP_REQ_POST_UPLOAD = 2,
    HTTP_REQ_POST_JSON   = 3,
    HTTP_REQ_GET         = 4,
};

/* POST upload type */
enum {
    HTTP_POST_UPLOAD_AVATAR = 1,
};

/* Connection info to retain the response structure for POST/PUT/GET
 * messages.
 */
struct http_cxn_info {
    int cxn_type;
    char *json_response;
    const char *url;
    int status_code;
    int upload_type;
    void *priv;
    struct MHD_Connection *connection;
    struct MHD_Response *response;
    struct MHD_PostProcessor *pp;
};

/* Globals */
static struct MHD_Daemon *g_daemon;

#define HTTP_RESPONSE_404_NOTFOUND		\
    "{\"status\":{\"code\":\"404\","		\
    "\"description\":\"Resource Not Found\"}}"

#define HTTP_RESPONSE_201_CREATED		\
    "{\"status\":{\"code\":\"201\","		\
    "\"description\":\"Resource Created\"}}"

#define HTTP_RESPONSE_200_OK			\
    "{\"status\":{\"code\":\"200\","		\
    "\"description\":\"OK\"}}"

static struct MHD_Response *
mhd_frame_response (struct MHD_Connection *connection, const char *json_http)
{
    struct MHD_Response *resp;

    resp = MHD_create_response_from_buffer(strlen(json_http),
					   (char *)json_http,
					   MHD_RESPMEM_MUST_COPY);
    if (resp == NULL) {
	log_error("framing response '%s' failed\n", json_http);
	return NULL;
    }

    MHD_add_response_header(resp, MHD_HTTP_HEADER_CONTENT_TYPE,
			    "application/json");

    return resp;
}

static int
mhd_send_page (struct http_cxn_info *httpcxn)
{
    if (httpcxn == NULL) {
	log_error("no connection information to send response!\n");
	return MHD_NO;
    }

    if (httpcxn->response == NULL) {
	httpcxn->response = mhd_frame_response(httpcxn->connection,
					       HTTP_RESPONSE_404_NOTFOUND);
	httpcxn->status_code = MHD_HTTP_NOT_FOUND;
    }

    MHD_queue_response(httpcxn->connection, httpcxn->status_code,
		       httpcxn->response);
    MHD_destroy_response(httpcxn->response);

    httpcxn->response = NULL;

    return MHD_YES;
}

static int check_json_content (void *cls, enum MHD_ValueKind kind, 
			       const char *key, const char *value)
{
    int *has_json = cls;

    if (strncmp(key, KEY_CONTENT_TYPE, strlen(KEY_CONTENT_TYPE)) == 0 &&
	strncmp(value, KEY_CONTENT_JSON, strlen(KEY_CONTENT_JSON)) == 0) {
	*has_json = 1;
    }
    return MHD_YES;
}

static void
gweb_build_http_response (struct http_cxn_info *cxn, char *resp, int status)
{
    if (resp) {
        cxn->response = mhd_frame_response(cxn->connection, (const char *)resp);
        cxn->status_code = (status == 0) ? MHD_HTTP_OK: MHD_HTTP_NOT_FOUND;
    } else {
        /* Generate based on status */
        if (status) {
            cxn->response = mhd_frame_response(cxn->connection,
                                               HTTP_RESPONSE_404_NOTFOUND);
            cxn->status_code = MHD_HTTP_NOT_FOUND;
        } else {
            cxn->response = mhd_frame_response(cxn->connection,
                                               HTTP_RESPONSE_200_OK);
            cxn->status_code = MHD_HTTP_OK;
        }
    }
}

static int
post_upload_completion_handler (void *coninfo_cls)
{
    struct http_cxn_info *httpcxn = coninfo_cls;
    char *response = NULL;
    int status = 0;

    if (httpcxn->upload_type == HTTP_POST_UPLOAD_AVATAR) {
        if (avatardb_handle_upload_complete(&httpcxn->priv, &response, &status) < 0)
            status = -1;

        avatardb_handle_upload_cleanup(&httpcxn->priv);

        gweb_build_http_response(httpcxn, response, status);
        if (response) {
            free(response);
        }
    }

    return MHD_YES;
}

static int
post_upload_handler (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
                     const char *filename, const char *content_type,
                     const char *transfer_encoding, const char *data, uint64_t off,
                     size_t size)
{
    struct http_cxn_info *httpcxn = coninfo_cls;

    char *response = NULL;
    int status = 0, ret;

    if (httpcxn->upload_type == HTTP_POST_UPLOAD_AVATAR) {
        ret = avatardb_handle_uploaded_block(&httpcxn->priv, key, data, size, off,
                                             content_type, transfer_encoding,
                                             &response, &status);
        if (ret >= 0) { /* continue */
            return MHD_YES;

        } else if (ret < 0) {
            log_error("Handling Avatar upload failed!\n");
            status = -1;
        }
    }

    gweb_build_http_response(httpcxn, response, status);
    if (response) {
        free(response);
    }

    return (httpcxn->status_code == MHD_HTTP_NOT_FOUND) ? MHD_NO: MHD_YES;
}

static int
json_post_handler (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
		   const char *filename, const char *content_type,
		   const char *transfer_encoding, const char *data, uint64_t off,
		   size_t size)
{
    struct http_cxn_info *httpcxn = coninfo_cls;
    char *response = NULL;
    int status;

    if (httpcxn->cxn_type == HTTP_REQ_POST_JSON) {
        if (gweb_json_post_processor(data, size, &response, &status)) {
            log_error("JSON post processor failed to handle API\n");
            status = -1;
        }

    } else if (httpcxn->cxn_type == HTTP_REQ_GET) {
        if (gweb_json_get_processor(httpcxn->connection, httpcxn->url,
                                    &response, &status)) {
            log_error("JSON get processor failed to handle API\n");
            status = -1;
        }
    }

    gweb_build_http_response(httpcxn, response, status);

    if (response) {
        free(response);
    }

    return MHD_YES;
}

/*
 * Microhttpd connection handler for all type of messages
 */
static int mhd_connection_handler (void *cls,
				   struct MHD_Connection *connection,
				   const char *url,
				   const char *method,
				   const char *version,
				   const char *upload_data,
				   size_t *upload_data_size,
				   void **con_cls)
{
    struct http_cxn_info *httpcxn = *con_cls;
    struct http_cxn_info default_httpcxn;

    int has_json = 0, type = HTTP_REQ_INVAL, req_type = 0;

#ifdef DEBUG
    log_debug("URL = <%s>\n", url);
    log_debug("METHOD = <%s>\n", method);
    log_debug("VERSION = <%s>\n", version);
    if (*upload_data_size < 80) {
        log_debug("UPLOAD_DATA = <%s>\n SIZE = %zd\n",
                  upload_data, *upload_data_size);
    } else {
        log_debug("UPLOAD_DATA_SIZE = %zd\n", *upload_data_size);
    }
#endif

    if (httpcxn) {
        switch (httpcxn->cxn_type) {
        case HTTP_REQ_POST:
        case HTTP_REQ_POST_UPLOAD:
        case HTTP_REQ_POST_JSON:
            if (httpcxn->pp != NULL) {
                if (*upload_data_size == 0) {
                    post_upload_completion_handler(httpcxn);
                    mhd_send_page(httpcxn);
                } else {
                    MHD_post_process(httpcxn->pp, upload_data, *upload_data_size);
                    *upload_data_size = 0;
                }
            }
            break;
        case HTTP_REQ_GET:
            json_post_handler(httpcxn, 0, NULL, NULL, NULL, NULL, NULL, 0, 0);
            mhd_send_page(httpcxn);
            break;
        default:
            break;
        }
	return MHD_YES;
    } else {
	MHD_get_connection_values(connection, MHD_HEADER_KIND,
                                  &check_json_content, &has_json);
        if (has_json) {
            if (strcmp(method, "POST") != 0) {
                /* Cannot handle JSON in methods other than POST */
                log_error("JSON handling is support only in POST (method=%s)\n",
                          method);
                goto __send_error_info;
            }
            type = HTTP_REQ_POST_JSON;

        } else if (strcmp(method, "GET") == 0) {
            type = HTTP_REQ_GET;

        } else if (strcmp(method, "POST") == 0) {
            /* Check if this is data upload */
            if (strcmp(url, "/uploads/avatar") == 0) {
                type = HTTP_REQ_POST_UPLOAD;
                req_type = HTTP_POST_UPLOAD_AVATAR;

            } else {
                type = HTTP_REQ_POST;
            }
        } else {
            log_error("Invalid message (neither POST nor GET)\n");
            goto __send_error_info;
        }

        if ((httpcxn = calloc(1, sizeof(struct http_cxn_info))) == NULL) {
            log_error("unable to allocate memory!\n");
            goto __send_error_info;
        }

        httpcxn->connection = connection;
        httpcxn->cxn_type = type;
        httpcxn->url = url;
        if (req_type) {
            httpcxn->upload_type = req_type;
        }

        switch (httpcxn->cxn_type) {
        case HTTP_REQ_POST:
        case HTTP_REQ_POST_JSON:
            httpcxn->pp = MHD_create_post_processor(connection, GWEB_POST_BUFSZ,
                                                    &json_post_handler, httpcxn);
            if (httpcxn->pp == NULL) {
                log_debug("%s: creating post processor failed!!!\n", __func__);
                free(httpcxn);
                goto __send_error_info;
            }
            break;
        case HTTP_REQ_POST_UPLOAD:
            httpcxn->pp = MHD_create_post_processor(connection, GWEB_POST_BUFSZ,
                                                    &post_upload_handler, httpcxn);
            if (httpcxn->pp == NULL) {
                log_debug("%s: creating post-upload processor failed!!!\n", __func__);
                free(httpcxn);
                goto __send_error_info;
            }
            break;

        case HTTP_REQ_GET:
        default:
            break;
        }

        *con_cls = httpcxn;

        return MHD_YES;
    }

__send_error_info:
    default_httpcxn.response = mhd_frame_response(connection,
						  HTTP_RESPONSE_404_NOTFOUND);
    default_httpcxn.status_code = MHD_HTTP_NOT_FOUND;
    default_httpcxn.connection = connection;

    mhd_send_page(&default_httpcxn);

    return MHD_YES;
}

static void
mhd_request_completed (void *cls, struct MHD_Connection *connection, 
		       void **con_cls,
		       enum MHD_RequestTerminationCode toe)
{
    struct http_cxn_info *httpcxn = *con_cls;

    if (httpcxn == NULL)
	return;

    log_debug("%s: request completed ========\n", __func__);

    if (httpcxn->json_response)
        free(httpcxn->json_response);
    if (httpcxn->response)
        MHD_destroy_response(httpcxn->response);
    if (httpcxn->pp)
        MHD_destroy_post_processor(httpcxn->pp);
    if (httpcxn->priv)
        free(httpcxn->priv);

    free(httpcxn);
    *con_cls = NULL;
}

static void
sig_kill_handler (int signum)
{
    log_debug("%s: killing daemon!\n", __func__);

    if (g_daemon)
	MHD_stop_daemon(g_daemon);

    gweb_mysql_shutdown();

#ifdef LOG_TO_SYSLOG
    closelog();
#endif

    exit(0);
}

static void
daemonize_this_process (void)
{
    pid_t pid;
    int fd;

    /* Fork parent and kill */
    if ((pid = fork()) < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set child as session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork keeping child as session leader */
    if ((pid = fork()) < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");

    for (fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--)
        close(fd);

#ifdef LOG_TO_SYSLOG
    /* Open the log file */
    openlog("GWEB-DAEMON", LOG_PID, LOG_DAEMON);

    setlogmask(LOG_MASK(LOG_DEBUG)  |
               LOG_MASK(LOG_NOTICE) |
               LOG_MASK(LOG_ERR));
#endif
}

/*
 * Start the webserver and listen to given port
 */
int main (int argc, char *argv[])
{
    struct MHD_Daemon *daemon;
    uint32_t server_port = GWEB_SERVER_PORT;

    /* Quick/Dirty check for port option */
    if (argc == 3 && argv[1][0] == '-' && argv[1][1] == 'p') {
        server_port = atoi(argv[2]);
        if (!server_port) {
            server_port = GWEB_SERVER_PORT;
        }
    }

    daemonize_this_process();

    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY,
		 server_port, NULL, NULL, &mhd_connection_handler, NULL,
		 MHD_OPTION_EXTERNAL_LOGGER, fprintf, NULL,
		 MHD_OPTION_NOTIFY_COMPLETED, &mhd_request_completed, NULL,
		 MHD_OPTION_END);
    if (daemon == NULL) {
	log_error("unable to start MHD daemon on port %d\n",
		  GWEB_SERVER_PORT);
	return -1;
    }

    /* Parse config file */
    config_parse_and_load(argc, argv);

    /* Initialize MySQL */
    if (gweb_mysql_init()) {
	log_error("opening MYSQL connection failed\n");
	if (g_daemon) {
	    MHD_stop_daemon(g_daemon);
	}
	return -1;
    }

    if (avatardb_init()) {
        log_error("avatar DB initialization failed\n");
        if (g_daemon) {
            MHD_stop_daemon(g_daemon);
        }
        return -1;
    }
    
    g_daemon = daemon;

    signal(SIGUSR1, sig_kill_handler);

    while (1) {
	sleep(60);
    }

    return 0;
}

/*
 * Local Variables:
 * compile-command:"gcc gweb_server.c -I../production/include -L../production/lib `mysql_config --cflags` -lmicrohttpd -ljson-c `mysql_config --libs`"
 * End:
 */
