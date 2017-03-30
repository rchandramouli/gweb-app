#ifndef MYSQLDB_LOG_H
#define MYSQLDB_LOG_H

#define report_mysql_error_noaction(ctx)	\
    log_error("%s\n", mysql_error(ctx))

#define report_mysql_error_code(ctx, code)		\
    do {						\
	report_mysql_error_noaction(ctx);		\
	if (ctx)					\
	    mysql_close(ctx);				\
	return (code);					\
    } while (0)

#define report_mysql_error(ctx)			\
    report_mysql_error_code(ctx, -1)

#define report_mysql_error_noclose(ctx)	\
    do {					\
	report_mysql_error_noaction(ctx);	\
	return (-1);				\
    } while (0)

#endif // MYSQLDB_LOG_H
