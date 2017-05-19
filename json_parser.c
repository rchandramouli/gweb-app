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

static const char *_table_registration_resp_fields[] = {
    [FIELD_REGISTRATION_RESP_CODE] = "code",
    [FIELD_REGISTRATION_RESP_DESC] = "description",
    [FIELD_REGISTRATION_RESP_UID] = "id",
};

static const char *_table_profile_resp_fields[] = {
    [FIELD_PROFILE_RESP_CODE] = "code",
    [FIELD_PROFILE_RESP_DESC] = "description",
};

static const char *_table_login_resp_fields[] = {
    [FIELD_LOGIN_RESP_CODE] = "code",
    [FIELD_LOGIN_RESP_DESC] = "description",
    [FIELD_LOGIN_RESP_UID] = "id",
    [FIELD_LOGIN_RESP_FNAME] = "fname",
    [FIELD_LOGIN_RESP_LNAME] = "lname",
    [FIELD_LOGIN_RESP_EMAIL] = "email",
    [FIELD_LOGIN_RESP_PHONE] = "phone",
    [FIELD_LOGIN_RESP_ADDRESS1] = "add1",
    [FIELD_LOGIN_RESP_ADDRESS2] = "add2",
    [FIELD_LOGIN_RESP_ADDRESS3] = "add3",
    [FIELD_LOGIN_RESP_COUNTRY] = "country",
    [FIELD_LOGIN_RESP_STATE] = "state",
    [FIELD_LOGIN_RESP_PINCODE] = "pincode",
    [FIELD_LOGIN_RESP_FACEBOOK_HANDLE] = "facebook_h",
    [FIELD_LOGIN_RESP_TWITTER_HANDLE] = "twitter_h",
    [FIELD_LOGIN_RESP_AVATAR_URL] = "url",
};

static const char *_table_avatar_resp_fields[] = {
    [FIELD_AVATAR_RESP_CODE] = "code",
    [FIELD_AVATAR_RESP_DESC] = "description",
};

#define table_field_at_index(tbl, findex)				\
    _table_##tbl##_fields[findex]

#define for_each_table_findex(findex, tbl)				\
    for (findex = 0; findex < ARRAY_SIZE(_table_##tbl##_fields); findex++)

#define json_parse_record_generator(name, tbl)				\
    int gweb_json_parse_record_##name (struct json_object *jobj,	\
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
		log_debug("<JSON-PARSE: (" #name ", " #tbl ")> "	\
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

#define json_dump_record_generator(name, tbl)				\
    static void gweb_json_dump_##tbl (struct j2c_##tbl##_msg *j2ctbl)	\
    {									\
	int findex;							\
									\
	for_each_table_findex(findex, tbl##_msg) {			\
	    log_debug("<JSON-PARSE: (" #name ", " #tbl ")> %s => %s\n", \
		      table_field_at_index(tbl##_msg, findex),	        \
		      j2ctbl->fields[findex]);				\
	}								\
    }

/*
 * Response generator allocates memory and expects the higher POST
 * responder to free it
 */
#define MAX_RESPONSE_BYTES (2048)

#define json_response_generator(name, tbl)                              \
    int gweb_json_gen_response_##tbl (j2c_resp_t *j2cresp,              \
                                      char **response)                  \
    {                                                                   \
        int findex, len = 0;                                            \
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
        len += sprintf(*response+len, "\"status\":{");                  \
                                                                        \
        for_each_table_findex(findex, tbl##_resp) {                     \
            if (j2ctbl->fields[findex]) {                               \
                len += sprintf(*response+len, "\"%s\":\"%s\",",         \
                          table_field_at_index(tbl##_resp, findex),     \
                          j2ctbl->fields[findex]);                      \
            }                                                           \
        }                                                               \
                                                                        \
        /* Remove trailing ',' */                                       \
        (*response)[len-1] = '}';                                       \
        (*response)[len] = '\0';                                        \
									\
        return 0;                                                       \
    }

/* Registration */
json_dump_record_generator(registration, registration)
json_parse_record_generator(registration, registration)
json_response_generator(registration, registration)

/* Profile */
json_dump_record_generator(profile, profile)
json_parse_record_generator(profile, profile)
json_response_generator(profile, profile)

/* Login */
json_dump_record_generator(login, login)
json_parse_record_generator(login, login)
json_response_generator(login, login)

/* Avatar */
json_dump_record_generator(avatar, avatar)
json_parse_record_generator(avatar, avatar)
json_response_generator(avatar, avatar)

struct json_map_info _j2c_map_info[] = {
    [JSON_C_REGISTRATION_MSG] = {
        .api_name         = "registration",
        .api_handler      = gweb_json_parse_record_registration,
        .api_resp_handler = gweb_json_gen_response_registration,
        .api_db_handler   = gweb_mysql_handle_registration,
        .api_db_resp_free = gweb_mysql_free_registration,
    },
    [JSON_C_PROFILE_MSG] = {
        .api_name         = "update_profile",
        .api_handler      = gweb_json_parse_record_profile,
        .api_resp_handler = gweb_json_gen_response_profile,
        .api_db_handler   = gweb_mysql_handle_profile,
        .api_db_resp_free = gweb_mysql_free_profile,
    },
    [JSON_C_LOGIN_MSG] = {
        .api_name         = "login",
        .api_handler      = gweb_json_parse_record_login,
        .api_resp_handler = gweb_json_gen_response_login,
        .api_db_handler   = gweb_mysql_handle_login,
        .api_db_resp_free = gweb_mysql_free_login,
    },
    [JSON_C_AVATAR_MSG] = {
        .api_name         = "update_avatar",
        .api_handler      = gweb_json_parse_record_avatar,
        .api_resp_handler = gweb_json_gen_response_avatar,
        .api_db_handler   = gweb_mysql_handle_avatar,
        .api_db_resp_free = gweb_mysql_free_avatar,
    },
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
    char *resp_string;
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

                /* TBD: Handle errors */
                /* Handle response structure from DB layer */
                if (j2cinfo->api_resp_handler && j2cresp != NULL) {
                    log_debug("<JSON-PARSE: post-processor> handling DB response: %s\n",
                              j2cinfo->api_name);
                    ret = (*j2cinfo->api_resp_handler)(j2cresp, response);
                }

                /* Free reponse structure from DB layer */
                if (j2cinfo->api_db_resp_free) {
                    (*j2cinfo->api_db_resp_free)(j2cresp);
                }
            }

            /* Always set POST processor status to be okay */
            if (status) {
                *status = 0;
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
