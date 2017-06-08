/*
 * Create/Initialize MYSQL tables
 *
 * NOTE: This could be auto-generated later
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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

#define DROP_TABLE_UserAddress                  \
    "DROP TABLE IF EXISTS UserAddress"

#define DROP_TABLE_UserPhone                    \
    "DROP TABLE IF EXISTS UserPhone"

#define DROP_TABLE_UserSocialNetwork            \
    "DROP TABLE IF EXISTS UserSocialNetwork"

#define DROP_TABLE_UserConnectRequest           \
    "DROP TABLE IF EXISTS UserConnectRequest"

#define DROP_TABLE_UserConnectChannel           \
    "DROP TABLE IF EXISTS UserConnectChannel"

#define CREATE_TABLE_UserRegInfo                                        \
    "CREATE TABLE IF NOT EXISTS UserRegInfo (UID VARCHAR(16) NOT NULL UNIQUE, "	\
    "FirstName VARCHAR(20), LastName VARCHAR(20), "                     \
    "Email VARCHAR(40) NOT NULL UNIQUE, StartDate DATETIME, "           \
    "EndDate DATETIME, Password VARCHAR(64), Validated BOOLEAN, "       \
    "UIDVersion INTEGER, DataVersion INTEGER, AvatarURL VARCHAR(100), " \
    "MobileIP VARCHAR(45), PRIMARY KEY (`UID`))"

#define CREATE_TABLE_UserAddress                                        \
    "CREATE TABLE IF NOT EXISTS UserAddress (UID VARCHAR(16) NOT NULL, " \
    "AddressType VARCHAR(10), Address1 VARCHAR(40), Address2 VARCHAR(40), " \
    "Address3 VARCHAR(40), State VARCHAR(20), Pincode VARCHAR(6), "     \
    "Country VARCHAR(20))"

#define CREATE_TABLE_UserPhone                                  \
    "CREATE TABLE IF NOT EXISTS UserPhone (UID VARCHAR(16) NOT NULL, "	\
    "PhoneType VARCHAR(10), Phone VARCHAR(10) NOT NULL)"

#define CREATE_TABLE_UserSocialNetwork                                  \
    "CREATE TABLE IF NOT EXISTS UserSocialNetwork (UID VARCHAR(16) NOT NULL, " \
    "NetworkType VARCHAR(10) NOT NULL, NetworkHandle VARCHAR(40) NOT NULL)"

#define CREATE_TABLE_UserConnectRequest					\
    "CREATE TABLE IF NOT EXISTS UserConnectRequest (FromUID VARCHAR(16) NOT NULL, " \
    "ToUID VARCHAR(16) NOT NULL, SentOn DATETIME, Flags VARCHAR(10))"

#define CREATE_TABLE_UserConnectChannel                                 \
    "CREATE TABLE IF NOT EXISTS UserConnectChannel (FromUID VARCHAR(16) NOT NULL, " \
    "ToUID VARCHAR(16) NOT NULL, ConnectedOn DATETIME, ChannelId VARCHAR(16) NOT NULL)"

/*
 * List of queries to run through to setup tables the first time. Add
 * user/db creation as well to the list.
 */
static const char *mysql_drop_query[] = {
    DROP_TABLE_UserRegInfo,
    DROP_TABLE_UserAddress,
    DROP_TABLE_UserPhone,
    DROP_TABLE_UserSocialNetwork,
    DROP_TABLE_UserConnectRequest,
    DROP_TABLE_UserConnectChannel,
};

static const char *mysql_create_query[] = {
    CREATE_TABLE_UserRegInfo,
    CREATE_TABLE_UserAddress,
    CREATE_TABLE_UserPhone,
    CREATE_TABLE_UserSocialNetwork,
    CREATE_TABLE_UserConnectRequest,
    CREATE_TABLE_UserConnectChannel,
};

int main (int argc, char *argv[])
{
    MYSQL *con;
    int query, drop_tables = 0;

    log_debug("Initializing schema, MySQL version = %s\n",
	      mysql_get_client_info());

    if (argc > 1 && (argv[1][0] == '-' && argv[1][1] == 'd')) {
	drop_tables = 1;
    }
    
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
    if (drop_tables) {
	for (query = 0; query < ARRAY_SIZE(mysql_drop_query); query++) {
	    if (mysql_query(con, mysql_drop_query[query])) {
		report_mysql_error(con);
	    }
	}
    }

    for (query = 0; query < ARRAY_SIZE(mysql_create_query); query++) {
	if (mysql_query(con, mysql_create_query[query])) {
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
