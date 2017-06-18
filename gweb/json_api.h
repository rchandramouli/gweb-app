#ifndef JSON_API_H
#define JSON_API_H

#include <json-c/json_inttypes.h>
#include <json-c/json_object.h>
#include <json-c/json_tokener.h>

#include <gweb/json_struct.h>

extern int gweb_json_post_processor (const char *data, size_t size,
                                     char **response, int *status);

extern int gweb_json_get_processor (void *connection, const char *url,
                                    char **response, int *status);

#endif // JSON_API_H
