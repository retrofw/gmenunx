#ifndef DEBUG_H
#define DEBUG_H

#define NODEBUG_L 0
#define ERROR_L 1
#define WARNING_L 2
#define INFO_L 3
#define DEBUG_L 4

#define DEBUG_COLOR		"\e[1;34m"
#define WARNING_COLOR	"\e[1;33m"
#define ERROR_COLOR		"\e[1;31m"
#define INFO_COLOR		"\e[1;32m"
#define CLEAR_COLOR		"\e[00m\n"

#ifndef LOG_LEVEL
	#define LOG_LEVEL INFO_L
#endif

#define D(str, ...) \
	fprintf(stdout, ERROR_COLOR "%s:%d %s: " str CLEAR_COLOR, __FILE__, __LINE__, __func__,  ##__VA_ARGS__)

#if (LOG_LEVEL >= DEBUG_L)
	#define DEBUG(str, ...) \
	fprintf(stdout, DEBUG_COLOR str CLEAR_COLOR, ##__VA_ARGS__)
#else
	#define DEBUG(...)
#endif

#if (LOG_LEVEL >= INFO_L)
	#define INFO(str, ...) \
	fprintf(stdout, INFO_COLOR str CLEAR_COLOR, ##__VA_ARGS__)
#else
	#define INFO(...)
#endif

#if (LOG_LEVEL >= WARNING_L)
	#define WARNING(str, ...) \
	fprintf(stderr, WARNING_COLOR str CLEAR_COLOR, ##__VA_ARGS__)
#else
	#define WARNING(...)
#endif

#if (LOG_LEVEL >= ERROR_L)
	#define ERROR(str, ...) \
	fprintf(stderr, ERROR_COLOR str CLEAR_COLOR, ##__VA_ARGS__)
#else
	#define ERROR(...)
#endif

#endif // DEBUG_H
