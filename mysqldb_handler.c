/*
 * Handler routines for DB updates
 */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include <mysql.h>

#include <gweb/common.h>
#include <gweb/json_struct.h>
#include <gweb/json_api.h>
#include <gweb/mysqldb_api.h>
#include <gweb/mysqldb_conf.h>
#include <gweb/mysqldb_log.h>
#include <gweb/uid.h>

/* Misc macros */
#define MAX_DATETIME_STRSZ   (20)

/* MySQL transaction status */
enum {
    GWEB_MYSQL_ERR_NO_RECORD = 1,
    GWEB_MYSQL_ERR_UNKNOWN,
    GWEB_MYSQL_ERR_DUPLICATE,
    GWEB_MYSQL_OK,
};

static MYSQL *g_mysql_ctx;

/* Atomic transactions -- depends on the backend storage engine
 * (eg. InnoDB)
 */
static inline void
gweb_mysql_start_transaction (void)
{
    gweb_mysql_ping();

    mysql_query(g_mysql_ctx, "START TRANSACTION");
}

/* Assume Abort/Commit is done before timeout and there is no need to
 * do ping again. Need to see how mysql behaves when ping is done
 * between the transaction block.
 */
static inline void
gweb_mysql_abort_transaction (void)
{
    /* gweb_mysql_ping(); */

    mysql_query(g_mysql_ctx, "ROLLBACK");
}

static inline void
gweb_mysql_commit_transaction (void)
{
    /* gweb_mysql_ping(); */

    mysql_query(g_mysql_ctx, "COMMIT");
}

static void
gweb_get_utc_datetime (char *dtbuf)
{
    time_t now;
    struct tm *tm_info;

    time(&now);
    tm_info = gmtime(&now);

    strftime(dtbuf, MAX_DATETIME_STRSZ, "%Y-%m-%d %H:%M:%S", tm_info);
}

static j2c_resp_t *
gweb_mysql_allocate_response (void)
{
    return calloc(1, sizeof(j2c_resp_t));
}

/*
 * Map MYSQL to HTTP code here (NOTE: needed a cleaner abstraction)
 */
static void
gweb_mysql_get_common_response (int mysql_code,
                                char **code, char **desc)
{
    switch (mysql_code) {
    case GWEB_MYSQL_ERR_NO_RECORD:
        *code = "404";
        *desc = "Record Not Found";
        break;

    case GWEB_MYSQL_ERR_UNKNOWN:
        *code = "404";
        *desc = "Unknown Error";
        break;

    case GWEB_MYSQL_ERR_DUPLICATE:
        *code = "404";
        *desc = "Duplicate Entry";
        break;

    case GWEB_MYSQL_OK:
        *code = "200";
        *desc = "OK";
        break;

    default:
        *code = NULL;
        *desc = NULL;
        break;
    }
}

#define generate_default_response(type, macro, code)                    \
    {                                                                   \
        struct j2c_##type##_resp *table;                                \
                                                                        \
        resp = gweb_mysql_allocate_response();                          \
        table = &resp->type;                                            \
        gweb_mysql_get_common_response(code,                            \
                       &table->fields[FIELD_##macro##_RESP_CODE],       \
                       &table->fields[FIELD_##macro##_RESP_DESC]);      \
    }

static void
gweb_mysql_prepare_response (int resp_type, int mysql_code,
                             j2c_resp_t **response)
{
    j2c_resp_t *resp = NULL;

    if (response == NULL) {
        return;
    }

    switch (resp_type) {
    case JSON_C_REGISTRATION_RESP:
        generate_default_response(registration, REGISTRATION, mysql_code);
        break;

    case JSON_C_PROFILE_RESP:
        generate_default_response(profile, PROFILE, mysql_code);
        break;

    case JSON_C_LOGIN_RESP:
        generate_default_response(login, LOGIN, mysql_code);
        break;

    case JSON_C_AVATAR_RESP:
        generate_default_response(avatar, AVATAR, mysql_code);
        break;

    default:
        return;
    }

    *response = resp;

    return;
}

#define PUSH_BUF(buf, len, fmt...)             \
    len += sprintf(buf+len, fmt)

/* For APIs requiring just a peek of record to see if one or more rows
 * exists.
 */
static int
gweb_mysql_get_query_count (const char *query)
{
    MYSQL_RES *result;
    int ret;

    if (mysql_query(g_mysql_ctx, query)) {
        report_mysql_error_noclose(g_mysql_ctx);
    }

    if ((result = mysql_store_result(g_mysql_ctx)) == NULL) {
        report_mysql_error_noclose(g_mysql_ctx);
    }

    ret = mysql_num_rows(result);

    mysql_free_result(result);

    return ret;
}

static int
gweb_mysql_check_uid_email (const char *uid_str, const char *email)
{
    uint8_t qrybuf[MAX_MYSQL_QRYSZ];
    int len = 0;

    if (email) {
        /* Check if UID/E-Mail is already registered */
        PUSH_BUF(qrybuf, len, "SELECT * FROM UserRegInfo WHERE "
                 "UID='%s' OR Email='%s'", uid_str, email);
    } else {
        /* Check if UID is already registered */
        PUSH_BUF(qrybuf, len, "SELECT * FROM UserRegInfo WHERE UID='%s'",
                 uid_str);
    }
    qrybuf[len] = '\0';

    gweb_mysql_ping();

    return gweb_mysql_get_query_count(qrybuf);
}

int
gweb_mysql_handle_registration (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    struct j2c_registration_msg *jrecord = &j2cmsg->registration;

    uint8_t qrybuf[MAX_MYSQL_QRYSZ], *db_qry1, *db_qry2;
    uint8_t uid_str[MAX_UID_STRSZ], utc_dt_str[MAX_DATETIME_STRSZ];
    int len = 0, ret;

    /* Get UID for this registration */
    gweb_app_get_uid_str(jrecord->fields[FIELD_REGISTRATION_PHONE],
                         jrecord->fields[FIELD_REGISTRATION_EMAIL],
                         uid_str);

    ret = gweb_mysql_check_uid_email((const char *)uid_str,
                               jrecord->fields[FIELD_REGISTRATION_EMAIL]);
    if (ret < 0) {
        gweb_mysql_prepare_response(JSON_C_REGISTRATION_RESP,
                                    GWEB_MYSQL_ERR_UNKNOWN,
                                    j2cresp);
        return 0;
    } else if (ret > 0) {
        /* Duplicate record */
        gweb_mysql_prepare_response(JSON_C_REGISTRATION_RESP,
                                    GWEB_MYSQL_ERR_DUPLICATE,
                                    j2cresp);
        return 0;
    }

    /* UID is not already registered, update database */
    len = 0;
    db_qry1 = &qrybuf[len];
    PUSH_BUF(qrybuf, len, "INSERT INTO UserRegInfo ");
    PUSH_BUF(qrybuf, len, "(UID, FirstName, LastName, Email, StartDate, ");
    PUSH_BUF(qrybuf, len, "Password) VALUES ");

    /* NOTE: Should this datetime be sent from the application
     * layer?
     */
    gweb_get_utc_datetime(utc_dt_str);

    PUSH_BUF(qrybuf, len, "('%s', '%s', '%s', '%s', '%s', '%s')",
             uid_str,
             jrecord->fields[FIELD_REGISTRATION_FNAME],
             jrecord->fields[FIELD_REGISTRATION_LNAME],
             jrecord->fields[FIELD_REGISTRATION_EMAIL],
             utc_dt_str,
             jrecord->fields[FIELD_REGISTRATION_PASSWORD]);

    qrybuf[len++] = '\0';

    db_qry2 = &qrybuf[len];
    PUSH_BUF(qrybuf, len, "INSERT INTO UserPhone ");
    PUSH_BUF(qrybuf, len, "(UID, PhoneType, Phone) VALUES ");

    /* NOTE: default phone type set to mobile */
    PUSH_BUF(qrybuf, len, "('%s', 'mobile', '%s')",
             uid_str,
             jrecord->fields[FIELD_REGISTRATION_PHONE]);
    qrybuf[len++] = '\0';

    gweb_mysql_start_transaction();

    if (mysql_query(g_mysql_ctx, db_qry1)) {
        goto __abort_transaction;
    }

    if (mysql_query(g_mysql_ctx, db_qry2)) {
        goto __abort_transaction;
    }

    gweb_mysql_commit_transaction();

    gweb_mysql_prepare_response(JSON_C_REGISTRATION_RESP,
                                GWEB_MYSQL_OK,
                                j2cresp);

    (*j2cresp)->registration.fields[FIELD_REGISTRATION_RESP_UID] =
                strndup(uid_str, strlen(uid_str));

    return 0;

__abort_transaction:
    report_mysql_error_noclose(g_mysql_ctx);
    gweb_mysql_abort_transaction();
    gweb_mysql_prepare_response(JSON_C_REGISTRATION_RESP,
                                GWEB_MYSQL_ERR_UNKNOWN,
                                j2cresp);
    return 0;
}

int
gweb_mysql_free_registration (j2c_resp_t *j2cresp)
{
    if (j2cresp) {
        struct j2c_registration_resp *resp = &j2cresp->registration;

        if (resp->fields[FIELD_REGISTRATION_RESP_UID])
            free(resp->fields[FIELD_REGISTRATION_RESP_UID]);
        free(j2cresp);
    }
    return 0;
}

int
gweb_mysql_handle_login (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    struct j2c_login_msg *jrecord = &j2cmsg->login;

    MYSQL_RES *result;

    uint8_t qrybuf[MAX_MYSQL_QRYSZ];
    int len = 0, ret = -1;

    PUSH_BUF(qrybuf, len, "SELECT UID FROM UserRegInfo WHERE ");
    PUSH_BUF(qrybuf, len, "Email='%s' AND Password='%s'",
             jrecord->fields[FIELD_LOGIN_EMAIL],
             jrecord->fields[FIELD_LOGIN_PASSWORD]);
    qrybuf[len] = '\0';

    gweb_mysql_ping();

    if (mysql_query(g_mysql_ctx, qrybuf)) {
	report_mysql_error_noclose(g_mysql_ctx);
        goto __bail_out;
    }

    if ((result = mysql_store_result(g_mysql_ctx)) == NULL) {
	report_mysql_error_noclose(g_mysql_ctx);
        goto __bail_out;
    }

    if (mysql_num_rows(result) == 1) {
        MYSQL_ROW row;

        if ((row = mysql_fetch_row(result)) == NULL) {
            mysql_free_result(result);
            goto __bail_out;
        }

        gweb_mysql_prepare_response(JSON_C_LOGIN_RESP,
                                    GWEB_MYSQL_OK,
                                    j2cresp);

        (*j2cresp)->login.fields[FIELD_LOGIN_RESP_UID] =
            strndup(row[0], strlen(row[0]));
    }
    mysql_free_result(result);

    return 0;

__bail_out:
    gweb_mysql_prepare_response(JSON_C_LOGIN_RESP,
                                GWEB_MYSQL_ERR_UNKNOWN,
                                j2cresp);
    return 0;
}

int
gweb_mysql_free_login (j2c_resp_t *j2cresp)
{
    if (j2cresp) {
        struct j2c_login_resp *resp = &j2cresp->login;

        if (resp->fields[FIELD_LOGIN_RESP_UID])
            free(resp->fields[FIELD_LOGIN_RESP_UID]);
        free(j2cresp);
    }
    return 0;
}

int
gweb_mysql_handle_avatar (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    struct j2c_avatar_msg *jrecord = &j2cmsg->avatar;

    uint8_t qrybuf[MAX_MYSQL_QRYSZ];
    int len = 0, ret;

    /* Check if UID is registered */
    ret = gweb_mysql_check_uid_email(jrecord->fields[FIELD_PROFILE_UID], NULL);
    if (ret <= 0) {
        report_mysql_error_noclose(g_mysql_ctx);
        goto __bail_out;
    }

    PUSH_BUF(qrybuf, len,
             "UPDATE UserRegInfo SET AvatarURL='%s' WHERE UID='%s'",
             jrecord->fields[FIELD_AVATAR_URL],
             jrecord->fields[FIELD_AVATAR_UID]);
    qrybuf[len] = '\0';

    gweb_mysql_ping();

    gweb_mysql_start_transaction();

    if (mysql_query(g_mysql_ctx, qrybuf)) {
        goto __abort_transaction;
    }

    gweb_mysql_commit_transaction();

    gweb_mysql_prepare_response(JSON_C_AVATAR_RESP,
                                GWEB_MYSQL_OK,
                                j2cresp);
    return 0;

__abort_transaction:
    gweb_mysql_abort_transaction();
__bail_out:
    gweb_mysql_prepare_response(JSON_C_AVATAR_RESP,
                                GWEB_MYSQL_ERR_NO_RECORD,
                                j2cresp);
    return 0;
}

int
gweb_mysql_free_avatar (j2c_resp_t *j2cresp)
{
    if (j2cresp) {
        free(j2cresp);
    }
    return 0;
}

static int
gweb_mysql_query_social_network (const char *uid, const char *sn_type)
{
    uint8_t qrybuf[MAX_MYSQL_QRYSZ];
    int len = 0, ret;

    PUSH_BUF(qrybuf, len, "SELECT UID from UserSocialNetwork WHERE UID='%s' "
             "AND NetworkType='%s'", uid, sn_type);
    qrybuf[len] = '\0';

    return gweb_mysql_get_query_count(qrybuf);
}

static int
gweb_mysql_update_social_network (char *qrybuf, struct j2c_profile_msg *jrecord,
                                  const char *sn_type, int field_idx, int found)
{
    int len = 0;

    if (strlen(jrecord->fields[field_idx]) == 0) {
        PUSH_BUF(qrybuf, len, "DELETE FROM UserSocialNetwork WHERE UID='%s' "
                 "AND NetworkType='%s'",
                 jrecord->fields[FIELD_PROFILE_UID],
                 sn_type);
    } else {
        if (found) {
            PUSH_BUF(qrybuf, len,
                     "UPDATE UserSocialNetwork SET NetworkHandle='%s' "
                     "WHERE UID='%s' AND NetworkType='%s'",
                     jrecord->fields[field_idx],
                     jrecord->fields[FIELD_PROFILE_UID],
                     sn_type);
        } else {
            PUSH_BUF(qrybuf, len,
                     "INSERT INTO UserSocialNetwork (UID, NetworkType, "
                     "NetworkHandle) VALUES ('%s', '%s', '%s')",
                     jrecord->fields[FIELD_PROFILE_UID],
                     sn_type,
                     jrecord->fields[field_idx]);
        }
    }
    qrybuf[len] = '\0';

    return len;
}

int
gweb_mysql_handle_profile (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    struct j2c_profile_msg *jrecord = &j2cmsg->profile;

    uint8_t qrybuf[MAX_MYSQL_QRYSZ], *db_qry1, *db_qry2, *db_qry3;
    int record_found = 0, sn_facebook_found = 0, sn_twitter_found = 0;
    int len = 0, tmp_len, ret;
    int bail_out_err = GWEB_MYSQL_ERR_UNKNOWN;

    /* Check if UID is registered */
    ret = gweb_mysql_check_uid_email(jrecord->fields[FIELD_PROFILE_UID], NULL);
    if (ret <= 0) {
        report_mysql_error_noclose(g_mysql_ctx);
        bail_out_err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    /*
     * NOTE: address type hardcoded as permanent for now
     */
    PUSH_BUF(qrybuf, len,
             "SELECT UID from UserAddress WHERE UID='%s' AND AddressType='%s'",
             jrecord->fields[FIELD_PROFILE_UID],
             "permanent");
    qrybuf[len] = '\0';

    gweb_mysql_ping();

    /* Check if the record exists */
    ret = gweb_mysql_get_query_count(qrybuf);
    if (ret < 0) {
        report_mysql_error_noclose(g_mysql_ctx);
        goto __bail_out;
    }
    record_found = ret;

    ret = gweb_mysql_query_social_network(jrecord->fields[FIELD_PROFILE_UID],
                                          "facebook");
    if (ret < 0) {
        report_mysql_error_noclose(g_mysql_ctx);
        goto __bail_out;
    }
    sn_facebook_found = ret;

    ret = gweb_mysql_query_social_network(jrecord->fields[FIELD_PROFILE_UID],
                                          "twitter");
    if (ret < 0) {
        report_mysql_error_noclose(g_mysql_ctx);
        goto __bail_out;
    }
    sn_twitter_found = ret;

    db_qry2 = db_qry3 = NULL;

    /* Update record if exists or else, insert. Note that, we have a
     * case where there is no primary key in this table. Check for
     * other social network handles.
     */
    len = 0;
    db_qry1 = &qrybuf[len];

    if (record_found) {
        PUSH_BUF(qrybuf, len, "UPDATE UserAddress SET Address1='%s', "
                 "Address2='%s', Address3='%s', State='%s', Pincode='%s', "
                 "Country='%s' WHERE UID='%s' AND AddressType='%s'",
                 jrecord->fields[FIELD_PROFILE_ADDRESS1],
                 jrecord->fields[FIELD_PROFILE_ADDRESS2],
                 jrecord->fields[FIELD_PROFILE_ADDRESS3],
                 jrecord->fields[FIELD_PROFILE_STATE],
                 jrecord->fields[FIELD_PROFILE_PINCODE],
                 jrecord->fields[FIELD_PROFILE_COUNTRY],
                 jrecord->fields[FIELD_PROFILE_UID],
                 "permanent");
    } else {
        PUSH_BUF(qrybuf, len, "INSERT INTO UserAddress "
                 "(UID, AddressType, Address1, Address2, Address3, State, "
                 "Pincode, Country) VALUES ('%s', '%s', '%s', '%s', '%s', "
                 "'%s', '%s', '%s')",
                 jrecord->fields[FIELD_PROFILE_UID],
                 "permanent",
                 jrecord->fields[FIELD_PROFILE_ADDRESS1],
                 jrecord->fields[FIELD_PROFILE_ADDRESS2],
                 jrecord->fields[FIELD_PROFILE_ADDRESS3],
                 jrecord->fields[FIELD_PROFILE_STATE],
                 jrecord->fields[FIELD_PROFILE_PINCODE],
                 jrecord->fields[FIELD_PROFILE_COUNTRY]);
    }
    qrybuf[len++] = '\0';

    if (jrecord->fields[FIELD_PROFILE_FACEBOOK_HANDLE]) {
        db_qry2 = &qrybuf[len];
        tmp_len = gweb_mysql_update_social_network(db_qry2, jrecord, "facebook",
                             FIELD_PROFILE_FACEBOOK_HANDLE, sn_facebook_found);
        len += tmp_len + 1;
    }

    if (jrecord->fields[FIELD_PROFILE_TWITTER_HANDLE]) {
        db_qry3 = &qrybuf[len];
        tmp_len = gweb_mysql_update_social_network(db_qry3, jrecord, "twitter",
                             FIELD_PROFILE_TWITTER_HANDLE, sn_twitter_found);
        len += tmp_len + 1;
    }

    /* Start transaction and push the updates */
    gweb_mysql_start_transaction();

    if (db_qry1 && mysql_query(g_mysql_ctx, db_qry1)) {
        goto __abort_transaction;
    }

    if (db_qry2 && mysql_query(g_mysql_ctx, db_qry2)) {
        goto __abort_transaction;
    }

    if (db_qry3 && mysql_query(g_mysql_ctx, db_qry3)) {
        goto __abort_transaction;
    }

    gweb_mysql_commit_transaction();

    gweb_mysql_prepare_response(JSON_C_PROFILE_RESP,
                                GWEB_MYSQL_OK,
                                j2cresp);
    return 0;

__abort_transaction:
    report_mysql_error_noclose(g_mysql_ctx);
    gweb_mysql_abort_transaction();

__bail_out:
    gweb_mysql_prepare_response(JSON_C_PROFILE_RESP,
                                bail_out_err,
                                j2cresp);
    return 0;
}

int
gweb_mysql_free_profile (j2c_resp_t *j2cresp)
{
    if (j2cresp) {
        free(j2cresp);
    }
    return 0;
}

int
gweb_mysql_shutdown (void)
{
    log_debug("Closing MySQL connection!\n");

    if (g_mysql_ctx)
	mysql_close(g_mysql_ctx);

    return 0;
}

static int
gweb_mysql_connect (MYSQL *ctx)
{
    if (!ctx) {
        return -1;
    }

    if (mysql_real_connect(ctx,
                           MYSQL_DB_HOST,
                           MYSQL_DB_USER,
                           MYSQL_DB_PASSWORD,
                           MYSQL_DB_NAMESPACE,
                           0,
                           NULL,
                           CLIENT_MULTI_STATEMENTS) == NULL) {
        return -1;
    }

    return 0;
}

/*
 * Check / reconnect MySQL connection
 */
int
gweb_mysql_ping (void)
{
    unsigned long thid_before_ping, thid_after_ping;

    thid_before_ping = mysql_thread_id(g_mysql_ctx);
    mysql_ping(g_mysql_ctx);
    thid_after_ping = mysql_thread_id(g_mysql_ctx);

    if (thid_before_ping != thid_after_ping) {
        log_debug("%s: MySQL reconnected!\n", __func__);
    }

    return 0;
}

int
gweb_mysql_init (void)
{
    my_bool reconnect = 1;

    log_debug("Initializing schema, MySQL version = %s\n",
	      mysql_get_client_info());

    if ((g_mysql_ctx = mysql_init(NULL)) == NULL) {
	report_mysql_error(g_mysql_ctx);
    }

    mysql_options(g_mysql_ctx, MYSQL_OPT_RECONNECT, &reconnect);

    if (gweb_mysql_connect(g_mysql_ctx)) {
        report_mysql_error(g_mysql_ctx);
    }

    log_debug("MySQL connected!\n");

    return 0;
}
