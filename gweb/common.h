#ifndef COMMON_H
#define COMMON_H

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x)   sizeof(x)/sizeof(x[0])
#endif

#ifdef LOG_TO_SYSLOG
#include <syslog.h>
#endif

#ifdef DEBUG
#ifdef LOG_TO_SYSLOG
#  define log_notice(...)  syslog(LOG_NOTICE, __VA_ARGS__)
#  define log_debug(...)   syslog(LOG_DEBUG,  __VA_ARGS__)
#  define log_error(...)   syslog(LOG_ERR,    __VA_ARGS__)

#else
#  define log_debug(...)   fprintf(stdout, __VA_ARGS__)
#  define log_error(...)   fprintf(stdout, __VA_ARGS__)
#  define log_notice(...)  fprintf(stdout, __VA_ARGS__)
#endif /* LOG_TO_SYSLOG */

#else
#  define log_debug(...)   do { } while (0)
#  define log_error(...)   do { } while (0)
#  define log_notice(...)  do { } while (0)
#endif

#endif // COMMON_H
