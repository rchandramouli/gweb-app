#ifndef CONFIG_H
#define CONFIG_H

struct mysql_config {
    const char *host;
    const char *username;
    const char *password;
    const char *database;
};

struct avatardb_config {
    void *s3ctx;
    const char *loc_cache;
    const char *loc_fmount;
    const char *folder;
    const char *url;
};

extern int config_parse_and_load (int argc, char *argv[]);
extern int config_load_dotrc (const char *config_file);

extern struct avatardb_config *config_load_avatardb (void);
extern struct mysql_config *config_load_mysqldb (void);

#endif /* CONFIG_H */
