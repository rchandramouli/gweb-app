#ifndef MYSQLDB_API_H
#define MYSQLDB_API_H

#include <gweb/json_struct.h>

#define MAX_MYSQL_QRYSZ    1024

/*
 * MySQL APIs for queries
 */
extern int gweb_mysql_handle_registration (j2c_map_t *j2cmap);
extern int gweb_mysql_handle_login (j2c_map_t *j2cmap);

extern int gweb_mysql_init (void);
extern int gweb_mysql_shutdown (void);
			   
#endif // MYSQLDB_API_H
