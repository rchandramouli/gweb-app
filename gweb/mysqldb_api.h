#ifndef MYSQLDB_API_H
#define MYSQLDB_API_H

#include <gweb/json_struct.h>

#define MAX_MYSQL_QRYSZ    1024

/* MySQL transaction error code */
enum {
    MYSQL_STATUS_OK = 0,
    MYSQL_STATUS_FAIL = 1,
};

/*
 * MySQL APIs for queries
 */
extern int gweb_mysql_handle_registration (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp);
extern int gweb_mysql_handle_login (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp);
extern int gweb_mysql_handle_profile (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp);
extern int gweb_mysql_handle_avatar (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp);
extern int gweb_mysql_handle_cxn_request (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp);
extern int gweb_mysql_handle_cxn_channel (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp);
extern int gweb_mysql_handle_cxn_request_query (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp);
extern int gweb_mysql_handle_cxn_channel_query (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp);

extern int gweb_mysql_free_registration (j2c_resp_t *j2cresp);
extern int gweb_mysql_free_login (j2c_resp_t *j2cresp);
extern int gweb_mysql_free_profile (j2c_resp_t *j2cresp);
extern int gweb_mysql_free_avatar (j2c_resp_t *j2cresp);
extern int gweb_mysql_free_cxn_request (j2c_resp_t *j2cresp);
extern int gweb_mysql_free_cxn_channel (j2c_resp_t *j2cresp);
extern int gweb_mysql_free_cxn_request_query (j2c_resp_t *j2cresp);
extern int gweb_mysql_free_cxn_channel_query (j2c_resp_t *j2cresp);

extern int gweb_mysql_ping (void);
extern int gweb_mysql_init (void);
extern int gweb_mysql_shutdown (void);
			   
#endif // MYSQLDB_API_H
