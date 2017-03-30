#ifndef COMMON_H
#define COMMON_H

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x)   sizeof(x)/sizeof(x[0])
#endif

#ifdef DEBUG
#  define log_debug(...)  fprintf(stdout, __VA_ARGS__)
#  define log_error(...)  fprintf(stdout, __VA_ARGS__)
#else
#  define log_debug(...)  do { } while (0)
#  define log_error(...)  do { } while (0)
#endif

#endif // COMMON_H
