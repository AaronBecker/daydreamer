
#ifndef COMPATIBILITY_H
#define COMPATIBILITY_H
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform-specific settings, included before we get to anything else
 * in daydreamer.h.
 */
#ifdef __APPLE__
#endif

#ifdef __linux__
// This is needed to give access to some of the string handling functions
// that we use.
#define _GNU_SOURCE
#endif

#ifdef _WIN32
// Rename windows functions to the posix equivalent where possible,
// provide an implementation where there is no equivalent.
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
