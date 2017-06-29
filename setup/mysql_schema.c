/*
 * Create/Initialize MYSQL tables
 *
 * NOTE: This could be auto-generated later
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <mysql.h>

#ifndef DEBUG
#  define DEBUG
#endif

#include <gweb/common.h>
#include <gweb/config.h>
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

#define DROP_TABLE_UserConnectPreferences               \
    "DROP TABLE IF EXISTS UserConnectPreferences"

#define CREATE_TABLE_UserRegInfo                                        \
    "CREATE TABLE IF NOT EXISTS UserRegInfo (UID VARCHAR(16) BINARY "   \
    "NOT NULL UNIQUE, FirstName VARCHAR(20), LastName VARCHAR(20), "    \
    "Email VARCHAR(40) NOT NULL UNIQUE, StartDate DATETIME, "           \
    "EndDate DATETIME, Password VARCHAR(64), Validated BOOLEAN, "       \
    "UIDVersion INTEGER, DataVersion INTEGER, AvatarURL VARCHAR(100), " \
    "MobileIP VARCHAR(45), PRIMARY KEY (`UID`))"

#define CREATE_TABLE_UserAddress                                        \
    "CREATE TABLE IF NOT EXISTS UserAddress (UID VARCHAR(16) BINARY NOT NULL, " \
    "AddressType VARCHAR(10), Address1 VARCHAR(40), Address2 VARCHAR(40), " \
    "Address3 VARCHAR(40), State VARCHAR(20), Pincode VARCHAR(6), "     \
    "Country VARCHAR(20))"

#define CREATE_TABLE_UserPhone                                          \
    "CREATE TABLE IF NOT EXISTS UserPhone (UID VARCHAR(16) BINARY NOT NULL, " \
    "PhoneType VARCHAR(10), Phone VARCHAR(10) NOT NULL)"

#define CREATE_TABLE_UserSocialNetwork                                  \
    "CREATE TABLE IF NOT EXISTS UserSocialNetwork (UID VARCHAR(16) BINARY NOT NULL, " \
    "NetworkType VARCHAR(10) NOT NULL, NetworkHandle VARCHAR(40) NOT NULL)"

#define CREATE_TABLE_UserConnectRequest					\
    "CREATE TABLE IF NOT EXISTS UserConnectRequest (FromUID VARCHAR(16) BINARY NOT NULL, " \
    "ToUID VARCHAR(16) NOT NULL, SentOn DATETIME, Flags VARCHAR(10))"

#define CREATE_TABLE_UserConnectChannel                                 \
    "CREATE TABLE IF NOT EXISTS UserConnectChannel (FromUID VARCHAR(16) BINARY NOT NULL, " \
    "ToUID VARCHAR(16) NOT NULL, ConnectedOn DATETIME, ChannelId VARCHAR(16) NOT NULL)"

#define CREATE_TABLE_UserConnectPreferences                             \
    "CREATE TABLE IF NOT EXISTS UserConnectPreferences (UID VARCHAR(16) BINARY NOT NULL, " \
    "ChannelId VARCHAR(16) NOT NULL, ChannelFlags VARCHAR(16) NOT NULL)"

#define ALTER_TABLE_V2_UserRegInfo                                      \
    "ALTER TABLE UserRegInfo ADD ProfileFlags VARCHAR(10) DEFAULT 'public'"

#define ALTER_TABLE_UserRegInfo                                         \
    "ALTER TABLE UserRegInfo CHANGE UID UID VARCHAR(16) BINARY NOT NULL UNIQUE"

#define ALTER_TABLE_UserAddress                                         \
    "ALTER TABLE UserAddress CHANGE UID UID VARCHAR(16) BINARY NOT NULL"

#define ALTER_TABLE_UserPhone                                           \
    "ALTER TABLE UserPhone CHANGE UID UID VARCHAR(16) BINARY NOT NULL"

#define ALTER_TABLE_UserSocialNetwork                                   \
    "ALTER TABLE UserSocialNetwork CHANGE UID UID VARCHAR(16) BINARY NOT NULL"

#define ALTER_TABLE_UserConnectRequest                                  \
    "ALTER TABLE UserConnectRequest "                                   \
    "CHANGE FromUID FromUID VARCHAR(16) BINARY NOT NULL, "              \
    "CHANGE ToUID ToUID VARCHAR(16) BINARY NOT NULL"

#define ALTER_TABLE_UserConnectChannel                                  \
    "ALTER TABLE UserConnectChannel "                                   \
    "CHANGE FromUID FromUID VARCHAR(16) BINARY NOT NULL, "              \
    "CHANGE ToUID ToUID VARCHAR(16) BINARY NOT NULL"

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
    DROP_TABLE_UserConnectPreferences,
};

/* Version 0 */
static const char *mysql_db_update_v0[] = {
    CREATE_TABLE_UserRegInfo,
    CREATE_TABLE_UserAddress,
    CREATE_TABLE_UserPhone,
    CREATE_TABLE_UserSocialNetwork,
    CREATE_TABLE_UserConnectRequest,
    CREATE_TABLE_UserConnectChannel,
};

/* Version 1 */
static const char *mysql_db_update_v1[] = {
    ALTER_TABLE_UserRegInfo,
    ALTER_TABLE_UserAddress,
    ALTER_TABLE_UserPhone,
    ALTER_TABLE_UserSocialNetwork,
    ALTER_TABLE_UserConnectRequest,
    ALTER_TABLE_UserConnectChannel,
};

static const char *mysql_db_update_v2[] = {
    CREATE_TABLE_UserConnectPreferences,
    ALTER_TABLE_V2_UserRegInfo,
};

static struct mysql_config *g_mysql_cfg;

#define MYSQL_RUN_QUERY(q, ctx, table)            \
    do {                                          \
        for (q = 0; q < ARRAY_SIZE(table); q++) { \
            if (mysql_query(ctx, table[q])) {     \
                report_mysql_error(ctx);          \
            }                                     \
        }                                         \
    } while(0)

int main (int argc, char *argv[])
{
    MYSQL *con;
    int query, drop_tables = 0, version = -1;

    log_debug("Initializing schema, MySQL version = %s\n",
	      mysql_get_client_info());

    if (argc > 1 && (argv[1][0] == '-' && argv[1][1] == 'd')) {
	drop_tables = 1;
    }

    if (argc > 1) {
        if (strcmp(argv[1], "-v0") == 0) {
            version = 0;
        } else if (strcmp(argv[1], "-v1") == 0) {
            version = 1;
        } else if (strcmp(argv[1], "-v2") == 0) {
            version = 2;
        }
    }

    if ((con = mysql_init(NULL)) == NULL) {
	report_mysql_error(con);
    }

    if (config_parse_and_load(argc, argv)) {
        log_debug("Parse error loading MySQL connect info, exiting.\n");
        return -1;
    }

    if ((g_mysql_cfg = config_load_mysqldb()) == NULL) {
        log_debug("No MySQL connect information found, exiting.\n");
        return -1;
    }

    if (mysql_real_connect(con,
                           g_mysql_cfg->host,
                           g_mysql_cfg->username,
                           g_mysql_cfg->password,
                           g_mysql_cfg->database,
                           0, NULL, CLIENT_MULTI_STATEMENTS) == NULL) {
	report_mysql_error(con);
    }

    log_debug("MySQL connected!\n");

    /* Run through all MySQL queries */
    if (drop_tables) {
        MYSQL_RUN_QUERY(query, con, mysql_drop_query);
    }

    /* Pick the update version to run */
    switch (version) {
    case 0:
        MYSQL_RUN_QUERY(query, con, mysql_db_update_v0);
        break;
    case 1:
        MYSQL_RUN_QUERY(query, con, mysql_db_update_v1);
        break;
    case 2:
        MYSQL_RUN_QUERY(query, con, mysql_db_update_v2);
        break;
    default:
        break;
    }

    mysql_close(con);

    return 0;
}

/*
 * Local Variables:
 * compile-command:"gcc mysql_schema.c `mysql_config --cflags` `mysql_config --libs`"
 * End:
 */
