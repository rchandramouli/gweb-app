#ifndef AVATARDB_H
#define AVATARDB_H

extern int avatardb_init (void);

extern int
avatardb_handle_upload_complete (void **priv, char **response, int *status);

extern int
avatardb_handle_uploaded_block (void **priv, const char *key, const char *data,
                                size_t size, uint64_t off, const char *content_type,
                                const char *encoding, char **response, int *status);

extern int avatardb_handle_upload_cleanup (void **priv);

#endif // AVATARDB_H
