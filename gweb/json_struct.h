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
    /* MSG-END */ JSON_C_MSG_MAX,
};

enum {
    /* RESP-START */ JSON_C_RESP_MIN,
    JSON_C_REGISTRATION_RESP,
    JSON_C_PROFILE_RESP,
    JSON_C_LOGIN_RESP,
    JSON_C_AVATAR_RESP,
    /* RESP-END */ JSON_C_RESP_MAX,
};

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
    FIELD_LOGIN_RESP_MAX,
};

enum _JSON_C_AVATAR_RESP_FIELDS {
    FIELD_AVATAR_RESP_CODE,
    FIELD_AVATAR_RESP_DESC,
    FIELD_AVATAR_RESP_MAX,
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

typedef union {
    struct j2c_registration_msg registration;
    struct j2c_profile_msg      profile;
    struct j2c_login_msg        login;
    struct j2c_avatar_msg       avatar;
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

typedef union {
    struct j2c_registration_resp  registration;
    struct j2c_profile_resp       profile;
    struct j2c_login_resp         login;
    struct j2c_avatar_resp        avatar;
} j2c_resp_t;

#endif // JSON_STRUCT_H
