#ifndef JSON_STRUCT_H
#define JSON_STRUCT_H

/*
 * JSON to C structure map used by MySQL and other dump routines
 */
enum {
    JSON_C_REGISTRATION_API,
    JSON_C_LOGIN_API,
    JSON_C_API_MAX,
};

enum _JSON_C_REGISTRATION_MSG_FIELDS {
    FIELD_REGISTRATION_FNAME,
    FIELD_REGISTRATION_LNAME,
    FIELD_REGISTRATION_EMAIL,
    FIELD_REGISTRATION_PHONE,
    FIELD_REGISTRATION_ADDRESS1,
    FIELD_REGISTRATION_ADDRESS2,
    FIELD_REGISTRATION_ADDRESS3,
    FIELD_REGISTRATION_COUNTRY,
    FIELD_REGISTRATION_STATE,
    FIELD_REGISTRATION_PINCODE,
    FIELD_REGISTRATION_PASSWORD,
    FIELD_REGISTRATION_MAX,
};

enum _JSON_C_LOGIN_MSG_FIELDS {
    FIELD_LOGIN_EMAIL,
    FIELD_LOGIN_PASSWORD,
    FIELD_LOGIN_MAX,
};

struct j2c_registration {
    const char *fields[FIELD_REGISTRATION_MAX];
};

struct j2c_login {
    const char *fields[FIELD_LOGIN_MAX];
};

typedef union {
    struct j2c_registration registration;
    struct j2c_login        login;
} j2c_map_t;

#endif // JSON_STRUCT_H
