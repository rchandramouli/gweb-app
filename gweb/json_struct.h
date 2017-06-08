#ifndef JSON_STRUCT_H
#define JSON_STRUCT_H

/*
 * JSON to C structure map used by MySQL and other dump routines. Each
 * of the message below has a corresponding response structure.
 */
enum {
    /* MSG-START */ JSON_C_MSG_MIN,
    JSON_C_REGISTRATION_MSG,
    JSON_C_PROFILE_MSG,
    JSON_C_LOGIN_MSG,
    JSON_C_AVATAR_MSG,
    JSON_C_CXN_REQUEST_MSG,
    JSON_C_CXN_CHANNEL_MSG,
    JSON_C_CXN_REQUEST_QUERY_MSG,
    JSON_C_CXN_CHANNEL_QUERY_MSG,
    /* MSG-END */ JSON_C_MSG_MAX,
};

enum {
    /* RESP-START */ JSON_C_RESP_MIN,
    JSON_C_REGISTRATION_RESP,
    JSON_C_PROFILE_RESP,
    JSON_C_LOGIN_RESP,
    JSON_C_AVATAR_RESP,
    JSON_C_CXN_REQUEST_RESP,
    JSON_C_CXN_CHANNEL_RESP,
    JSON_C_CXN_REQUEST_QUERY_RESP,
    JSON_C_CXN_CHANNEL_QUERY_RESP,
    /* RESP-END */ JSON_C_RESP_MAX,
};

/* Magic constant to indicate the array start/end in framing
 * response.
 */
#define JSON_C_ARRAY_START     ((void *)0xDEADCAFE)
#define JSON_C_ARRAY_END       ((void *)0xEDDAACEF)

enum _JSON_C_REGISTRATION_MSG_FIELDS {
    FIELD_REGISTRATION_FNAME,
    FIELD_REGISTRATION_LNAME,
    FIELD_REGISTRATION_EMAIL,
    FIELD_REGISTRATION_PHONE,
    FIELD_REGISTRATION_PASSWORD,
    FIELD_REGISTRATION_MAX,
};

enum _JSON_C_PROFILE_MSG_FIELDS {
    FIELD_PROFILE_UID,
    FIELD_PROFILE_ADDRESS1,
    FIELD_PROFILE_ADDRESS2,
    FIELD_PROFILE_ADDRESS3,
    FIELD_PROFILE_COUNTRY,
    FIELD_PROFILE_STATE,
    FIELD_PROFILE_PINCODE,
    FIELD_PROFILE_FACEBOOK_HANDLE,
    FIELD_PROFILE_TWITTER_HANDLE,
    FIELD_PROFILE_MAX,
};

enum _JSON_C_LOGIN_MSG_FIELDS {
    FIELD_LOGIN_EMAIL,
    FIELD_LOGIN_PASSWORD,
    FIELD_LOGIN_MAX,
};

enum _JSON_C_AVATAR_MSG_FIELDS {
    FIELD_AVATAR_UID,
    FIELD_AVATAR_URL,
    FIELD_AVATAR_MAX,
};

enum _JSON_C_CXN_REQUEST_MSG_FIELDS {
    FIELD_CXN_REQUEST_UID,
    FIELD_CXN_REQUEST_TO_UID,
    FIELD_CXN_REQUEST_FLAG,
    FIELD_CXN_REQUEST_MAX,
};

enum _JSON_C_CXN_CHANNEL_MSG_FIELDS {
    FIELD_CXN_CHANNEL_UID,
    FIELD_CXN_CHANNEL_TO_UID,
    FIELD_CXN_CHANNEL_TYPE,
    FIELD_CXN_CHANNEL_MAX,
};

enum _JSON_C_CXN_REQUEST_QUERY_MSG_FIELDS {
    FIELD_CXN_REQUEST_QUERY_FROM_UID,
    FIELD_CXN_REQUEST_QUERY_TO_UID,
    FIELD_CXN_REQUEST_QUERY_FLAG,
    FIELD_CXN_REQUEST_QUERY_MAX,
};

enum _JSON_C_CXN_CHANNEL_QUERY_MSG_FIELDS {
    FIELD_CXN_CHANNEL_QUERY_FROM_UID,
    FIELD_CXN_CHANNEL_QUERY_TO_UID,
    FIELD_CXN_CHANNEL_QUERY_TYPE,
    FIELD_CXN_CHANNEL_QUERY_MAX,
};

enum _JSON_C_REGISTRATION_RESP_FIELDS {
    FIELD_REGISTRATION_RESP_CODE,
    FIELD_REGISTRATION_RESP_DESC,
    FIELD_REGISTRATION_RESP_UID,
    FIELD_REGISTRATION_RESP_MAX,
};

enum _JSON_C_PROFILE_RESP_FIELDS {
    FIELD_PROFILE_RESP_CODE,
    FIELD_PROFILE_RESP_DESC,
    FIELD_PROFILE_RESP_MAX,
};

enum _JSON_C_LOGIN_RESP_FIELDS {
    FIELD_LOGIN_RESP_CODE,
    FIELD_LOGIN_RESP_DESC,
    FIELD_LOGIN_RESP_UID,
    FIELD_LOGIN_RESP_FNAME,
    FIELD_LOGIN_RESP_LNAME,
    FIELD_LOGIN_RESP_EMAIL,
    FIELD_LOGIN_RESP_PHONE,
    FIELD_LOGIN_RESP_ADDRESS1,
    FIELD_LOGIN_RESP_ADDRESS2,
    FIELD_LOGIN_RESP_ADDRESS3,
    FIELD_LOGIN_RESP_COUNTRY,
    FIELD_LOGIN_RESP_STATE,
    FIELD_LOGIN_RESP_PINCODE,
    FIELD_LOGIN_RESP_FACEBOOK_HANDLE,
    FIELD_LOGIN_RESP_TWITTER_HANDLE,
    FIELD_LOGIN_RESP_AVATAR_URL,
    FIELD_LOGIN_RESP_MAX,
};

enum _JSON_C_AVATAR_RESP_FIELDS {
    FIELD_AVATAR_RESP_CODE,
    FIELD_AVATAR_RESP_DESC,
    FIELD_AVATAR_RESP_MAX,
};

enum _JSON_C_CXN_REQUEST_RESP_FIELDS {
    FIELD_CXN_REQUEST_RESP_CODE,
    FIELD_CXN_REQUEST_RESP_DESC,
    FIELD_CXN_REQUEST_RESP_MAX,
};

enum _JSON_C_CXN_CHANNEL_RESP_FIELDS {
    FIELD_CXN_CHANNEL_RESP_CODE,
    FIELD_CXN_CHANNEL_RESP_DESC,
    FIELD_CXN_CHANNEL_RESP_MAX,
};

enum _JSON_C_CXN_REQUEST_QUERY_RESP_FIELDS {
    FIELD_CXN_REQUEST_QUERY_RESP_CODE,
    FIELD_CXN_REQUEST_QUERY_RESP_DESC,
    FIELD_CXN_REQUEST_QUERY_RESP_RECORD_COUNT,
    FIELD_CXN_REQUEST_QUERY_RESP_ARRAY_START,
    FIELD_CXN_REQUEST_QUERY_RESP_UID,
    FIELD_CXN_REQUEST_QUERY_RESP_FNAME,
    FIELD_CXN_REQUEST_QUERY_RESP_LNAME,
    FIELD_CXN_REQUEST_QUERY_RESP_AVATAR_URL,
    FIELD_CXN_REQUEST_QUERY_RESP_DATE,
    FIELD_CXN_REQUEST_QUERY_RESP_FLAG,
    FIELD_CXN_REQUEST_QUERY_RESP_ARRAY_END,
    FIELD_CXN_REQUEST_QUERY_RESP_MAX,
};

enum _JSON_C_CXN_CHANNEL_QUERY_RESP_FIELDS {
    FIELD_CXN_CHANNEL_QUERY_RESP_CODE,
    FIELD_CXN_CHANNEL_QUERY_RESP_DESC,
    FIELD_CXN_CHANNEL_QUERY_RESP_RECORD_COUNT,
    FIELD_CXN_CHANNEL_QUERY_RESP_ARRAY_START,
    FIELD_CXN_CHANNEL_QUERY_RESP_UID,
    FIELD_CXN_CHANNEL_QUERY_RESP_FNAME,
    FIELD_CXN_CHANNEL_QUERY_RESP_LNAME,
    FIELD_CXN_CHANNEL_QUERY_RESP_AVATAR_URL,
    FIELD_CXN_CHANNEL_QUERY_RESP_DATE,
    FIELD_CXN_CHANNEL_QUERY_RESP_CHANNEL_TYPE,
    FIELD_CXN_CHANNEL_QUERY_RESP_ARRAY_END,
    FIELD_CXN_CHANNEL_QUERY_RESP_MAX,
};

struct j2c_registration_msg {
    const char *fields[FIELD_REGISTRATION_MAX];
};

struct j2c_profile_msg {
    const char *fields[FIELD_PROFILE_MAX];
};

struct j2c_login_msg {
    const char *fields[FIELD_LOGIN_MAX];
};

struct j2c_avatar_msg {
    const char *fields[FIELD_AVATAR_MAX];
};

struct j2c_cxn_request_msg {
    const char *fields[FIELD_CXN_REQUEST_MAX];
};

struct j2c_cxn_channel_msg {
    const char *fields[FIELD_CXN_CHANNEL_MAX];
};

struct j2c_cxn_request_query_msg {
    const char *fields[FIELD_CXN_REQUEST_QUERY_MAX];
};

struct j2c_cxn_channel_query_msg {
    const char *fields[FIELD_CXN_CHANNEL_QUERY_MAX];
};

typedef union {
    struct j2c_registration_msg registration;
    struct j2c_profile_msg      profile;
    struct j2c_login_msg        login;
    struct j2c_avatar_msg       avatar;

    struct j2c_cxn_request_msg  cxn_request;
    struct j2c_cxn_channel_msg  cxn_channel;

    struct j2c_cxn_request_query_msg  cxn_request_query;
    struct j2c_cxn_channel_query_msg  cxn_channel_query;
} j2c_msg_t;

struct j2c_registration_resp {
    char *fields[FIELD_REGISTRATION_RESP_MAX];
};

struct j2c_profile_resp {
    char *fields[FIELD_PROFILE_RESP_MAX];
};

struct j2c_login_resp {
    char *fields[FIELD_LOGIN_RESP_MAX];
};

struct j2c_avatar_resp {
    char *fields[FIELD_AVATAR_RESP_MAX];
};

struct j2c_cxn_request_resp {
    char *fields[FIELD_CXN_REQUEST_RESP_MAX];
};

struct j2c_cxn_channel_resp {
    char *fields[FIELD_CXN_CHANNEL_RESP_MAX];
};

struct j2c_cxn_request_query_resp_array1 {
    char *fields[FIELD_CXN_REQUEST_QUERY_RESP_ARRAY_END -
                 FIELD_CXN_REQUEST_QUERY_RESP_ARRAY_START];
};

struct j2c_cxn_request_query_resp {
    char *fields[FIELD_CXN_REQUEST_QUERY_RESP_ARRAY_START];
    int nr_array1_records;
    struct j2c_cxn_request_query_resp_array1 *array1;
};

struct j2c_cxn_channel_query_resp_array1 {
    char *fields[FIELD_CXN_CHANNEL_QUERY_RESP_ARRAY_END -
                 FIELD_CXN_CHANNEL_QUERY_RESP_ARRAY_START];
};

struct j2c_cxn_channel_query_resp {
    char *fields[FIELD_CXN_CHANNEL_QUERY_RESP_ARRAY_START];
    int nr_array1_records;
    struct j2c_cxn_channel_query_resp_array1 *array1;
};

typedef union {
    struct j2c_registration_resp  registration;
    struct j2c_profile_resp       profile;
    struct j2c_login_resp         login;
    struct j2c_avatar_resp        avatar;

    struct j2c_cxn_request_resp       cxn_request;
    struct j2c_cxn_request_query_resp cxn_request_query;

    struct j2c_cxn_channel_resp       cxn_channel;
    struct j2c_cxn_channel_query_resp cxn_channel_query;
} j2c_resp_t;

#endif // JSON_STRUCT_H
