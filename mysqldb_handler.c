/*
 * Handler routines for DB updates
 */
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>

#include <mysql.h>

#include <gweb/common.h>
#include <gweb/json_struct.h>
#include <gweb/json_api.h>
#include <gweb/mysqldb_api.h>
#include <gweb/mysqldb_conf.h>
#include <gweb/mysqldb_log.h>

/*
 * Handle registration record. The regn record is a list of pointers
 * to the given JSON string. Note that in case if we are handling the
 * registration asynchronously, this needs to be changed. We assume
 * that the *regn would appropriately point to the buffer which is
 * valid.
 */
#if 0
| FirstName     | varchar(20) | YES  |     | NULL    |       |
| LastName      | varchar(20) | YES  |     | NULL    |       |
| Email         | varchar(40) | NO   | UNI | NULL    |       |
| Phone         | char(10)    | NO   | PRI | NULL    |       |
| Address1      | varchar(40) | YES  |     | NULL    |       |
| Address2      | varchar(40) | YES  |     | NULL    |       |
| Address3      | varchar(40) | YES  |     | NULL    |       |
| Country       | varchar(20) | YES  |     | NULL    |       |
| State         | varchar(20) | YES  |     | NULL    |       |
| Pincode       | varchar(6)  | YES  |     | NULL    |       |
| Password      | varchar(64) | YES  |     | NULL    |       |
| RegisteredOn  | date        | YES  |     | NULL    |       |
| LastUpdatedOn | date        | YES  |     | NULL    |       |
| Validated     | tinyint(1)  | YES  |     | NULL    |       |
#endif

static MYSQL *g_mysql_ctx;

int
gweb_mysql_handle_registration (j2c_map_t *j2cmap)
{
    struct j2c_registration *jrecord = &j2cmap->registration;

    uint8_t qrybuf[MAX_MYSQL_QRYSZ];
    int len = 0;

    len += sprintf(qrybuf+len, "INSERT INTO AppUsers VALUES(");
    len += sprintf(qrybuf+len, "'%s',", jrecord->fields[0]);
    len += sprintf(qrybuf+len, "'%s',", jrecord->fields[1]);
    len += sprintf(qrybuf+len, "'%s',", jrecord->fields[2]);
    len += sprintf(qrybuf+len, "'%s',", jrecord->fields[3]);
    len += sprintf(qrybuf+len, "'%s',", jrecord->fields[4]);
    len += sprintf(qrybuf+len, "'%s',", jrecord->fields[5]);
    len += sprintf(qrybuf+len, "'%s',", jrecord->fields[6]);
    len += sprintf(qrybuf+len, "'%s',", jrecord->fields[7]);
    len += sprintf(qrybuf+len, "'%s',", jrecord->fields[8]);
    len += sprintf(qrybuf+len, "'%s',", jrecord->fields[9]);
    len += sprintf(qrybuf+len, "'%s',", jrecord->fields[10]);
    len += sprintf(qrybuf+len, "NULL, NULL, FALSE)");

    qrybuf[len] = '\0';

    if (mysql_query(g_mysql_ctx, qrybuf)) {
	report_mysql_error_noclose(g_mysql_ctx);
    }

    return 0;
}

int
gweb_mysql_handle_login (j2c_map_t *j2cmap)
{
    struct j2c_login *jrecord = &j2cmap->login;

    MYSQL_RES *result;

    uint8_t qrybuf[MAX_MYSQL_QRYSZ];
    int len = 0, ret = -1;

    len += sprintf(qrybuf+len, "SELECT * FROM AppUsers WHERE ");
    len += sprintf(qrybuf+len, "Email='%s' AND Password='%s'",
		   jrecord->fields[0], jrecord->fields[1]);

    qrybuf[len] = '\0';

    if (mysql_query(g_mysql_ctx, qrybuf)) {
	report_mysql_error_noclose(g_mysql_ctx);
    }

    if ((result = mysql_store_result(g_mysql_ctx)) == NULL) {
	report_mysql_error_noclose(g_mysql_ctx);
    }

    if (mysql_num_rows(result) == 1) {
	ret = 0;
    }

    mysql_free_result(result);

    return (ret);
}

int
gweb_mysql_shutdown (void)
{
    log_debug("Closing MySQL connection!\n");

    if (g_mysql_ctx)
	mysql_close(g_mysql_ctx);

    return 0;
}

int
gweb_mysql_init (void)
{
    log_debug("Initializing schema, MySQL version = %s\n",
	      mysql_get_client_info());

    if ((g_mysql_ctx = mysql_init(NULL)) == NULL) {
	report_mysql_error(g_mysql_ctx);
    }

    if (mysql_real_connect(g_mysql_ctx,
			   MYSQL_DB_HOST,
			   MYSQL_DB_USER,
			   MYSQL_DB_PASSWORD,
			   MYSQL_DB_NAMESPACE,
			   0,
			   NULL,
			   CLIENT_MULTI_STATEMENTS) == NULL) {
	report_mysql_error(g_mysql_ctx);
    }

    log_debug("MySQL connected!\n");

    return 0;
}
