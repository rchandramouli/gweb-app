/*
 * Create/Initialize MYSQL tables
 *
 * NOTE: This could be auto-generated later
 */
#include <stdio.h>
#include <stdlib.h>

#include <mysql.h>

#ifndef DEBUG
#  define DEBUG
#endif

#include <gweb/common.h>
#include <gweb/mysqldb_conf.h>
#include <gweb/mysqldb_log.h>

/* SQL Macros */
#define DROP_TABLE_UserRegInfo                  \
    "DROP TABLE IF EXISTS UserRegInfo"

#define CREATE_TABLE_UserRegInfo                                        \
    "CREATE TABLE UserRegInfo (UID VARCHAR(16) NOT NULL UNIQUE, "       \
    "FirstName VARCHAR(20), LastName VARCHAR(20), "                     \
    "Email VARCHAR(40) NOT NULL UNIQUE, StartDate DATETIME, "           \
    "EndDate DATETIME, Password VARCHAR(64), Validated BOOLEAN, "       \
    "UIDVersion INTEGER, DataVersion INTEGER, AvatarURL VARCHAR(100), " \
    "MobileIP VARCHAR(45), PRIMARY KEY (`UID`))"

#define DROP_TABLE_UserAddress                  \
    "DROP TABLE IF EXISTS UserAddress"

#define CREATE_TABLE_UserAddress                                        \
    "CREATE TABLE UserAddress (UID VARCHAR(16) NOT NULL, "              \
    "AddressType VARCHAR(10), Address1 VARCHAR(40), Address2 VARCHAR(40), " \
    "Address3 VARCHAR(40), State VARCHAR(20), Pincode VARCHAR(6), "     \
    "Country VARCHAR(20))"

#define DROP_TABLE_UserPhone                    \
    "DROP TABLE IF EXISTS UserPhone"

#define CREATE_TABLE_UserPhone                                  \
    "CREATE TABLE UserPhone (UID VARCHAR(16) NOT NULL, "        \
    "PhoneType VARCHAR(10), Phone VARCHAR(10) NOT NULL)"

#define DROP_TABLE_UserSocialNetwork            \
    "DROP TABLE IF EXISTS UserSocialNetwork"

#define CREATE_TABLE_UserSocialNetwork                                  \
    "CREATE TABLE UserSocialNetwork (UID VARCHAR(16) NOT NULL, "        \
    "NetworkType VARCHAR(10) NOT NULL, NetworkHandle VARCHAR(40) NOT NULL)"

/*
 * List of queries to run through to setup tables the first time. Add
 * user/db creation as well to the list.
 */
static const char *mysql_query_list[] = {
    DROP_TABLE_UserRegInfo,
    DROP_TABLE_UserAddress,
    DROP_TABLE_UserPhone,
    DROP_TABLE_UserSocialNetwork,

    CREATE_TABLE_UserRegInfo,
    CREATE_TABLE_UserAddress,
    CREATE_TABLE_UserPhone,
    CREATE_TABLE_UserSocialNetwork,
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
