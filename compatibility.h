
#ifndef COMPATIBILITY_H
#define COMPATIBILITY_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__
#define _GNU_SOURCE
#endif

#ifdef _WIN32
#include <windows.h>
#define strcasecmp      _stricmp
#define strncasecmp     _strnicmp
int _strnicmp(const char *string1, const char *string2, size_t count);
char* strcasestr(register char *s, register char *find);
char* strsep(char **stringp, const char *delim);
#endif

#ifdef __cplusplus
}
#endif
#endif
