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
    int (*api_handler) (struct json_object *, j2c_map_t *);
    int (*api_db_handler) (j2c_map_t *);
};

/* Debug globals */
static int json_parse_dump = 1;

static const char *_table_registration_fields[] = {
    [FIELD_REGISTRATION_FNAME] = "fname",
    [FIELD_REGISTRATION_LNAME] = "lname",
    [FIELD_REGISTRATION_EMAIL] = "email",
    [FIELD_REGISTRATION_PHONE] = "phone",
    [FIELD_REGISTRATION_ADDRESS1] = "add1",
    [FIELD_REGISTRATION_ADDRESS2] = "add2",
    [FIELD_REGISTRATION_ADDRESS3] = "add3",
    [FIELD_REGISTRATION_COUNTRY] = "country",
    [FIELD_REGISTRATION_STATE] = "state",
    [FIELD_REGISTRATION_PINCODE] = "pincode",
    [FIELD_REGISTRATION_PASSWORD] = "password",
};

static const char *_table_login_fields[] = {
    [FIELD_LOGIN_EMAIL] = "email",
    [FIELD_LOGIN_PASSWORD] = "password",
};

#define table_field_at_index(tbl, findex)				\
    _table_##tbl##_fields[findex]

#define for_each_table_findex(findex, tbl)				\
    for (findex = 0; findex < ARRAY_SIZE(_table_##tbl##_fields); findex++)

#define json_parse_record_generator(name, tbl)				\
    int gweb_json_parse_record_##name (struct json_object *jobj,	\
				       j2c_map_t *j2cmap)		\
    {									\
	struct json_object *jfield;					\
	struct j2c_##tbl *j2ctbl = &j2cmap->tbl;			\
	int findex;							\
									\
	for_each_table_findex(findex, tbl) {				\
	    if (!json_object_object_get_ex(jobj,			\
			 table_field_at_index(tbl, findex), &jfield)) { \
		log_debug("<JSON-PARSE: (" #name ", " #tbl ")> "	\
			  "skipping field '%s' in message\n",		\
			  table_field_at_index(tbl, findex));		\
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
    static void gweb_json_dump_##tbl (struct j2c_##tbl *j2ctbl)		\
    {									\
	int findex;							\
									\
	for_each_table_findex(findex, tbl) {				\
	    log_debug("<JSON-PARSE: (" #name ", " #tbl ")> "		\
		      "%s ==> %s\n", table_field_at_index(tbl, findex),	\
		      j2ctbl->fields[findex]);				\
	}								\
    }

/* Debug dump */
json_dump_record_generator(registration, registration)
json_dump_record_generator(login, login)

/* Record parsers */
json_parse_record_generator(registration, registration)
json_parse_record_generator(login, login)

struct json_map_info _j2c_map_info[] = {
    [JSON_C_REGISTRATION_API] = {
	.api_name       = "registration",
	.api_handler    = gweb_json_parse_record_registration,
	.api_db_handler = gweb_mysql_handle_registration,
    },
    [JSON_C_LOGIN_API] = {
	.api_name       = "login",
	.api_handler    = gweb_json_parse_record_login,
	.api_db_handler = gweb_mysql_handle_login,
    },
};

/*
 * JSON POST processor
 *
 * Handle HTTP POST requests with application/json message
 */
int
gweb_json_post_processor (const char *data, size_t size)
{
    struct json_object *jobj = json_tokener_parse(data);
    struct json_object *jrecord;
    struct json_map_info *j2cinfo;
    j2c_map_t j2cmap;
    int api_index, ret = -1;

    if (!jobj) {
	log_error("<JSON-PARSE: post-processor> invalid json string!\n");
	return -1;
    }

    for (api_index = JSON_C_REGISTRATION_API; api_index < JSON_C_API_MAX;
	 api_index++) {

	j2cinfo = &_j2c_map_info[api_index];

	if (json_object_object_get_ex(jobj, j2cinfo->api_name, &jrecord)) {
	    if (j2cinfo->api_handler) {
		log_debug("<JSON-PARSE: post-processor> handling API: %s\n",
			  j2cinfo->api_name);
		ret = (*j2cinfo->api_handler)(jrecord, &j2cmap);
	    }
	    if (j2cinfo->api_db_handler) {
		log_debug("<JSON-PARSE: post-processor> handling API backend: %s\n",
			  j2cinfo->api_name);
		ret = (*j2cinfo->api_db_handler)(&j2cmap);
	    }
	}
    }
    return ret;
}
