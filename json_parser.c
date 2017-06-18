/*
 * Handle parsing of JSON strings in the REST APIs
 */
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>

#include <microhttpd.h>

#include <gweb/common.h>
#include <gweb/json_api.h>
#include <gweb/mysqldb_api.h>

/*
 * JSON C map for each of the REST APIs to parse JSON message and push
 * DB updates. NOTE: the backend call should be decoupled from this
 * module and lifted.
 */
struct json_map_info {
    const char *api_name;
    /* JSON-C APIs */
    int (*api_handler) (struct json_object *, j2c_msg_t *);
    int (*api_resp_handler) (j2c_resp_t *, char **);

    /* JSON-DB APIs */
    int (*api_db_handler) (j2c_msg_t *, j2c_resp_t **);
    int (*api_db_resp_free) (j2c_resp_t *);
};

/* Debug globals */
static int json_parse_dump = 1;

static const char *_table_registration_msg_fields[] = {
    [FIELD_REGISTRATION_FNAME] = "fname",
    [FIELD_REGISTRATION_LNAME] = "lname",
    [FIELD_REGISTRATION_EMAIL] = "email",
    [FIELD_REGISTRATION_PHONE] = "phone",
    [FIELD_REGISTRATION_PASSWORD] = "password",
};

static const char *_table_profile_msg_fields[] = {
    [FIELD_PROFILE_UID] = "id",
    [FIELD_PROFILE_ADDRESS1] = "add1",
    [FIELD_PROFILE_ADDRESS2] = "add2",
    [FIELD_PROFILE_ADDRESS3] = "add3",
    [FIELD_PROFILE_COUNTRY] = "country",
    [FIELD_PROFILE_STATE] = "state",
    [FIELD_PROFILE_PINCODE] = "pincode",
    [FIELD_PROFILE_FACEBOOK_HANDLE] = "facebook_h",
    [FIELD_PROFILE_TWITTER_HANDLE] = "twitter_h",
};

static const char *_table_login_msg_fields[] = {
    [FIELD_LOGIN_EMAIL] = "email",
    [FIELD_LOGIN_PASSWORD] = "password",
};

static const char *_table_avatar_msg_fields[] = {
    [FIELD_AVATAR_UID] = "id",
    [FIELD_AVATAR_URL] = "url",
};

static const char *_table_cxn_channel_msg_fields[] = {
    [FIELD_CXN_CHANNEL_UID] = "from",
    [FIELD_CXN_CHANNEL_TO_UID] = "to",
    [FIELD_CXN_CHANNEL_TYPE] = "channel",
};

static const char *_table_cxn_request_msg_fields[] = {
    [FIELD_CXN_REQUEST_UID] = "from",
    [FIELD_CXN_REQUEST_TO_UID] = "to",
    [FIELD_CXN_REQUEST_FLAG] = "flag",
};

static const char *_table_cxn_request_query_msg_fields[] = {
    [FIELD_CXN_REQUEST_QUERY_FROM_UID] = "from",
    [FIELD_CXN_REQUEST_QUERY_TO_UID] = "to",
    [FIELD_CXN_REQUEST_QUERY_FLAG] = "flag",
};

static const char *_table_cxn_channel_query_msg_fields[] = {
    [FIELD_CXN_CHANNEL_QUERY_FROM_UID] = "from",
    [FIELD_CXN_CHANNEL_QUERY_TO_UID] = "to",
    [FIELD_CXN_CHANNEL_QUERY_TYPE] = "channel",
};

static const char *_table_uid_query_msg_fields[] = {
    [FIELD_UID_QUERY_EMAIL] = "email",
};

static const char *_table_profile_query_msg_fields[] = {
    [FIELD_PROFILE_QUERY_UID] = "id",
};

static const char *_table_avatar_query_msg_fields[] = {
    [FIELD_AVATAR_QUERY_UID] = "id",
};

static const char *_table_registration_resp_fields[] = {
    [FIELD_REGISTRATION_RESP_CODE] = "code",
    [FIELD_REGISTRATION_RESP_DESC] = "description",
    [FIELD_REGISTRATION_RESP_UID] = "id",
};

static const char *_table_profile_resp_fields[] = {
    [FIELD_PROFILE_RESP_CODE] = "code",
    [FIELD_PROFILE_RESP_DESC] = "description",
};

static const char *_table_profile_info_resp_fields[] = {
    [FIELD_PROFILE_INFO_RESP_CODE] = "code",
    [FIELD_PROFILE_INFO_RESP_DESC] = "description",
    [FIELD_PROFILE_INFO_RESP_UID] = "id",
    [FIELD_PROFILE_INFO_RESP_FNAME] = "fname",
    [FIELD_PROFILE_INFO_RESP_LNAME] = "lname",
    [FIELD_PROFILE_INFO_RESP_EMAIL] = "email",
    [FIELD_PROFILE_INFO_RESP_PHONE] = "phone",
    [FIELD_PROFILE_INFO_RESP_ADDRESS1] = "add1",
    [FIELD_PROFILE_INFO_RESP_ADDRESS2] = "add2",
    [FIELD_PROFILE_INFO_RESP_ADDRESS3] = "add3",
    [FIELD_PROFILE_INFO_RESP_COUNTRY] = "country",
    [FIELD_PROFILE_INFO_RESP_STATE] = "state",
    [FIELD_PROFILE_INFO_RESP_PINCODE] = "pincode",
    [FIELD_PROFILE_INFO_RESP_FACEBOOK_HANDLE] = "facebook_h",
    [FIELD_PROFILE_INFO_RESP_TWITTER_HANDLE] = "twitter_h",
    [FIELD_PROFILE_INFO_RESP_AVATAR_URL] = "url",
};

static const char *_table_avatar_resp_fields[] = {
    [FIELD_AVATAR_RESP_CODE] = "code",
    [FIELD_AVATAR_RESP_DESC] = "description",
};

static const char *_table_cxn_request_resp_fields[] = {
    [FIELD_CXN_REQUEST_RESP_CODE] = "code",
    [FIELD_CXN_REQUEST_RESP_DESC] = "description",
};

static const char *_table_cxn_channel_resp_fields[] = {
    [FIELD_CXN_CHANNEL_RESP_CODE] = "code",
    [FIELD_CXN_CHANNEL_RESP_DESC] = "description",
};

static const char *_table_cxn_request_query_resp_fields[] = {
    [FIELD_CXN_REQUEST_QUERY_RESP_CODE] = "code",
    [FIELD_CXN_REQUEST_QUERY_RESP_DESC] = "description",
    [FIELD_CXN_REQUEST_QUERY_RESP_RECORD_COUNT] = "count",
    [FIELD_CXN_REQUEST_QUERY_RESP_ARRAY_START] = JSON_C_ARRAY_START,
    [FIELD_CXN_REQUEST_QUERY_RESP_UID] = "id",
    [FIELD_CXN_REQUEST_QUERY_RESP_FNAME] = "fname",
    [FIELD_CXN_REQUEST_QUERY_RESP_LNAME] = "lname",
    [FIELD_CXN_REQUEST_QUERY_RESP_AVATAR_URL] = "url",
    [FIELD_CXN_REQUEST_QUERY_RESP_DATE] = "date",
    [FIELD_CXN_REQUEST_QUERY_RESP_FLAG] = "flag",
    [FIELD_CXN_REQUEST_QUERY_RESP_ARRAY_END] = JSON_C_ARRAY_END,
};

static const char *_table_cxn_channel_query_resp_fields[] = {
    [FIELD_CXN_CHANNEL_QUERY_RESP_CODE] = "code",
    [FIELD_CXN_CHANNEL_QUERY_RESP_DESC] = "description",
    [FIELD_CXN_CHANNEL_QUERY_RESP_RECORD_COUNT] = "count",
    [FIELD_CXN_CHANNEL_QUERY_RESP_ARRAY_START] = JSON_C_ARRAY_START,
    [FIELD_CXN_CHANNEL_QUERY_RESP_UID] = "id",
    [FIELD_CXN_CHANNEL_QUERY_RESP_FNAME] = "fname",
    [FIELD_CXN_CHANNEL_QUERY_RESP_LNAME] = "lname",
    [FIELD_CXN_CHANNEL_QUERY_RESP_AVATAR_URL] = "url",
    [FIELD_CXN_CHANNEL_QUERY_RESP_DATE] = "date",
    [FIELD_CXN_CHANNEL_QUERY_RESP_CHANNEL_TYPE] = "channel",
    [FIELD_CXN_CHANNEL_QUERY_RESP_ARRAY_END] = JSON_C_ARRAY_END,
};

static const char *_table_uid_query_resp_fields[] = {
    [FIELD_UID_QUERY_RESP_CODE] = "code",
    [FIELD_UID_QUERY_RESP_DESC] = "description",
    [FIELD_UID_QUERY_RESP_UID] = "id",
};

static const char *_table_avatar_query_resp_fields[] = {
    [FIELD_AVATAR_QUERY_RESP_CODE] = "code",
    [FIELD_AVATAR_QUERY_RESP_DESC] = "description",
    [FIELD_AVATAR_QUERY_RESP_URL] = "url",
};

#define table_field_at_index(tbl, findex)				\
    _table_##tbl##_fields[findex]

#define for_each_table_findex(findex, tbl)				\
    for (findex = 0; findex < ARRAY_SIZE(_table_##tbl##_fields); findex++)

#define json_parse_record_generator(tbl)				\
    int gweb_json_parse_record_##tbl (struct json_object *jobj,         \
                                      j2c_msg_t *j2cmsg)		\
    {									\
	struct json_object *jfield;					\
	struct j2c_##tbl##_msg *j2ctbl = &j2cmsg->tbl;			\
	int findex;							\
									\
	for_each_table_findex(findex, tbl##_msg) {			\
	    if (!json_object_object_get_ex(jobj,			\
 			 table_field_at_index(tbl##_msg, findex),       \
                         &jfield)) {                                    \
		log_debug("<JSON-PARSE: (" #tbl ")> "                   \
			  "skipping field '%s' in message\n",		\
			  table_field_at_index(tbl##_msg, findex));     \
		j2ctbl->fields[findex] = NULL;				\
		continue;						\
	    }								\
	    j2ctbl->fields[findex] = json_object_get_string(jfield);	\
	}								\
	if (json_parse_dump)						\
	    gweb_json_dump_##tbl(j2ctbl);				\
	return 1;							\
    }

#define json_dump_record_generator(tbl)                                 \
    static void gweb_json_dump_##tbl (struct j2c_##tbl##_msg *j2ctbl)	\
    {									\
	int findex;							\
									\
	for_each_table_findex(findex, tbl##_msg) {			\
            log_debug("<JSON-PARSE: (" #tbl ")> %s => %s\n",            \
		      table_field_at_index(tbl##_msg, findex),	        \
		      j2ctbl->fields[findex]);				\
	}								\
    }

/*
 * Response generator allocates memory and expects the higher POST
 * responder to free it
 */
#define MAX_RESPONSE_BYTES (2048)

#define PUSH_BUF(fmt,...)                       \
    len += sprintf(*response+len, fmt, ## __VA_ARGS__)

#define json_dummy_array_response_generator(tbl)                        \
    int gweb_json_gen_response_array_##tbl (j2c_resp_t *j2cresp,        \
                                            void *table,                \
                                            int array_idx,              \
                                            int start_findex,           \
                                            int buf_len,                \
                                            char **response)            \
    {                                                                   \
        return buf_len;                                                 \
    }

#define json_array_response_generator(tbl)                              \
    int gweb_json_gen_response_array_##tbl (j2c_resp_t *j2cresp,        \
                                            void *table,                \
                                            int array_idx,              \
                                            int start_findex,           \
                                            int buf_len,                \
                                            char **response)            \
    {                                                                   \
        int len = buf_len, idx, tbl_idx;                                \
        struct j2c_##tbl##_resp *j2ctbl = &j2cresp->tbl;                \
                                                                        \
        /* For now support one array set */                             \
        struct j2c_##tbl##_resp_array1 *j2carr = j2ctbl->array1;        \
        int nr_elem = j2ctbl->nr_array1_records;                        \
                                                                        \
        if (nr_elem < 0) {                                              \
            return (len);                                               \
        }                                                               \
                                                                        \
        PUSH_BUF("\"array%d\":[", array_idx);                           \
        for (idx = 0; idx < nr_elem; idx++) {                           \
            PUSH_BUF("{");                                              \
            for (tbl_idx = start_findex+1; ; tbl_idx++) {               \
                if (table_field_at_index(tbl##_resp, tbl_idx) ==        \
                    JSON_C_ARRAY_END) {                                 \
                    break;                                              \
                }                                                       \
                if (j2carr[idx].fields[tbl_idx-start_findex-1]) {       \
                    PUSH_BUF("\"%s\":\"%s\",",                          \
                             table_field_at_index(tbl##_resp, tbl_idx), \
                             j2carr[idx].fields[tbl_idx-start_findex-1]); \
                }                                                       \
            }                                                           \
            len--;                                                     \
            PUSH_BUF("},");                                             \
        }                                                               \
        if (nr_elem) {                                                  \
            len--;                                                      \
        }                                                               \
        (*response)[len++] = ']';                                       \
        (*response)[len++] = '\0';                                      \
        return (len);                                                   \
    }

#define json_response_generator(tbl)                                    \
    int gweb_json_gen_response_##tbl (j2c_resp_t *j2cresp,              \
                                      char **response)                  \
    {                                                                   \
        int findex, array_count = 0, len = 0;                           \
        struct j2c_##tbl##_resp *j2ctbl = &j2cresp->tbl;                \
                                                                        \
        if (!response) {                                                \
            return 1;                                                   \
        }                                                               \
                                                                        \
        *response = malloc(MAX_RESPONSE_BYTES * sizeof(char));          \
        if (!*response) {                                               \
            return 1;                                                   \
        }                                                               \
                                                                        \
        PUSH_BUF("{\"status\":{");                                      \
        for_each_table_findex(findex, tbl##_resp) {                     \
            if (table_field_at_index(tbl##_resp, findex) ==             \
                JSON_C_ARRAY_START) {                                   \
                array_count++;                                          \
                if (array_count == 1) {                                 \
                    len = gweb_json_gen_response_array_##tbl(j2cresp,   \
                                    j2ctbl, array_count, findex,        \
                                    len, response);                     \
                }                                                       \
                while (table_field_at_index(tbl##_resp, ++findex)       \
                       != JSON_C_ARRAY_END);                            \
                continue;                                               \
            }                                                           \
            if (j2ctbl->fields[findex]) {                               \
                PUSH_BUF("\"%s\":\"%s\",",                              \
                         table_field_at_index(tbl##_resp, findex),      \
                         j2ctbl->fields[findex]);                       \
            }                                                           \
        }                                                               \
                                                                        \
        /* Remove trailing ',' */                                       \
        (*response)[len-1] = '}';                                       \
        (*response)[len++] = '}';                                       \
        (*response)[len] = '\0';                                        \
                                                                        \
        return 0;                                                       \
    }

/* Registration */
json_dump_record_generator(registration)
json_parse_record_generator(registration)
json_dummy_array_response_generator(registration)
json_response_generator(registration)

/* Profile */
json_dump_record_generator(profile)
json_parse_record_generator(profile)
json_dummy_array_response_generator(profile)
json_response_generator(profile)

/* Login */
json_dump_record_generator(login)
json_parse_record_generator(login)
json_dummy_array_response_generator(login)

/* Avatar */
json_dump_record_generator(avatar)
json_parse_record_generator(avatar)
json_dummy_array_response_generator(avatar)
json_response_generator(avatar)

/* Connect request */
json_dump_record_generator(cxn_request)
json_parse_record_generator(cxn_request)
json_dummy_array_response_generator(cxn_request)
json_response_generator(cxn_request)

json_dump_record_generator(cxn_request_query)
json_parse_record_generator(cxn_request_query)
json_array_response_generator(cxn_request_query)
json_response_generator(cxn_request_query)

/* Connect channel */
json_dump_record_generator(cxn_channel)
json_parse_record_generator(cxn_channel)
json_dummy_array_response_generator(cxn_channel)
json_response_generator(cxn_channel)

json_dump_record_generator(cxn_channel_query)
json_parse_record_generator(cxn_channel_query)
json_array_response_generator(cxn_channel_query)
json_response_generator(cxn_channel_query)

json_dump_record_generator(uid_query)
json_parse_record_generator(uid_query)
json_dummy_array_response_generator(uid_query)
json_response_generator(uid_query)

json_dump_record_generator(profile_query)
json_parse_record_generator(profile_query)
json_dummy_array_response_generator(profile_query)

json_dummy_array_response_generator(profile_info)
json_response_generator(profile_info)

json_dump_record_generator(avatar_query)
json_parse_record_generator(avatar_query)
json_dummy_array_response_generator(avatar_query)
json_response_generator(avatar_query)

#define JSON_PARSE_FN(tbl)     gweb_json_parse_record_##tbl
#define JSON_RESP_FN(tbl)      gweb_json_gen_response_##tbl

#define API_RECORD_ENTRY(idx, name, j_parse, j_resp, db_handler, db_free) \
    [idx] = {                                                           \
        .api_name          = name,                                      \
        .api_handler       = j_parse,                                   \
        .api_resp_handler  = j_resp,                                    \
        .api_db_handler    = db_handler,                                \
        .api_db_resp_free  = db_free,                                   \
    }

struct json_map_info _j2c_map_info[] = {
    API_RECORD_ENTRY(JSON_C_REGISTRATION_MSG,
                     "registration",
                     JSON_PARSE_FN(registration),
                     JSON_RESP_FN(registration),
                     gweb_mysql_handle_registration,
                     gweb_mysql_free_registration),

    API_RECORD_ENTRY(JSON_C_PROFILE_MSG,
                     "update_profile",
                     JSON_PARSE_FN(profile),
                     JSON_RESP_FN(profile),
                     gweb_mysql_handle_profile,
                     gweb_mysql_free_profile),

    API_RECORD_ENTRY(JSON_C_LOGIN_MSG,
                     "login",
                     JSON_PARSE_FN(login),
                     JSON_RESP_FN(profile_info),
                     gweb_mysql_handle_login,
                     gweb_mysql_free_login),

    API_RECORD_ENTRY(JSON_C_AVATAR_MSG,
                     "update_avatar",
                     JSON_PARSE_FN(avatar),
                     JSON_RESP_FN(avatar),
                     gweb_mysql_handle_avatar,
                     gweb_mysql_free_avatar),

    API_RECORD_ENTRY(JSON_C_CXN_REQUEST_MSG,
                     "cxn_request",
                     JSON_PARSE_FN(cxn_request),
                     JSON_RESP_FN(cxn_request),
                     gweb_mysql_handle_cxn_request,
                     gweb_mysql_free_cxn_request),

    API_RECORD_ENTRY(JSON_C_CXN_CHANNEL_MSG,
                     "cxn_channel",
                     JSON_PARSE_FN(cxn_channel),
                     JSON_RESP_FN(cxn_channel),
                     gweb_mysql_handle_cxn_channel,
                     gweb_mysql_free_cxn_channel),

    /* GET APIs, does not require PARSE for JSON, retained till JSON
     * handling is cleaned up.
     */
    API_RECORD_ENTRY(JSON_C_CXN_REQUEST_QUERY_MSG,
                     "cxn_request_query",
                     JSON_PARSE_FN(cxn_request_query),
                     JSON_RESP_FN(cxn_request_query),
                     gweb_mysql_handle_cxn_request_query,
                     gweb_mysql_free_cxn_request_query),

    API_RECORD_ENTRY(JSON_C_CXN_CHANNEL_QUERY_MSG,
                     "cxn_channel_query",
                     JSON_PARSE_FN(cxn_channel_query),
                     JSON_RESP_FN(cxn_channel_query),
                     gweb_mysql_handle_cxn_channel_query,
                     gweb_mysql_free_cxn_channel_query),

    API_RECORD_ENTRY(JSON_C_UID_QUERY_MSG,
                     "uid_query",
                     JSON_PARSE_FN(uid_query),
                     JSON_RESP_FN(uid_query),
                     gweb_mysql_handle_uid_query,
                     gweb_mysql_free_uid_query),

    API_RECORD_ENTRY(JSON_C_PROFILE_QUERY_MSG,
                     "profile_query",
                     JSON_PARSE_FN(profile_query),
                     JSON_RESP_FN(profile_info),
                     gweb_mysql_handle_profile_query,
                     gweb_mysql_free_profile_query),

    API_RECORD_ENTRY(JSON_C_AVATAR_QUERY_MSG,
                     "avatar_query",
                     JSON_PARSE_FN(avatar_query),
                     JSON_RESP_FN(avatar_query),
                     gweb_mysql_handle_avatar_query,
                     gweb_mysql_free_avatar_query),
};

/*
 * JSON POST processor
 *
 * Handle HTTP POST requests with application/json message
 */
int
gweb_json_post_processor (const char *data, size_t size, char **response, int *status)
{
    struct json_object *jobj = json_tokener_parse(data);
    struct json_object *jrecord;
    struct json_map_info *j2cinfo;
    j2c_msg_t j2cmsg;
    j2c_resp_t *j2cresp;
    int api_index, ret = -1;

    if (!jobj) {
        log_error("<JSON-PARSE: post-processor> invalid json string!\n");
        return -1;
    }

    for (api_index = JSON_C_MSG_MIN+1; api_index < JSON_C_MSG_MAX; api_index++) {
        j2cinfo = &_j2c_map_info[api_index];

        if (json_object_object_get_ex(jobj, j2cinfo->api_name, &jrecord)) {
            if (j2cinfo->api_handler) {
                log_debug("<JSON-PARSE: post-processor> handling API: %s\n",
                          j2cinfo->api_name);
                ret = (*j2cinfo->api_handler)(jrecord, &j2cmsg);
            }
            /* *FIXME* Handle JSON parsing failures */
            if (j2cinfo->api_db_handler) {
                j2cresp = NULL;
                log_debug("<JSON-PARSE: post-processor> handling API backend: %s\n",
                          j2cinfo->api_name);

                ret = (*j2cinfo->api_db_handler)(&j2cmsg, &j2cresp);
                if (status) {
                    *status = ret;
                }

                /* TBD: Handle errors */
                /* Handle response structure from DB layer */
                if (j2cinfo->api_resp_handler && j2cresp != NULL) {
                    log_debug("<JSON-PARSE: post-processor> handling DB response: %s\n",
                              j2cinfo->api_name);
                    ret = (*j2cinfo->api_resp_handler)(j2cresp, response);
                }

                /* Free response structure from DB layer, call free
                 * even if the above response handler had failed.
                 */
                if (j2cinfo->api_db_resp_free) {
                    (*j2cinfo->api_db_resp_free)(j2cresp);
                }
            }

            /* NOTE: we don't handle multiple JSON messages through a
             * single POST message due to varied response for each of
             * the messages. We may need a vector to push responses so
             * that these could be bundled back as a single POST
             * response.
             */
            break;
        }
    }
   return ret;
}

#define parse_get_to_json_tokens(conn, tbl, buf, len)       \
    do {                                                    \
        char *p_buf;                                        \
        const char *val, *fld;                              \
        int idx;                                            \
                                                            \
        p_buf = buf;                                        \
        len += sprintf(p_buf+len, "{\"" #tbl "\":{");       \
        for_each_table_findex(idx, tbl##_msg) {             \
            fld = table_field_at_index(tbl##_msg, idx);     \
            val = MHD_lookup_connection_value(conn,         \
                             MHD_GET_ARGUMENT_KIND, fld);   \
            if (val) {                                      \
                len += sprintf(p_buf+len, "\"%s\":\"%s\",", \
                               fld, val);                   \
            }                                               \
        }                                                   \
        buf[len-1] = '}';                                   \
        buf[len++] = '}';                                   \
        buf[len] = '\0';                                    \
    } while (0)

int
gweb_json_get_processor (void *connection, const char *url,
                         char **response, int *status)
{
    char json_get_buf[256];
    int len = 0, is_valid_qry = 1;

    if (strcmp(url, "/query/cxn_request") == 0) {
        parse_get_to_json_tokens(connection, cxn_request_query,
                                 json_get_buf, len);

    } else if (strcmp(url, "/query/cxn_channel") == 0) {
        parse_get_to_json_tokens(connection, cxn_channel_query,
                                 json_get_buf, len);

    } else if (strcmp(url, "/query/uid") == 0) {
        parse_get_to_json_tokens(connection, uid_query, json_get_buf, len);

    } else if (strcmp(url, "/query/profile") == 0) {
        parse_get_to_json_tokens(connection, profile_query, json_get_buf, len);

    } else if (strcmp(url, "/query/avatar") == 0) {
        parse_get_to_json_tokens(connection, avatar_query, json_get_buf, len);

    } else {
        is_valid_qry = 0;
    }

    if (is_valid_qry) {
        return gweb_json_post_processor((const char *)json_get_buf, len,
                                        response, status);
    }

    return MHD_YES;
}
