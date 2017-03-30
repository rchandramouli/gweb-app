/*
 * Create/Initialize MYSQL tables
 *
 * NOTE: This could be auto-generated later
 */
#include <stdio.h>
#include <stdlib.h>

#include <mysql.h>

#define DEBUG
#include <gweb/common.h>
#include <gweb/mysqldb_conf.h>
#include <gweb/mysqldb_log.h>

/* SQL Macros */
#define DROP_TABLE_AppUsers						\
    "DROP TABLE IF EXISTS AppUsers"

#define CREATE_TABLE_AppUsers						\
    "CREATE TABLE AppUsers (FirstName VARCHAR(20), LastName VARCHAR(20), " \
    "Email VARCHAR(40) NOT NULL UNIQUE, Phone CHAR(10) NOT NULL, Address1 VARCHAR(40), " \
    "Address2 VARCHAR(40), Address3 VARCHAR(40), Country VARCHAR(20), " \
    "State VARCHAR(20), Pincode VARCHAR(6), Password VARCHAR(64), "	\
    "RegisteredOn DATE, LastUpdatedOn DATE, Validated BOOLEAN," \
    "PRIMARY KEY (`Phone`))"

/*
 * List of queries to run through to setup tables the first time. Add
 * user/db creation as well to the list.
 */
static const char *mysql_query_list[] = {
    DROP_TABLE_AppUsers,
    CREATE_TABLE_AppUsers,
};

int main (int argc, char *argv[])
{
    MYSQL *con;
    int query;

    log_debug("Initializing schema, MySQL version = %s\n",
	      mysql_get_client_info());

    if ((con = mysql_init(NULL)) == NULL) {
	report_mysql_error(con);
    }

    if (mysql_real_connect(con, MYSQL_DB_HOST, MYSQL_DB_USER,
			   MYSQL_DB_PASSWORD, MYSQL_DB_NAMESPACE,
			   0, NULL, CLIENT_MULTI_STATEMENTS) == NULL) {
	report_mysql_error(con);
    }

    log_debug("MySQL connected!\n");

    /* Run through all MySQL queries */
    for (query = 0; query < ARRAY_SIZE(mysql_query_list); query++) {
	if (mysql_query(con, mysql_query_list[query])) {
	    report_mysql_error(con);
	}
    }

    mysql_close(con);
    
    return 0;
}

/*
 * Local Variables:
 * compile-command:"gcc mysql_schema.c `mysql_config --cflags` `mysql_config --libs`"
 * End:
 */
