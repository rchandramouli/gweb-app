#ifndef JSON_API_H
#define JSON_API_H

#include <json-c/json_inttypes.h>
#include <json-c/json_object.h>
#include <json-c/json_tokener.h>

#include <gweb/json_struct.h>

extern int gweb_json_parse_record_registration (struct json_object *jobj,
					        j2c_msg_t *j2ctbl);

extern int gweb_json_parse_record_login (struct json_object *jobj,
					 j2c_msg_t *j2ctbl);

extern int gweb_json_parse_record_profile (struct json_object *jobj,
                                           j2c_msg_t *j2ctbl);

extern int gweb_json_gen_response_registration (j2c_resp_t *j2cresp,
                                                char **response);

extern int gweb_json_gen_response_login (j2c_resp_t *j2cresp,
                                         char **response);

extern int gweb_json_gen_response_profile (j2c_resp_t *j2cresp,
                                           char **response);

extern int gweb_json_post_processor (const char *data, size_t size,
                                     char **response, int *status);

extern int gweb_json_get_processor (void *connection, const char *url,
                                    char **response, int *status);

#endif // JSON_API_H
