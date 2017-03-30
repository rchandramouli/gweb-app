#ifndef JSON_API_H
#define JSON_API_H

#include <json-c/json_inttypes.h>
#include <json-c/json_object.h>
#include <json-c/json_tokener.h>

#include <gweb/json_struct.h>

extern int gweb_json_parse_record_registration (struct json_object *jobj,
					        j2c_map_t *j2ctbl);

extern int gweb_json_parse_record_login (struct json_object *jobj,
					 j2c_map_t *j2ctbl);

extern int gweb_json_post_processor (const char *data, size_t size);

#endif // JSON_API_H
