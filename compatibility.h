
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

#ifdef _MSC_VER
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64;
typedef unsigned __int64 uint64_t;
typedef __int16 int16_t;
#else
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#endif

#ifdef __cplusplus
}
#endif
#endif
