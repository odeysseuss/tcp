/*
* SDS style string library
*
* NOTE: To use this library define the following macro in exactly one file
* before inlcuding str.h:
*   #define STRING_IMPLEMENTATION
*   #include "str.h"
*/

#ifndef STR_H
#define STR_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Used for custom general purpose allocators
/// To use a custom allocators simply define them to the specific allocator's functions
/// Warning: The function signatures has to be the same as stdlib's allocator
/// Recommended Allocator: Microsoft's mimalloc
#define malloc_ malloc
#define calloc_ calloc
#define realloc_ realloc
#define free_ free

typedef char *String;

/// constructors and destructors
String strNew(const char *s);
String strNewLen(const void *s, const uint32_t len);
String strEmpty(void);
void strFree(String s);

#ifdef STRING_IMPLEMENTATION

// header for String type
// used as a binary perfix that's stored before the actual String type
typedef struct {
    uint32_t len_;
    uint32_t alloc_;
} StrHdr_;

// get pointer to header from String
static inline StrHdr_ *getStrHdr_(const String s) {
    return ((StrHdr_ *)s - 1);
}

// get available memory space from String
static inline uint32_t getStrAvail_(const String s) {
    return (getStrHdr_(s)->alloc_ - getStrHdr_(s)->len_);
}

static inline uint32_t getStrLen_(const String s) {
    return (getStrHdr_(s)->len_);
}

static inline uint32_t getStrAlloc_(const String s) {
    return (getStrHdr_(s)->alloc_);
}

String strNewLen(const void *s, const uint32_t len) {
    StrHdr_ *hdr = (StrHdr_ *)malloc_(sizeof(StrHdr_) + len + 1);
    if (!hdr) {
        return NULL;
    }

    hdr->len_ = len;
    hdr->alloc_ = len;

    String str = (String)(hdr + 1);
    if (s && len > 0) {
        memcpy(str, s, len);
    }
    str[len] = '\0';

    return str;
}

String strEmpty(void) {
    return strNewLen("", 0);
}

String strNew(const char *s) {
    if (!s) {
        return NULL;
    }

    return strNewLen(s, strlen(s));
}

void strFree(String s) {
    if (!s) {
        return;
    }

    StrHdr_ *hdr = getStrHdr_(s);
    free(hdr);
}

#endif // STRING_IMPLEMENTATION

#undef malloc_
#undef calloc_
#undef realloc_
#undef free_

#ifdef __cplusplus
}
#endif

#endif
