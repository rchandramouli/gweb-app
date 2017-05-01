#ifndef UID_H
#define UID_H

#define MAX_UID_STRSZ   (16)

extern uint64_t gweb_app_get_uid (const char *phone, const char *email);

extern void gweb_app_get_uid_str (const char *phone, const char *email,
                                  char *uid);

#endif // UID_H
