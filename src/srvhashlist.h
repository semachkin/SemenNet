#ifndef HASHLIST_H
#define HASHLIST_H

#include "stdint.h"
#include "minwindef.h"

typedef unsigned char ENUMT;

typedef struct STRVAL_S {
    char *p; 
    size_t len;
} STRVAL;

typedef struct TYPEOBJECT_S {
    void *data;
    ENUMT type;
} TYPEOBJECT;

typedef struct HASHSTRVAL_S {
    TYPEOBJECT obj;
    struct HASHSTRVAL_S *last;
    uint16_t hash;
} HASHSTRVAL;

typedef struct HASHLIST_S {
    HASHSTRVAL **buff;
    uint16_t size;
    uint16_t nuse;
} HASHLIST;

#define HashIndexing(l, s, p) \
    { STRVAL key = STRVALObj(s, strlen(s)); p = HashGet(l, key); }

#ifndef ARRAlloc
#define ARRAlloc(t, s) (cast(malloc(s*sizeof(t)), t*))
#endif

#ifndef MEMFree
#define MEMFree(p) \
    if (p != NULL) free(p); \
    p = NULL;
#endif

#ifndef cast
#define cast(a, t) ((t)(a))
#endif

#ifndef InlineApi
#define InlineApi static inline
#endif

#ifndef MAX_NUM
#define MAX_NUM(t) ((~(t)0)-2)
#endif

#define HASHLIST_STARTSIZE 16
#define MAX_HASHLIST_SIZE MAX_NUM(uint16_t)

void HashSetVal(HASHLIST *list, STRVAL key, TYPEOBJECT val);
void HashListRealloc(HASHLIST *list, uint16_t newsize);
void HashListClean(HASHLIST *list);
HASHSTRVAL *HashGet(HASHLIST *list, STRVAL key);

#endif