#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>

#include <mysql.h>

#define DEBUG
#include <gweb/common.h>
#include <gweb/server.h>
#include <gweb/json_api.h>
#include <gweb/mysqldb_api.h>

/* Global structures */
enum {
    HTTP_REQ_POST = 1,
    HTTP_REQ_GET = 2,
};

/* Connection info to retain the response structure for POST/PUT/GET
 * messages.
 */
struct http_cxn_info {
    int cxn_type;
    char *json_response;
    int status_code;
    struct MHD_Connection *connection;
    struct MHD_Response *response;
    struct MHD_PostProcessor *pp;
};

/* Globals */
static struct MHD_Daemon *g_daemon;

#define HTTP_RESPONSE_404_NOTFOUND		\
    "\"status\":{\"code\":\"404\","		\
    "\"description\":\"Resource Not Found\"}"

#define HTTP_RESPONSE_201_CREATED		\
    "\"status\":{\"code\":\"201\","		\
    "\"description\":\"Resource Created\"}"

#define HTTP_RESPONSE_200_OK			\
    "\"status\":{\"code\":\"200\","		\
    "\"description\":\"OK\"}"

static struct MHD_Response *
mhd_frame_response (struct MHD_Connection *connection, const char *json_http)
{
    struct MHD_Response *resp;
    int ret;

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

static int
json_post_handler (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
		   const char *filename, const char *content_type,
		   const char *transfer_encoding, const char *data, uint64_t off,
		   size_t size)
{
    struct http_cxn_info *httpcxn = coninfo_cls;

    if (gweb_json_post_processor(data, size)) {
	log_error("JSON post processor failed to handle API\n");
	return MHD_NO;
    }

    httpcxn->response = mhd_frame_response(httpcxn->connection,
					   HTTP_RESPONSE_200_OK);
    httpcxn->status_code = MHD_HTTP_OK;

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

    int ret = MHD_NO, has_json;

    log_debug("URL = <%s>\n", url);
    log_debug("METHOD = <%s>\n", method);
    log_debug("VERSION = <%s>\n", version);
    log_debug("UPLOAD_DATA = <%s>\n SIZE = %zd\n",
	   upload_data, *upload_data_size);

    if (httpcxn && httpcxn->pp != NULL) {
	if (*upload_data_size == 0) {
	    mhd_send_page(httpcxn);
	} else {
	    log_debug("UPLOAD DATA:\n<%s>\n DATA-SIZE: %zd\n", upload_data,
		   *upload_data_size);
	    MHD_post_process(httpcxn->pp, upload_data, *upload_data_size);
	    *upload_data_size = 0;
	}
	return MHD_YES;
    } else {
	has_json = 0;
	MHD_get_connection_values(connection, MHD_HEADER_KIND, &check_json_content, &has_json);
	if (has_json) {
	    log_debug("FOUND JSON content!\n");

	    if ((httpcxn = calloc(1, sizeof(struct http_cxn_info))) == NULL) {
		log_error("unable to allocate memory!\n");
		goto __send_error_info;
	    }

	    httpcxn->connection = connection;

	    if (strcmp(method, "POST") == 0) {
		httpcxn->cxn_type = HTTP_REQ_POST;
	    } else {
		httpcxn->cxn_type = HTTP_REQ_GET;
	    }

	    httpcxn->pp = MHD_create_post_processor(connection, GWEB_POST_BUFSZ,
						    &json_post_handler, httpcxn);
	    if (httpcxn->pp == NULL) {
		log_debug("%s: creating post processor failed!!!\n", __func__);
		free(httpcxn);
		goto __send_error_info;
	    }

	    *con_cls = httpcxn;
	    return MHD_YES;
	}
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

    if (httpcxn->cxn_type == HTTP_REQ_POST) {
	if (httpcxn->json_response)
	    free(httpcxn->json_response);
	if (httpcxn->response)
	    MHD_destroy_response(httpcxn->response);
	MHD_destroy_post_processor(httpcxn->pp);
    }

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

    exit(0);
}

/*
 * Start the webserver and listen to given port
 */
int main (int argc, char *argv[])
{
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY,
		 GWEB_SERVER_PORT, NULL, NULL, &mhd_connection_handler, NULL,
		 MHD_OPTION_EXTERNAL_LOGGER, fprintf, NULL,
		 MHD_OPTION_NOTIFY_COMPLETED, &mhd_request_completed, NULL,
		 MHD_OPTION_END);
    if (daemon == NULL) {
	log_error("unable to start MHD daemon on port %d\n",
		  GWEB_SERVER_PORT);
	return -1;
    }

    /* Initialize MySQL */
    if (gweb_mysql_init()) {
	log_error("opening MYSQL connection failed\n");
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
 * compile-command:"gcc handle_response_microhttpd.c -I../production/include -L../production/lib `mysql_config --cflags` -lmicrohttpd -ljson-c `mysql_config --libs`"
 * End:
 */
