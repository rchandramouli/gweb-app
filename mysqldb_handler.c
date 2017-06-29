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
#include <gweb/mysqldb_log.h>
#include <gweb/config.h>
#include <gweb/uid.h>

/* Misc macros */
#define MAX_DATETIME_STRSZ   (20)

/* MySQL transaction status */
enum {
    GWEB_MYSQL_ERR_NO_RECORD = 1,
    GWEB_MYSQL_ERR_NO_MEMORY,
    GWEB_MYSQL_ERR_UNKNOWN,
    GWEB_MYSQL_ERR_DUPLICATE,
    GWEB_MYSQL_OK,
};

static struct mysql_config *g_mysql_cfg;
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
        J2C_RESP_TABLE(type, *table);                                   \
                                                                        \
        table = &resp->type;                                            \
        gweb_mysql_get_common_response(code,                            \
                       &table->fields[FIELD_##macro##_RESP_CODE],       \
                       &table->fields[FIELD_##macro##_RESP_DESC]);      \
    }

static void
gweb_mysql_update_response (int resp_type, int mysql_code,
                            j2c_resp_t **response)
{
    j2c_resp_t *resp;

    if (response == NULL) {
        return;
    }
    resp = *response;

    switch (resp_type) {
    case JSON_C_REGISTRATION_RESP:
        generate_default_response(registration, REGISTRATION, mysql_code);
        break;

    case JSON_C_PROFILE_RESP:
        generate_default_response(profile, PROFILE, mysql_code);
        break;

    case JSON_C_PROFILE_INFO_RESP:
      generate_default_response(profile_info, PROFILE_INFO, mysql_code);
        break;

    case JSON_C_AVATAR_RESP:
        generate_default_response(avatar, AVATAR, mysql_code);
        break;

    case JSON_C_CXN_REQUEST_RESP:
        generate_default_response(cxn_request, CXN_REQUEST, mysql_code);
        break;

    case JSON_C_CXN_REQUEST_QUERY_RESP:
        generate_default_response(cxn_request_query, CXN_REQUEST_QUERY, mysql_code);
        break;

    case JSON_C_CXN_CHANNEL_RESP:
        generate_default_response(cxn_channel, CXN_CHANNEL, mysql_code);
        break;

    case JSON_C_CXN_CHANNEL_QUERY_RESP:
        generate_default_response(cxn_channel_query, CXN_CHANNEL_QUERY, mysql_code);
        break;

    case JSON_C_UID_QUERY_RESP:
        generate_default_response(uid_query, UID_QUERY, mysql_code);
        break;

    case JSON_C_AVATAR_QUERY_RESP:
        generate_default_response(avatar_query, AVATAR_QUERY, mysql_code);
        break;

    case JSON_C_CXN_PREFERENCE_RESP:
        generate_default_response(cxn_preference, CXN_PREFERENCE, mysql_code);
        break;

    case JSON_C_CXN_PREFERENCE_QUERY_RESP:
        generate_default_response(cxn_preference_query, CXN_PREFERENCE_QUERY, mysql_code);
        break;
        
    default:
        return;
    }
    return;
}

static void
gweb_mysql_prepare_response (int resp_type, int mysql_code,
                             j2c_resp_t **response)
{
    if (response == NULL) {
        return;
    }

    /*
     * Allocate response only if NULL. This helps to reuse single
     * response allocation multiple times
     */
    if (*response == NULL) {
        *response = gweb_mysql_allocate_response();
    }

    gweb_mysql_update_response(resp_type, mysql_code, response);

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
        return -1;
    }

    if ((result = mysql_store_result(g_mysql_ctx)) == NULL) {
        report_mysql_error_noclose(g_mysql_ctx);
        return -1;
    }

    ret = mysql_num_rows(result);

    mysql_free_result(result);

    return ret;
}

int
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
        return MYSQL_STATUS_FAIL;
    } else if (ret > 0) {
        /* Duplicate record */
        gweb_mysql_prepare_response(JSON_C_REGISTRATION_RESP,
                                    GWEB_MYSQL_ERR_DUPLICATE,
                                    j2cresp);
        return MYSQL_STATUS_FAIL;
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

    return MYSQL_STATUS_OK;

__abort_transaction:
    report_mysql_error_noclose(g_mysql_ctx);
    gweb_mysql_abort_transaction();
    gweb_mysql_prepare_response(JSON_C_REGISTRATION_RESP,
                                GWEB_MYSQL_ERR_UNKNOWN,
                                j2cresp);
    return MYSQL_STATUS_FAIL;
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
    return MYSQL_STATUS_OK;
}

static const char *profile_info_qry_fields[] = {
    [FIELD_PROFILE_INFO_RESP_CODE] = NULL,
    [FIELD_PROFILE_INFO_RESP_DESC] = NULL,
    [FIELD_PROFILE_INFO_RESP_UID] = "UserRegInfo.UID",
    [FIELD_PROFILE_INFO_RESP_FNAME] = "UserRegInfo.FirstName",
    [FIELD_PROFILE_INFO_RESP_LNAME] = "UserRegInfo.LastName",
    [FIELD_PROFILE_INFO_RESP_EMAIL] = "UserRegInfo.Email",
    [FIELD_PROFILE_INFO_RESP_PHONE] = "UserPhone.Phone",
    [FIELD_PROFILE_INFO_RESP_ADDRESS1] = "UserAddress.Address1",
    [FIELD_PROFILE_INFO_RESP_ADDRESS2] = "UserAddress.Address2",
    [FIELD_PROFILE_INFO_RESP_ADDRESS3] = "UserAddress.Address3",
    [FIELD_PROFILE_INFO_RESP_COUNTRY] = "UserAddress.Country",
    [FIELD_PROFILE_INFO_RESP_STATE] = "UserAddress.State",
    [FIELD_PROFILE_INFO_RESP_PINCODE] = "UserAddress.Pincode",
    /* NOTE: Multi-record, using below hack */
    [FIELD_PROFILE_INFO_RESP_FACEBOOK_HANDLE] = "UserSocialNetwork.NetworkType",
    [FIELD_PROFILE_INFO_RESP_TWITTER_HANDLE] = "UserSocialNetwork.NetworkHandle",
    [FIELD_PROFILE_INFO_RESP_AVATAR_URL] = "UserRegInfo.AvatarURL",
    [FIELD_PROFILE_INFO_RESP_FLAG] = "UserRegInfo.ProfileFlags",
};

/*
 * Populate complete profile information based on UID or e-mail
 */
static int
gweb_mysql_populate_profile_info (j2c_resp_t *j2cresp, const char *uid,
                                  const char *email)
{
    uint8_t qrybuf[MAX_MYSQL_QRYSZ];
    int len = 0, fld;

    MYSQL_RES *result;
    MYSQL_ROW row;

    J2C_RESP_TABLE(profile_info, *resp) = &j2cresp->profile_info;

    if (uid == NULL && email == NULL) {
        return GWEB_MYSQL_ERR_NO_RECORD;
    }

    PUSH_BUF(qrybuf, len, "SELECT ");

    for (fld = 0; fld < FIELD_PROFILE_INFO_RESP_MAX; fld++) {
        if (profile_info_qry_fields[fld] == NULL) {
            PUSH_BUF(qrybuf, len, "NULL,");
        } else {
            PUSH_BUF(qrybuf, len, "%s,", profile_info_qry_fields[fld]);
        }
    }

    len--; /* Strip leading ',' */

    PUSH_BUF(qrybuf, len, " FROM UserRegInfo "
	     "LEFT JOIN UserPhone USING(UID) "
	     "LEFT JOIN UserSocialNetwork USING(UID) "
	     "LEFT JOIN UserAddress USING(UID) "
	     "WHERE ");

    if (uid != NULL) {
        PUSH_BUF(qrybuf, len, "UserRegInfo.UID='%s'", uid);
    } else if (email != NULL) {
        PUSH_BUF(qrybuf, len, "UserRegInfo.Email='%s'", email);
    }

    qrybuf[len] = '\0';

    gweb_mysql_ping();

    if (mysql_query(g_mysql_ctx, qrybuf)) {
        report_mysql_error_noclose(g_mysql_ctx);
        return GWEB_MYSQL_ERR_UNKNOWN;
    }

    if ((result = mysql_store_result(g_mysql_ctx)) == NULL) {
        report_mysql_error_noclose(g_mysql_ctx);
        return GWEB_MYSQL_ERR_UNKNOWN;
    }

    if (mysql_num_rows(result) == 0) {
        mysql_free_result(result);
        return GWEB_MYSQL_ERR_NO_RECORD;
    }

    if ((row = mysql_fetch_row(result)) == NULL) {
        mysql_free_result(result);
        return GWEB_MYSQL_ERR_UNKNOWN;
    }

    for (fld = FIELD_PROFILE_INFO_RESP_UID;
         fld < FIELD_PROFILE_INFO_RESP_MAX;
         fld++) {
        if (fld == FIELD_PROFILE_INFO_RESP_FACEBOOK_HANDLE ||
            fld == FIELD_PROFILE_INFO_RESP_TWITTER_HANDLE) {
            continue;
        }
        if (row[fld]) {
            resp->fields[fld] = strndup(row[fld], strlen(row[fld]));
        }
    }

    do {
        char *network_type = row[FIELD_PROFILE_INFO_RESP_FACEBOOK_HANDLE];
        int handle_idx = FIELD_PROFILE_INFO_RESP_TWITTER_HANDLE;

        if (network_type) {
            if (strcasecmp(network_type, "facebook") == 0) {
                resp->fields[FIELD_PROFILE_INFO_RESP_FACEBOOK_HANDLE] =
                    strndup(row[handle_idx], strlen(row[handle_idx]));

            } else if (strcasecmp(network_type, "twitter") == 0) {
                resp->fields[FIELD_PROFILE_INFO_RESP_TWITTER_HANDLE] =
                    strndup(row[handle_idx], strlen(row[handle_idx]));
            }
        }
    } while ((row = mysql_fetch_row(result)));

    mysql_free_result(result);

    return GWEB_MYSQL_OK;
}

static int
gweb_mysql_free_profile_info (j2c_resp_t *j2cresp)
{
    int fld;
    J2C_RESP_TABLE(profile_info, *resp) = &j2cresp->profile_info;

    for (fld = FIELD_PROFILE_INFO_RESP_UID;
         fld < FIELD_PROFILE_INFO_RESP_MAX; fld++) {
        if (resp->fields[fld])
            free(resp->fields[fld]);
    }
    return GWEB_MYSQL_OK;
}

int
gweb_mysql_handle_login (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    uint8_t qrybuf[MAX_MYSQL_QRYSZ];
    int len = 0, err = GWEB_MYSQL_ERR_UNKNOWN, ret = MYSQL_STATUS_FAIL;
    int count;

    J2C_MSG_TABLE(login, *jrecord) = &j2cmsg->login;

    /* Allocate for response */
    gweb_mysql_prepare_response(JSON_C_PROFILE_INFO_RESP,
                                GWEB_MYSQL_OK,
                                j2cresp);

    if (!jrecord->fields[FIELD_LOGIN_EMAIL] ||
        !jrecord->fields[FIELD_LOGIN_PASSWORD]) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }
    
    PUSH_BUF(qrybuf, len, "SELECT UID from UserRegInfo WHERE "
             "UserRegInfo.Email='%s' AND UserRegInfo.Password='%s'",
             jrecord->fields[FIELD_LOGIN_EMAIL],
             jrecord->fields[FIELD_LOGIN_PASSWORD]);
    qrybuf[len] = '\0';

    if ((count = gweb_mysql_get_query_count(qrybuf)) < 0) {
        goto __bail_out;
    }

    if (count == 0) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    err = gweb_mysql_populate_profile_info(*j2cresp, NULL,
                              jrecord->fields[FIELD_LOGIN_EMAIL]);
    if (err != GWEB_MYSQL_OK) {
        goto __bail_out;
    }

    ret = MYSQL_STATUS_OK;

__bail_out:
    gweb_mysql_update_response(JSON_C_PROFILE_INFO_RESP, err, j2cresp);

    return ret;
}

int
gweb_mysql_free_login (j2c_resp_t *j2cresp)
{
    if (j2cresp) {
        gweb_mysql_free_profile_info(j2cresp);
        free(j2cresp);
    }
    return MYSQL_STATUS_OK;
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
    return MYSQL_STATUS_OK;

__abort_transaction:
    gweb_mysql_abort_transaction();
__bail_out:
    gweb_mysql_prepare_response(JSON_C_AVATAR_RESP,
                                GWEB_MYSQL_ERR_NO_RECORD,
                                j2cresp);
    return MYSQL_STATUS_FAIL;
}

int
gweb_mysql_free_avatar (j2c_resp_t *j2cresp)
{
    if (j2cresp) {
        free(j2cresp);
    }
    return MYSQL_STATUS_OK;
}

static int
gweb_mysql_query_social_network (const char *uid, const char *sn_type)
{
    uint8_t qrybuf[MAX_MYSQL_QRYSZ];
    int len = 0;

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

/* If the given string is "" then set the field to NULL, else update
 * the field
 */
#define STR_OR_NULL_QRY(fld, fldname)				\
    if (jrecord->fields[fld]) {					\
	if (strlen(jrecord->fields[fld]) == 0) {		\
	    PUSH_BUF(qrybuf, len, fldname "NULL,");		\
	} else {						\
	    PUSH_BUF(qrybuf, len, fldname "'%s',",		\
		     jrecord->fields[fld]);			\
	}							\
    }

#define STR_OR_NULL_INSERT_QRY(fld, fldname)	\
    STR_OR_NULL_QRY(fld, fldname)		\
    else {					\
	PUSH_BUF(qrybuf, len, "NULL,");		\
    }

int
gweb_mysql_handle_profile (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    struct j2c_profile_msg *jrecord = &j2cmsg->profile;

    uint8_t qrybuf[MAX_MYSQL_QRYSZ], *db_qry1, *db_qry2, *db_qry3;
    int record_found = 0, sn_facebook_found = 0, sn_twitter_found = 0;
    int update_fields = 0;
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
    if ((ret = gweb_mysql_get_query_count(qrybuf)) < 0) {
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

    db_qry1 = db_qry2 = db_qry3 = NULL;

    /* Update record if exists or else, insert. Note that, we have a
     * case where there is no primary key in this table. Check for
     * other social network handles.
     */
    len = 0;

    /* Check if there is atleast one field to update */
    if (jrecord->fields[FIELD_PROFILE_ADDRESS1] ||
	jrecord->fields[FIELD_PROFILE_ADDRESS2] ||
	jrecord->fields[FIELD_PROFILE_ADDRESS3] ||
	jrecord->fields[FIELD_PROFILE_STATE] ||
	jrecord->fields[FIELD_PROFILE_PINCODE] ||
	jrecord->fields[FIELD_PROFILE_COUNTRY]) {
	update_fields = 1;
    }

    if (record_found && update_fields) {
	PUSH_BUF(qrybuf, len, "UPDATE UserAddress SET ");
	STR_OR_NULL_QRY(FIELD_PROFILE_ADDRESS1, "Address1=");
	STR_OR_NULL_QRY(FIELD_PROFILE_ADDRESS2, "Address2=");
	STR_OR_NULL_QRY(FIELD_PROFILE_ADDRESS3, "Address3=");
	STR_OR_NULL_QRY(FIELD_PROFILE_STATE, "State=");
	STR_OR_NULL_QRY(FIELD_PROFILE_PINCODE, "Pincode=");
	STR_OR_NULL_QRY(FIELD_PROFILE_COUNTRY, "Country=");
	len--; /* Strip leading ',' */
	PUSH_BUF(qrybuf, len, " WHERE UID='%s' AND AddressType='%s'",
		 jrecord->fields[FIELD_PROFILE_UID], "permanent");

	db_qry1 = &qrybuf[0];
	qrybuf[len++] = '\0';

    } else if (!record_found) {
        PUSH_BUF(qrybuf, len, "INSERT INTO UserAddress "
                 "(UID, AddressType, Address1, Address2, Address3, State, "
                 "Pincode, Country) VALUES ('%s', '%s', ",
		 jrecord->fields[FIELD_PROFILE_UID],
		 "permanent");
	STR_OR_NULL_INSERT_QRY(FIELD_PROFILE_ADDRESS1, "");
	STR_OR_NULL_INSERT_QRY(FIELD_PROFILE_ADDRESS2, "");
	STR_OR_NULL_INSERT_QRY(FIELD_PROFILE_ADDRESS3, "");
	STR_OR_NULL_INSERT_QRY(FIELD_PROFILE_STATE, "");
	STR_OR_NULL_INSERT_QRY(FIELD_PROFILE_PINCODE, "");
	STR_OR_NULL_INSERT_QRY(FIELD_PROFILE_COUNTRY, "");
	len--; /* Strip leading ',' */

	PUSH_BUF(qrybuf, len, ")");

	db_qry1 = &qrybuf[0];
	qrybuf[len++] = '\0';
    }

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
    return MYSQL_STATUS_OK;

__abort_transaction:
    report_mysql_error_noclose(g_mysql_ctx);
    gweb_mysql_abort_transaction();

__bail_out:
    gweb_mysql_prepare_response(JSON_C_PROFILE_RESP,
                                bail_out_err,
                                j2cresp);
    return MYSQL_STATUS_FAIL;
}

/* Connection Request / Channel Request handling */

#define CXN_REQUEST_FLAG_CLOSED      "closed"
#define CXN_CHANNEL_BASIC_CONNECT    "connect"

static int
gweb_mysql_prepare_cxn_accept_query (const char *from_uid, const char *to_uid,
                                     uint8_t *buf, int *qlen)
{
    uint8_t qrybuf[MAX_MYSQL_QRYSZ], utc_dt_str[MAX_DATETIME_STRSZ];
    int len = 0, qcount = -1, row_count, idx;

    MYSQL_RES *result = NULL;
    MYSQL_ROW row;

    PUSH_BUF(qrybuf, len, "SELECT ChannelId FROM UserConnectPreferences WHERE "
             "UID='%s' AND ChannelFlags='public'", to_uid);
    qrybuf[len++] = '\0';

    if (mysql_query(g_mysql_ctx, qrybuf)) {
        report_mysql_error_noclose(g_mysql_ctx);
        goto __bail_out;
    }

    if ((result = mysql_store_result(g_mysql_ctx)) == NULL) {
        report_mysql_error_noclose(g_mysql_ctx);
        goto __bail_out;
    }

    gweb_get_utc_datetime(utc_dt_str);

    len = 0;

    PUSH_BUF(buf, len, "DELETE FROM UserConnectChannel WHERE "
             "FromUID='%s' AND ToUID='%s'", from_uid, to_uid);
    buf[len++] = '\0';
    qcount = 1;

    /* Insert ChannelId as 'connect' if no public preferences exists */
    if ((row_count = mysql_num_rows(result)) == 0) {
        PUSH_BUF(buf, len, "INSERT INTO UserConnectChannel (FromUID, ToUID, "
                 "ConnectedOn, ChannelId) VALUES ('%s', '%s', '%s', '%s')",
                 from_uid, to_uid, utc_dt_str,
                 CXN_CHANNEL_BASIC_CONNECT);
        buf[len++] = '\0';
        *qlen = len;
        qcount++;
        goto __bail_out;
    }

    PUSH_BUF(buf, len, "INSERT INTO UserConnectChannel (FromUID, ToUID, "
             "ConnectedOn, ChannelId) VALUES ");

    for (idx = 0; idx < row_count; idx++) {
        if ((row = mysql_fetch_row(result)) == NULL) {
            qcount = -1;
            goto __bail_out;
        }
        PUSH_BUF(buf, len, "('%s', '%s', '%s', '%s'),",
                 from_uid, to_uid, utc_dt_str, row[0]);
    }
    buf[--len] = '\0';
    *qlen = len;
    qcount++;

__bail_out:
    if (result) {
        mysql_free_result(result);
    }
    return qcount;
}

int
gweb_mysql_handle_cxn_request (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    uint8_t qrybuf[MAX_MYSQL_QRYSZ], *q_ptr;
    uint8_t utc_dt_str[MAX_DATETIME_STRSZ];
    int err = GWEB_MYSQL_ERR_UNKNOWN, ret = MYSQL_STATUS_FAIL;
    int len = 0, count, is_update = 0, qry_idx, qcount = 0;

    J2C_MSG_TABLE(cxn_request, *jrecord) = &j2cmsg->cxn_request;

    gweb_mysql_prepare_response(JSON_C_CXN_REQUEST_RESP, GWEB_MYSQL_OK, j2cresp);

    /* Sanity tests */
    if (!jrecord->fields[FIELD_CXN_REQUEST_UID] ||
        !jrecord->fields[FIELD_CXN_REQUEST_TO_UID]) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    /* Check if UIDs are valid */
    PUSH_BUF(qrybuf, len, "SELECT UID FROM UserRegInfo WHERE UID='%s' or UID='%s'",
             jrecord->fields[FIELD_CXN_REQUEST_UID],
             jrecord->fields[FIELD_CXN_REQUEST_TO_UID]);
    qrybuf[len++] = '\0';

    gweb_mysql_ping();

    if ((count = gweb_mysql_get_query_count(qrybuf)) < 0) {
        goto __bail_out;
    }

    /* NOTE: we do not update if multiple UIDs (>2) exists (which
     * should not have been in the first place)
     */
    if (count != 2) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    /* Check if the record exists, if so, update the flags alone */
    len = 0;
    PUSH_BUF(qrybuf, len, "SELECT FromUID, ToUID FROM UserConnectRequest WHERE "
             "FromUID='%s' AND ToUID='%s'",
             jrecord->fields[FIELD_CXN_REQUEST_UID],
             jrecord->fields[FIELD_CXN_REQUEST_TO_UID]);
    qrybuf[len++] = '\0';

    if ((is_update = gweb_mysql_get_query_count(qrybuf)) < 0) {
        goto __bail_out;
    }

    len = 0;
    if (is_update) {
        PUSH_BUF(qrybuf, len, "UPDATE UserConnectRequest SET Flags='%s' WHERE "
                 "FromUID='%s' AND ToUID='%s'",
                 jrecord->fields[FIELD_CXN_REQUEST_FLAG],
                 jrecord->fields[FIELD_CXN_REQUEST_UID],
                 jrecord->fields[FIELD_CXN_REQUEST_TO_UID]);
        qrybuf[len++] = '\0';
    } else {
        gweb_get_utc_datetime(utc_dt_str);
        PUSH_BUF(qrybuf, len, "INSERT INTO UserConnectRequest (FromUID, ToUID, "
                 "SentOn, Flags) VALUES ('%s', '%s', '%s', '%s')",
                 jrecord->fields[FIELD_CXN_REQUEST_UID],
                 jrecord->fields[FIELD_CXN_REQUEST_TO_UID],
                 utc_dt_str,
                 jrecord->fields[FIELD_CXN_REQUEST_FLAG]);
        qrybuf[len++] = '\0';
    }

    /* If the request flag is 'accept' connect the UIDs in the channel
     * table based on the preferences that are exposed as public
     */
    if (!strcmp(jrecord->fields[FIELD_CXN_REQUEST_FLAG], CXN_REQUEST_FLAG_CLOSED)) {
        int q_len = 0;

        qcount = gweb_mysql_prepare_cxn_accept_query(jrecord->fields[FIELD_CXN_REQUEST_UID],
                                                     jrecord->fields[FIELD_CXN_REQUEST_TO_UID],
                                                     &qrybuf[len], &q_len);
        if (qcount == -1) {
            goto __bail_out;
        }
        len += q_len;
    }

    gweb_mysql_start_transaction();

    q_ptr = qrybuf;
    for (len = qry_idx = 0; qry_idx < qcount + 1; qry_idx++) {
        log_debug("**# [Q%d] EXEC MYSQL [%s]\n", qry_idx, q_ptr+len);
        if (mysql_query(g_mysql_ctx, q_ptr + len)) {
            report_mysql_error_noclose(g_mysql_ctx);
            gweb_mysql_abort_transaction();
            goto __bail_out;
        }
        len += strlen(q_ptr + len) + 1;
    }

    gweb_mysql_commit_transaction();

    err = GWEB_MYSQL_OK;
    ret = MYSQL_STATUS_OK;

__bail_out:
    gweb_mysql_update_response(JSON_C_CXN_REQUEST_RESP, err, j2cresp);
    return ret;
}

int
gweb_mysql_handle_cxn_channel (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    uint8_t qrybuf[MAX_MYSQL_QRYSZ];
    uint8_t utc_dt_str[MAX_DATETIME_STRSZ];
    int err = GWEB_MYSQL_ERR_UNKNOWN, ret = MYSQL_STATUS_FAIL;
    int len = 0, count, is_update = 0;

    J2C_MSG_TABLE(cxn_channel, *jrecord) = &j2cmsg->cxn_channel;

    gweb_mysql_prepare_response(JSON_C_CXN_CHANNEL_RESP, GWEB_MYSQL_OK, j2cresp);

    /* Sanity tests */
    if (!jrecord->fields[FIELD_CXN_CHANNEL_UID] ||
        !jrecord->fields[FIELD_CXN_CHANNEL_TO_UID] ||
        !jrecord->fields[FIELD_CXN_CHANNEL_TYPE]) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    /* Check if UIDs are valid */
    PUSH_BUF(qrybuf, len, "SELECT UID FROM UserRegInfo WHERE UID='%s' or UID='%s'",
             jrecord->fields[FIELD_CXN_CHANNEL_UID],
             jrecord->fields[FIELD_CXN_CHANNEL_TO_UID]);
    qrybuf[len++] = '\0';

    gweb_mysql_ping();

    if ((count = gweb_mysql_get_query_count(qrybuf)) < 0) {
        goto __bail_out;
    }

    /* NOTE: we do not update if multiple UIDs (>2) exists (which
     * should not have been in the first place)
     */
    if (count != 2) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    /* Check if the record exists, if so, update the flags alone */
    len = 0;
    PUSH_BUF(qrybuf, len, "SELECT FromUID, ToUID FROM UserConnectChannel WHERE "
             "FromUID='%s' AND ToUID='%s' AND ChannelId='%s'",
             jrecord->fields[FIELD_CXN_CHANNEL_UID],
             jrecord->fields[FIELD_CXN_CHANNEL_TO_UID],
             jrecord->fields[FIELD_CXN_CHANNEL_TYPE]);
    qrybuf[len++] = '\0';

    if ((is_update = gweb_mysql_get_query_count(qrybuf)) < 0) {
        goto __bail_out;
    }

    len = 0;
    if (!is_update) {
        gweb_get_utc_datetime(utc_dt_str);
        PUSH_BUF(qrybuf, len, "INSERT INTO UserConnectChannel (FromUID, ToUID, "
                 "ConnectedOn, ChannelId) VALUES ('%s', '%s', '%s', '%s')",
                 jrecord->fields[FIELD_CXN_CHANNEL_UID],
                 jrecord->fields[FIELD_CXN_CHANNEL_TO_UID],
                 utc_dt_str,
                 jrecord->fields[FIELD_CXN_CHANNEL_TYPE]);
        qrybuf[len++] = '\0';

        gweb_mysql_start_transaction();

        if (mysql_query(g_mysql_ctx, qrybuf)) {
            report_mysql_error_noclose(g_mysql_ctx);
            gweb_mysql_abort_transaction();
            goto __bail_out;
        }

        gweb_mysql_commit_transaction();
    }

    err = GWEB_MYSQL_OK;
    ret = MYSQL_STATUS_OK;

__bail_out:
    gweb_mysql_update_response(JSON_C_CXN_CHANNEL_RESP, err, j2cresp);
    return ret;
}

/*
 * Given UID, fetch name and avatar for minimal profile display. Note
 * that the e-mail/phone or other details have not been fetched.
 */
static int
gweb_mysql_get_name_avatar_from_uid (const char *uid, char **fname,
                                     char **lname, char **avatar)
{
    MYSQL_RES *result;
    MYSQL_ROW row;

    uint8_t qrybuf[MAX_MYSQL_QRYSZ];
    int len = 0;

    /* TBD: LOG error */
    if (!uid || !fname || !lname || !avatar)
        return MYSQL_STATUS_FAIL;

    PUSH_BUF(qrybuf, len, "SELECT FirstName, LastName, AvatarURL FROM UserRegInfo "
             "WHERE UID='%s'", uid);
    qrybuf[len] = '\0';

    *fname = *lname = *avatar = NULL;

    if (mysql_query(g_mysql_ctx, qrybuf)) {
        report_mysql_error_noclose(g_mysql_ctx);
        return MYSQL_STATUS_FAIL;
    }

    if ((result = mysql_store_result(g_mysql_ctx)) == NULL) {
        report_mysql_error_noclose(g_mysql_ctx);
        return MYSQL_STATUS_FAIL;
    }

    /* *TBD* There should be only one row if there is a match. */
    if (mysql_num_rows(result) != 1) {
        mysql_free_result(result);
        return MYSQL_STATUS_FAIL;
    }

    if ((row = mysql_fetch_row(result)) == NULL) {
        report_mysql_error_noclose(g_mysql_ctx);
        mysql_free_result(result);
        return MYSQL_STATUS_FAIL;
    }

    if (row[0])
        *fname = strndup(row[0], strlen(row[0])); /* FirstName */
    if (row[1])
        *lname = strndup(row[1], strlen(row[1])); /* LastName */
    if (row[2])
        *avatar = strndup(row[2], strlen(row[2])); /* AvatarURL */

    mysql_free_result(result);

    return MYSQL_STATUS_OK;
}

#define CXN_OUTBOUND   (1)
#define CXN_INBOUND    (2)

#define MYSQL_MAX_CXN_REQUEST_ROWS_PER_QUERY      (20)
#define MYSQL_MAX_CXN_CHANNEL_ROWS_PER_QUERY      (20)
#define MYSQL_MAX_CXN_PREFERENCE_ROWS_PER_QUERY   (20)

#define CXN_REQ_IDX(x)   \
   ((FIELD_CXN_REQUEST_QUERY_RESP_##x) - FIELD_CXN_REQUEST_QUERY_RESP_ARRAY_START - 1)

#define CXN_CHNL_IDX(x)                                                 \
    ((FIELD_CXN_CHANNEL_QUERY_RESP_##x) - FIELD_CXN_CHANNEL_QUERY_RESP_ARRAY_START - 1)

#define CXN_PREF_IDX(x)                         \
    ((FIELD_CXN_PREFERENCE_QUERY_RESP_##x) - FIELD_CXN_PREFERENCE_QUERY_RESP_ARRAY_START - 1)

int
gweb_mysql_handle_cxn_request_query (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    int match_count, direction = 0;
    int max_rows, idx, rowid = 0, len = 0;
    int err = GWEB_MYSQL_ERR_UNKNOWN, ret = MYSQL_STATUS_FAIL;
    uint8_t qrybuf[MAX_MYSQL_QRYSZ], buf[32];

    char *fname = NULL, *lname = NULL, *avatar = NULL;
    const char *uid = NULL;

    MYSQL_RES *result = NULL;
    MYSQL_ROW row;

    J2C_MSG_TABLE(cxn_request_query, *jrecord) = &j2cmsg->cxn_request_query;
    J2C_RESP_TABLE(cxn_request_query, *resp) = NULL;
    struct j2c_cxn_request_query_resp_array1 *arr = NULL;

    /* Allocate for response */
    gweb_mysql_prepare_response(JSON_C_CXN_REQUEST_QUERY_RESP,
                                GWEB_MYSQL_OK,
                                j2cresp);
    resp = &((*j2cresp)->cxn_request_query);
    resp->nr_array1_records = -1; /* No records */

    PUSH_BUF(qrybuf, len, "SELECT FromUID, ToUID, SentOn, Flags FROM UserConnectRequest "
             "WHERE TRUE ");

    if (jrecord->fields[FIELD_CXN_REQUEST_QUERY_FROM_UID]) {
        uid = jrecord->fields[FIELD_CXN_REQUEST_QUERY_FROM_UID];
        direction = CXN_OUTBOUND;
        PUSH_BUF(qrybuf, len, "AND FromUID='%s' ", uid);

    } else if (jrecord->fields[FIELD_CXN_REQUEST_QUERY_TO_UID]) {
        uid = jrecord->fields[FIELD_CXN_REQUEST_QUERY_TO_UID];
        direction = CXN_INBOUND;
        PUSH_BUF(qrybuf, len, "AND ToUID='%s' ", uid);
    }

    if (!direction || !gweb_mysql_check_uid_email(uid, NULL)) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    if (jrecord->fields[FIELD_CXN_REQUEST_QUERY_FLAG]) {
        PUSH_BUF(qrybuf, len, "AND Flags='%s' ",
                 jrecord->fields[FIELD_CXN_REQUEST_QUERY_FLAG]);
    }

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

    match_count = mysql_num_rows(result);
    if (match_count == 0) {
        goto __send_record;
    }

    /* *TBD* For easier access, dump all requested data to a file in
     * JSON format and upload.
     */
    max_rows = (match_count >= MYSQL_MAX_CXN_REQUEST_ROWS_PER_QUERY) ?
        MYSQL_MAX_CXN_REQUEST_ROWS_PER_QUERY: match_count;

    resp->array1 = calloc(max_rows, sizeof(struct j2c_cxn_request_query_resp_array1));
    if (!resp->array1) {
        err = GWEB_MYSQL_ERR_NO_MEMORY;
        goto __bail_out;
    }

    for (rowid = idx = 0; idx < match_count && rowid < max_rows; idx++) {
        if ((row = mysql_fetch_row(result)) == NULL) {
            goto __bail_out;
        }

        if (!row[0] || !row[1]) { /* FromUID || ToUID */
            /* Skip if there is no FromUID or ToUID */
            continue;
        }

        /* NOTE: Number of rows in response could be lesser than max_rows */
        arr = &resp->array1[rowid++];

        if (direction == CXN_INBOUND) {
            arr->fields[CXN_REQ_IDX(UID)] = strndup(row[0], strlen(row[0]));
        } else {
            arr->fields[CXN_REQ_IDX(UID)] = strndup(row[1], strlen(row[1]));
        }
        if (row[2]) { /* SentOn */
            arr->fields[CXN_REQ_IDX(DATE)] = strndup(row[2], strlen(row[2]));
        }
        if (row[3]) { /* Flags */
            arr->fields[CXN_REQ_IDX(FLAG)] = strndup(row[3], strlen(row[3]));
        }

        if (gweb_mysql_get_name_avatar_from_uid(arr->fields[CXN_REQ_IDX(UID)],
                           &fname, &lname, &avatar) != MYSQL_STATUS_OK) {
            free(arr->fields[CXN_REQ_IDX(UID)]);
            free(arr->fields[CXN_REQ_IDX(DATE)]);
            rowid--;
            continue;
        }

        if (fname) {
            arr->fields[CXN_REQ_IDX(FNAME)] = fname;
        }
        if (lname) {
            arr->fields[CXN_REQ_IDX(LNAME)] = lname;
        }
        if (avatar) {
            arr->fields[CXN_REQ_IDX(AVATAR_URL)] = avatar;
        }
    }

__send_record:
    sprintf(buf, "%d", match_count);
    resp->fields[FIELD_CXN_REQUEST_QUERY_RESP_RECORD_COUNT] = strndup(buf, strlen(buf));
    resp->nr_array1_records = rowid;

    err = GWEB_MYSQL_OK;
    ret = MYSQL_STATUS_OK;

__bail_out:
    if (result) {
        mysql_free_result(result);
    }
    gweb_mysql_update_response(JSON_C_CXN_REQUEST_QUERY_RESP, err, j2cresp);
    return ret;
}

int
gweb_mysql_handle_cxn_channel_query (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    int match_count, direction = 0;
    int max_rows, idx, rowid = 0, len = 0;
    int err = GWEB_MYSQL_ERR_UNKNOWN, ret = MYSQL_STATUS_FAIL;
    uint8_t qrybuf[MAX_MYSQL_QRYSZ], buf[32];

    char *fname = NULL, *lname = NULL, *avatar = NULL;
    const char *uid = NULL;

    MYSQL_RES *result = NULL;
    MYSQL_ROW row;

    struct j2c_cxn_channel_query_msg *jrecord = &j2cmsg->cxn_channel_query;
    struct j2c_cxn_channel_query_resp *resp = NULL;
    struct j2c_cxn_channel_query_resp_array1 *arr = NULL;

    gweb_mysql_prepare_response(JSON_C_CXN_CHANNEL_QUERY_RESP,
                                GWEB_MYSQL_OK,
                                j2cresp);
    resp = &(*j2cresp)->cxn_channel_query;
    resp->nr_array1_records = -1; /* No records */

    PUSH_BUF(qrybuf, len, "SELECT FromUID, ToUID, ConnectedOn, ChannelId "
             "FROM UserConnectChannel WHERE TRUE ");

    if (jrecord->fields[FIELD_CXN_CHANNEL_QUERY_FROM_UID]) {
        uid = jrecord->fields[FIELD_CXN_CHANNEL_QUERY_FROM_UID];
        direction = CXN_OUTBOUND;
        PUSH_BUF(qrybuf, len, "AND FromUID='%s' ", uid);

    } else if (jrecord->fields[FIELD_CXN_CHANNEL_QUERY_TO_UID]) {
        uid = jrecord->fields[FIELD_CXN_CHANNEL_QUERY_TO_UID];
        direction = CXN_INBOUND;
        PUSH_BUF(qrybuf, len, "AND ToUID='%s' ", uid);
    }

    if (!direction || !gweb_mysql_check_uid_email(uid, NULL)) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    if (jrecord->fields[FIELD_CXN_CHANNEL_QUERY_TYPE]) {
        PUSH_BUF(qrybuf, len, "AND ChannelId='%s' ",
                 jrecord->fields[FIELD_CXN_CHANNEL_QUERY_TYPE]);
    }

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

    match_count = mysql_num_rows(result);
    if (match_count == 0) {
        goto __send_record;
    }

    /* *TBD* For easier access, dump all requested data to a file in
     * JSON format and upload.
     */
    max_rows = (match_count >= MYSQL_MAX_CXN_CHANNEL_ROWS_PER_QUERY) ?
        MYSQL_MAX_CXN_CHANNEL_ROWS_PER_QUERY: match_count;

    resp->array1 = calloc(sizeof(struct j2c_cxn_channel_query_resp_array1),
                          max_rows);
    if (!resp->array1) {
        err = GWEB_MYSQL_ERR_NO_MEMORY;
        goto __bail_out;
    }

    for (rowid = idx = 0; idx < match_count && rowid < max_rows; idx++) {
        if ((row = mysql_fetch_row(result)) == NULL) {
            err = GWEB_MYSQL_ERR_UNKNOWN;
            goto __bail_out;
        }
        if (!row[0] || !row[1]) { /* ToUID */
            /* Skip if there is no ToUID */
            continue;
        }

        /* NOTE: Number of rows in response could be lesser than max_rows */
        arr = &resp->array1[rowid++];

        if (direction == CXN_INBOUND) {
            arr->fields[CXN_CHNL_IDX(UID)] = strndup(row[0], strlen(row[0]));
        } else {
            arr->fields[CXN_CHNL_IDX(UID)] = strndup(row[1], strlen(row[1]));
        }

        if (row[2]) { /* SentOn */
            arr->fields[CXN_CHNL_IDX(DATE)] = strndup(row[2], strlen(row[2]));
        }

        if (row[3]) { /* Type */
            arr->fields[CXN_CHNL_IDX(CHANNEL_TYPE)] = strndup(row[3], strlen(row[3]));
        }

        if (gweb_mysql_get_name_avatar_from_uid(arr->fields[CXN_CHNL_IDX(UID)],
                           &fname, &lname, &avatar) != MYSQL_STATUS_OK) {
            free(arr->fields[CXN_CHNL_IDX(UID)]);
            free(arr->fields[CXN_CHNL_IDX(DATE)]);
            rowid--;
            continue;
        }
        if (fname) {
            arr->fields[CXN_CHNL_IDX(FNAME)] = fname;
        }
        if (lname) {
            arr->fields[CXN_CHNL_IDX(LNAME)] = lname;
        }
        if (avatar) {
            arr->fields[CXN_CHNL_IDX(AVATAR_URL)] = avatar;
        }
    }

__send_record:
    sprintf(buf, "%d", match_count);
    resp->fields[FIELD_CXN_CHANNEL_QUERY_RESP_RECORD_COUNT] = strndup(buf, strlen(buf));
    resp->nr_array1_records = rowid;

    err = GWEB_MYSQL_OK;
    ret = MYSQL_STATUS_OK;

__bail_out:
    if (result) {
        mysql_free_result(result);
    }
    gweb_mysql_update_response(JSON_C_CXN_CHANNEL_QUERY_RESP, err, j2cresp);
    return ret;
}

int
gweb_mysql_handle_uid_query (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    uint8_t qrybuf[MAX_MYSQL_QRYSZ];
    int err = GWEB_MYSQL_ERR_UNKNOWN, ret = MYSQL_STATUS_FAIL;
    int len = 0;

    MYSQL_RES *result = NULL;
    MYSQL_ROW row;

    J2C_MSG_TABLE(uid_query, *jrecord) = &j2cmsg->uid_query;
    J2C_RESP_TABLE(uid_query, *resp) = NULL;

    gweb_mysql_prepare_response(JSON_C_UID_QUERY_RESP, GWEB_MYSQL_OK, j2cresp);
    resp = &(*j2cresp)->uid_query;

    if (!jrecord->fields[FIELD_UID_QUERY_EMAIL]) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    PUSH_BUF(qrybuf, len, "SELECT UID FROM UserRegInfo WHERE Email='%s'",
             jrecord->fields[FIELD_UID_QUERY_EMAIL]);
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

    if (mysql_num_rows(result) == 0) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    /* Pick the first matching record */
    if ((row = mysql_fetch_row(result)) == NULL) {
        report_mysql_error_noclose(g_mysql_ctx);
        goto __bail_out;
    }

    resp->fields[FIELD_UID_QUERY_RESP_UID] = strndup(row[0], strlen(row[0]));

    err = GWEB_MYSQL_OK;
    ret = MYSQL_STATUS_OK;

__bail_out:
    if (result) {
        mysql_free_result(result);
    }
    gweb_mysql_update_response(JSON_C_UID_QUERY_RESP, err, j2cresp);
    return ret;
}

int
gweb_mysql_handle_profile_query (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    int err = GWEB_MYSQL_ERR_UNKNOWN, ret = MYSQL_STATUS_FAIL;
    int count;

    J2C_MSG_TABLE(profile_query, *jrecord) = &j2cmsg->profile_query;

    gweb_mysql_prepare_response(JSON_C_PROFILE_INFO_RESP,
                                GWEB_MYSQL_OK,
                                j2cresp);

    if (!jrecord->fields[FIELD_PROFILE_QUERY_UID]) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    count = gweb_mysql_check_uid_email(jrecord->fields[FIELD_PROFILE_QUERY_UID],
                                       NULL);
    if (count < 0) {
        goto __bail_out;
    }

    if (count == 0) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    err = gweb_mysql_populate_profile_info(*j2cresp,
                     jrecord->fields[FIELD_PROFILE_QUERY_UID], NULL);
    if (err != GWEB_MYSQL_OK) {
        goto __bail_out;
    }

    ret = MYSQL_STATUS_OK;

__bail_out:
    gweb_mysql_update_response(JSON_C_PROFILE_INFO_RESP, err, j2cresp);

    return ret;
}

int
gweb_mysql_handle_avatar_query (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    uint8_t qrybuf[MAX_MYSQL_QRYSZ];
    int err = GWEB_MYSQL_ERR_UNKNOWN, ret = MYSQL_STATUS_FAIL;
    int len = 0;

    MYSQL_RES *result = NULL;
    MYSQL_ROW row;

    J2C_MSG_TABLE(avatar_query, *jrecord) = &j2cmsg->avatar_query;
    J2C_RESP_TABLE(avatar_query, *resp) = NULL;

    gweb_mysql_prepare_response(JSON_C_AVATAR_QUERY_RESP, GWEB_MYSQL_OK, j2cresp);
    resp = &(*j2cresp)->avatar_query;

    if (!jrecord->fields[FIELD_AVATAR_QUERY_UID]) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    PUSH_BUF(qrybuf, len, "SELECT AvatarURL FROM UserRegInfo WHERE UID='%s'",
             jrecord->fields[FIELD_AVATAR_QUERY_UID]);
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

    if (mysql_num_rows(result) == 0) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    /* Pick the first matching record */
    if ((row = mysql_fetch_row(result)) == NULL) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        report_mysql_error_noclose(g_mysql_ctx);
        goto __bail_out;
    }

    if (row[0]) {
        resp->fields[FIELD_AVATAR_QUERY_RESP_URL] = strndup(row[0], strlen(row[0]));
    } else {
        resp->fields[FIELD_AVATAR_QUERY_RESP_URL] = strndup("", strlen(""));
    }

    err = GWEB_MYSQL_OK;
    ret = MYSQL_STATUS_OK;

__bail_out:
    if (result) {
        mysql_free_result(result);
    }
    gweb_mysql_update_response(JSON_C_AVATAR_QUERY_RESP, err, j2cresp);
    return ret;
}

/* CXN - Preference */
#define MYSQL_MAX_CXN_PREF_QUERY    (20)

#define CXN_PREF_INS_IDX(x)                                             \
    ((FIELD_CXN_PREFERENCE_##x) - FIELD_CXN_PREFERENCE_ARRAY_START - 1)

int
gweb_mysql_handle_cxn_preference (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    uint8_t qrybuf[MAX_MYSQL_QRYSZ], *qry_delete, *qry_insert;
    int idx, len = 0, err = GWEB_MYSQL_ERR_UNKNOWN, ret = MYSQL_STATUS_FAIL;

    const char *uid = NULL;

    J2C_MSG_TABLE(cxn_preference, *jrecord) = &j2cmsg->cxn_preference;
    struct j2c_cxn_preference_msg_array1 *arr;

    gweb_mysql_prepare_response(JSON_C_CXN_PREFERENCE_RESP,
                                GWEB_MYSQL_OK,
                                j2cresp);

    uid = jrecord->fields[FIELD_CXN_PREFERENCE_UID];

    if (!uid || !gweb_mysql_check_uid_email(uid, NULL) || !jrecord->nr_array1_records) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    /* Delete records that already exists from the given set */
    qry_delete = &qrybuf[0];

    PUSH_BUF(qrybuf, len,
             "DELETE FROM CxnPrefTable USING UserConnectPreferences AS CxnPrefTable "
             "WHERE CxnPrefTable.UID='%s' AND FIND_IN_SET(CxnPrefTable.ChannelId, '",
             uid);

    for (idx = 0; idx < jrecord->nr_array1_records; idx++) {
        arr = &jrecord->array1[idx];
        if (arr->fields[CXN_PREF_INS_IDX(CHANNEL_TYPE)]) {
            PUSH_BUF(qrybuf, len, "%s,", arr->fields[CXN_PREF_INS_IDX(CHANNEL_TYPE)]);
        }
    }
    if (idx) {
        len--;
        PUSH_BUF(qrybuf, len, "')");
    }
    qrybuf[len++] = '\0';

    /* Insert records */
    qry_insert = &qrybuf[len];
    PUSH_BUF(qrybuf, len, "INSERT INTO UserConnectPreferences "
             "(UID, ChannelId, ChannelFlags) VALUES ");

    for (idx = 0; idx < jrecord->nr_array1_records; idx++) {
        arr = &jrecord->array1[idx];
        if (arr->fields[CXN_PREF_INS_IDX(CHANNEL_TYPE)]) {
            PUSH_BUF(qrybuf, len, "('%s', '%s', '%s'),",
                     uid, arr->fields[CXN_PREF_INS_IDX(CHANNEL_TYPE)],
                     arr->fields[CXN_PREF_INS_IDX(FLAG)]);
        }
    }
    if (idx) {
        qrybuf[--len] = '\0';
    }

    gweb_mysql_start_transaction();

    if (mysql_query(g_mysql_ctx, qry_delete)) {
        goto __abort_transaction;
    }

    if (mysql_query(g_mysql_ctx, qry_insert)) {
        goto __abort_transaction;
    }

    gweb_mysql_commit_transaction();

    err = GWEB_MYSQL_OK;
    ret = MYSQL_STATUS_OK;

__bail_out:
    gweb_mysql_update_response(JSON_C_CXN_PREFERENCE_RESP, err, j2cresp);
    return ret;

__abort_transaction:
    report_mysql_error_noclose(g_mysql_ctx);
    gweb_mysql_abort_transaction();
    gweb_mysql_update_response(JSON_C_CXN_PREFERENCE_RESP, err, j2cresp);
    return ret;
}

int
gweb_mysql_handle_cxn_preference_query (j2c_msg_t *j2cmsg, j2c_resp_t **j2cresp)
{
    int match_count, max_rows, idx, rowid = 0, len = 0;
    int err = GWEB_MYSQL_ERR_UNKNOWN, ret = MYSQL_STATUS_FAIL;
    uint8_t qrybuf[MAX_MYSQL_QRYSZ], buf[32];
    const char *uid = NULL;

    MYSQL_RES *result = NULL;
    MYSQL_ROW row;

    J2C_MSG_TABLE(cxn_preference_query, *jrecord) = &j2cmsg->cxn_preference_query;
    J2C_RESP_TABLE(cxn_preference_query, *resp) = NULL;
    struct j2c_cxn_preference_query_resp_array1 *arr = NULL;

    gweb_mysql_prepare_response(JSON_C_CXN_PREFERENCE_QUERY_RESP,
                                GWEB_MYSQL_OK,
                                j2cresp);

    resp = &(*j2cresp)->cxn_preference_query;
    resp->nr_array1_records = -1; /* No records */

    uid = jrecord->fields[FIELD_CXN_PREFERENCE_QUERY_UID];

    if (!uid || !gweb_mysql_check_uid_email(uid, NULL)) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    PUSH_BUF(qrybuf, len,
             "SELECT UID, ChannelId, ChannelFlags FROM UserConnectPreferences "
             "WHERE UID='%s'", uid);
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

    match_count = mysql_num_rows(result);
    if (match_count == 0) {
        err = GWEB_MYSQL_ERR_NO_RECORD;
        goto __bail_out;
    }

    /* *TBD* For easier access, dump all requested data to a file in
     * JSON format and upload.
     */
    max_rows = (match_count >= MYSQL_MAX_CXN_PREFERENCE_ROWS_PER_QUERY) ?
        MYSQL_MAX_CXN_PREFERENCE_ROWS_PER_QUERY: match_count;

    resp->array1 = calloc(sizeof(struct j2c_cxn_preference_query_resp_array1),
                          max_rows);
    if (!resp->array1) {
        err = GWEB_MYSQL_ERR_NO_MEMORY;
        goto __bail_out;
    }

    for (rowid = idx = 0; idx < match_count && rowid < max_rows; idx++) {
        if ((row = mysql_fetch_row(result)) == NULL) {
            err = GWEB_MYSQL_ERR_UNKNOWN;
            goto __bail_out;
        }

        /* NOTE: Number of rows in response could be lesser than max_rows */
        arr = &resp->array1[rowid++];

        arr->fields[CXN_PREF_IDX(CHANNEL_TYPE)] = strndup(row[1], strlen(row[1]));
        arr->fields[CXN_PREF_IDX(FLAG)] = strndup(row[2], strlen(row[2]));
    }

    sprintf(buf, "%d", match_count);
    resp->fields[FIELD_CXN_PREFERENCE_QUERY_RESP_RECORD_COUNT] = strndup(buf, strlen(buf));
    resp->nr_array1_records = rowid;

    err = GWEB_MYSQL_OK;
    ret = MYSQL_STATUS_OK;

__bail_out:
    if (result) {
        mysql_free_result(result);
    }
    gweb_mysql_update_response(JSON_C_CXN_PREFERENCE_QUERY_RESP, err, j2cresp);
    return ret;
}


int
gweb_mysql_free_profile_query (j2c_resp_t *j2cresp)
{
    if (j2cresp) {
        gweb_mysql_free_profile_info(j2cresp);
        free(j2cresp);
    }
    return MYSQL_STATUS_OK;
}

int
gweb_mysql_free_uid_query (j2c_resp_t *j2cresp)
{
    if (j2cresp) {
        if (j2cresp->uid_query.fields[FIELD_UID_QUERY_RESP_UID]) {
            free(j2cresp->uid_query.fields[FIELD_UID_QUERY_RESP_UID]);
        }
        free(j2cresp);
    }
    return MYSQL_STATUS_OK;
}

int
gweb_mysql_free_avatar_query (j2c_resp_t *j2cresp)
{
    if (j2cresp) {
        if (j2cresp->avatar_query.fields[FIELD_AVATAR_QUERY_RESP_URL]) {
            free(j2cresp->avatar_query.fields[FIELD_AVATAR_QUERY_RESP_URL]);
        }
        free(j2cresp);
    }
    return MYSQL_STATUS_OK;
}

int
gweb_mysql_free_cxn_request (j2c_resp_t *j2cresp)
{
    if (j2cresp) {
        free(j2cresp);
    }
    return MYSQL_STATUS_OK;
}

int
gweb_mysql_free_cxn_channel (j2c_resp_t *j2cresp)
{
    if (j2cresp) {
        free(j2cresp);
    }
    return MYSQL_STATUS_OK;
}

int
gweb_mysql_free_cxn_preference (j2c_resp_t *j2cresp)
{
    if (j2cresp) {
        free(j2cresp);
    }
    return MYSQL_STATUS_OK;
}

#define J2CRESP_FREE_ARRAY(arr, nr, maxfld)             \
    do {                                                \
        int idx, fld;                                   \
        if (arr) {                                      \
            for (idx = 0; idx < nr; idx++) {            \
                for (fld = 0; fld < maxfld; fld++) {    \
                    if (arr[idx].fields[fld]) {         \
                        free(arr[idx].fields[fld]);     \
                    }                                   \
                }                                       \
            }                                           \
            free(arr);                                  \
        }                                               \
    } while(0)

int
gweb_mysql_free_cxn_request_query (j2c_resp_t *j2cresp)
{
    struct j2c_cxn_request_query_resp *resp = NULL;

    if (j2cresp) {
        resp = &j2cresp->cxn_request_query;
        J2CRESP_FREE_ARRAY(resp->array1, resp->nr_array1_records,
                           CXN_REQ_IDX(ARRAY_END));
        free(resp->fields[FIELD_CXN_REQUEST_QUERY_RESP_RECORD_COUNT]);
        free(j2cresp);
    }
    return MYSQL_STATUS_OK;
}

int
gweb_mysql_free_cxn_channel_query (j2c_resp_t *j2cresp)
{
    struct j2c_cxn_channel_query_resp *resp = NULL;

    if (j2cresp) {
        resp = &j2cresp->cxn_channel_query;
        J2CRESP_FREE_ARRAY(resp->array1, resp->nr_array1_records,
                           CXN_CHNL_IDX(ARRAY_END));
        free(resp->fields[FIELD_CXN_CHANNEL_QUERY_RESP_RECORD_COUNT]);
        free(j2cresp);
    }
    return MYSQL_STATUS_OK;
}

int
gweb_mysql_free_cxn_preference_query (j2c_resp_t *j2cresp)
{
    struct j2c_cxn_preference_query_resp *resp = NULL;

    if (j2cresp) {
        resp = &j2cresp->cxn_preference_query;
        J2CRESP_FREE_ARRAY(resp->array1, resp->nr_array1_records,
                           CXN_PREF_IDX(ARRAY_END));
        free(resp->fields[FIELD_CXN_PREFERENCE_QUERY_RESP_RECORD_COUNT]);
        free(j2cresp);
    }
    return MYSQL_STATUS_OK;
}

int
gweb_mysql_free_profile (j2c_resp_t *j2cresp)
{
    if (j2cresp) {
        free(j2cresp);
    }
    return MYSQL_STATUS_OK;
}

int
gweb_mysql_shutdown (void)
{
    log_debug("Closing MySQL connection!\n");

    if (g_mysql_ctx)
	mysql_close(g_mysql_ctx);

    return MYSQL_STATUS_OK;
}

static int
gweb_mysql_connect (MYSQL *ctx)
{
    if (!ctx) {
        return MYSQL_STATUS_FAIL;
    }

    if (mysql_real_connect(ctx,
                           g_mysql_cfg->host,
                           g_mysql_cfg->username,
                           g_mysql_cfg->password,
                           g_mysql_cfg->database,
                           0,
                           NULL,
                           CLIENT_MULTI_STATEMENTS) == NULL) {
        return MYSQL_STATUS_FAIL;
    }

    return MYSQL_STATUS_OK;
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

    return MYSQL_STATUS_OK;
}

int
gweb_mysql_init (void)
{
    my_bool reconnect = 1;

    if ((g_mysql_cfg = config_load_mysqldb()) == NULL) {
        log_error("Invalid MYSQL configuration, bailing out\n");
        return MYSQL_STATUS_FAIL;
    }

    log_debug("Initializing schema, MySQL version = %s\n",
	      mysql_get_client_info());

    if ((g_mysql_ctx = mysql_init(NULL)) == NULL) {
	report_mysql_error(g_mysql_ctx);
    }

    mysql_options(g_mysql_ctx, MYSQL_OPT_RECONNECT, &reconnect);

    if (gweb_mysql_connect(g_mysql_ctx) != MYSQL_STATUS_OK) {
        report_mysql_error(g_mysql_ctx);
    }

    log_debug("MySQL connected!\n");

    return MYSQL_STATUS_OK;
}
