#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "srvhashlist.h"

void HashListRealloc(HASHLIST *list, uint16_t newsize) {
    //printf("List realloc to %d\n", newsize);
    HASHSTRVAL **newbuff = ARRAlloc(HASHSTRVAL*, newsize);
    memset(newbuff, 0, newsize*sizeof(HASHSTRVAL*));

    for (uint16_t i = 0; i < list->size; i++) {
        HASHSTRVAL *o = list->buff[i];
        if (o == NULL) continue;
        HASHSTRVAL *last = o->last;
        for (;;) {
            uint16_t newidx = o->hash & (newsize-1);
            o->last = newbuff[newidx];
            newbuff[newidx] = o;
            o = last;
            if (o == NULL) break;
            else last = last->last;
        }
    }
    MEMFree(list->buff);
    list->buff = newbuff;
    list->size = newsize;
}

#define hashfunc(k, str) \
    k = 0; \
    for (uint16_t l = 0; l < str.len; l++) k ^= k + cast(str.p[l], BYTE);

HASHSTRVAL *HashGet(HASHLIST *list, STRVAL key) {
    uint16_t hash; hashfunc(hash, key);
    for (HASHSTRVAL *v = list->buff[hash & (list->size-1)]; v != NULL;) {
        if (v->hash == hash) return v;
        v = v->last;
    }
    return cast(0, HASHSTRVAL*);
}

InlineApi HASHSTRVAL *newHashVal(HASHLIST *list, TYPEOBJECT val, uint16_t hash) {
    //printf("New hash value %d data %s\n", hash, cast(val.data, STRVAL*)->p);
    HASHSTRVAL *obj = ARRAlloc(HASHSTRVAL, 1);
    obj->obj = val;
    obj->hash = hash;
    obj->last = NULL;
    return obj;
}
void HashSetVal(HASHLIST *list, STRVAL key, TYPEOBJECT val) {
    //printf("Hash set value %s data %s\n", key.p, cast(val.data, STRVAL*)->p);
    HASHSTRVAL *hashval = HashGet(list, key);
    if (hashval != NULL) {
        hashval->obj = val;
        return;
    }
    uint16_t hash; hashfunc(hash, key);
    HASHSTRVAL *obj = newHashVal(list, val, hash);
    if (list->size == MAX_HASHLIST_SIZE)
        return;
    if (list->nuse == list->size)
        HashListRealloc(list, list->size*2);
    hash &= list->size-1;
    obj->last = list->buff[hash];
    list->buff[hash] = obj;
    list->nuse++;
    //printf("Elements count %d\n", list->nuse);
}

void HashListClean(HASHLIST *list) {
    for (uint16_t i = 0; i < list->size; i++) {
        HASHSTRVAL *last = NULL;
        for (HASHSTRVAL *o = list->buff[i]; o != NULL; o = last) {
            MEMFree(o->obj.data);
            last = o->last;
            free(o);
        }
    }
    free(list->buff);
}