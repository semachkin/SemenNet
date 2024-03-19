#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "srvdata.h"

#define ValueSetterArgs char *name, char *val, uint16_t namel, uint16_t vall

#define isname(s) (memcmp(name, s, namel) == 0)
#define isval(s) (memcmp(val, s, vall) == 0)

#define ValueSTR (STRVALObj(val, vall))

HASHLIST *MSGEncode(char *msg, size_t msglen) {
    HASHLIST *list = ARRAlloc(HASHLIST, 1);
    list->nuse = 0;
    list->size = 0;
    list->buff = NULL;
    HashListRealloc(list, HASHLIST_STARTSIZE);

    char *msgstart = msg;
    char *msgend = msg + msglen;
    char *start;
    uint16_t len;

    #define seekto(c, o) (msg = cast(memchr(msg, c, msglen - cast(msg - msgstart, size_t)), char*)+o)
    #define findcloser(o, c) { \
            uint16_t lay = 1; \
            for (; msg < msgend; msg++) { \
                if (*msg == o) lay++; \
                else if (*msg == c) lay--; \
                if (lay == 0) break; } \
        }

    life {
        start = msg;
        seekto('=', -1);
        if ((ptrdiff_t)msg == -1) break;
        len = msg - start;
        STRVAL key = STRVALObj(start, ++len);
        seekto('=', 1);
        start = msg;
        char attribute = *start;
        seekto(MA_NXTELMNT, -1);
        ENUMT type;
        if (attribute <= MA_COLOR3 && attribute > MA_LISTEND) { 
            type = attribute - 2;
            start++; 
        }
        len = msg - start;
        STRVAL val = STRVALObj(start, ++len);
        seekto(MA_NXTELMNT, 1);

        TYPEOBJECT obj = TYPEObj(NULL, DT_NULL);
        switch (type) {
            case DT_LIST: {
                start = msg;
                msg = val.p;
                /*seekto(MA_LISTEND, 0);                      ======== TODO: recursive parse ==========
                size_t listsize = msg - start;
                msg++;
                HASHLIST *listobj = MSGEncode(val.p, listsize + 2 + LIST_EXTRA_CLOSE_BYTES);
                obj = TYPEObj(listobj, DT_LIST);*/ 

                findcloser(MA_LIST, MA_LISTEND);
                size_t listlen = msg - val.p;
                msg++;
                char *listobj = ARRAlloc(char, listlen+1);
                memcpy(listobj, val.p, listlen);
                listobj[listlen] = Sf;
                obj = TYPEObj(listobj, DT_LIST);
            }
            break;
            case DT_STRING: {
                STRVAL *valobj = ARRAlloc(STRVAL, 1);
                *valobj = val;
                obj = TYPEObj(valobj, DT_STRING);
            }
            break;
            case DT_NUMBER: {
                char *strend = val.p + val.len + 1;
                double *numobj = ARRAlloc(double, 1);
                *numobj = strtod(val.p, &strend);
                obj = TYPEObj(numobj, DT_NUMBER);
            }
            break;
            /*case DT_VECTOR3: {
                double *vectorobj = ARRAlloc(double, 3);
                char *xend = start + 8;  
                vectorobj[0] = strtod(start, &xend);
                char *yend = start + 16; 
                vectorobj[1] = strtod(xend, &yend);
                char *zend = start + 24; 
                vectorobj[2] = strtod(yend, &zend);
                obj = TYPEObj(vectorobj, DT_VECTOR3);
            }
            break;
            case DT_COLOR3: {
                char *vectorobj = ARRAlloc(char, 3);
                char *rend = start + 3; 
                vectorobj[0] = cast(strtod(start, &rend), uint8_t);
                char *gend = start + 6; 
                vectorobj[1] = cast(strtod(rend, &gend), uint8_t);
                char *bend = start + 9; 
                vectorobj[2] = cast(strtod(gend, &bend), uint8_t);
                obj = TYPEObj(vectorobj, DT_COLOR3);
            }
            break;*/
        }

        HashSetVal(list, key, obj);
        if (*msg == MA_LISTEND) break; // stop parse if list end
    }
    #undef seekto

    return list;
}

char *MSGDecode(HASHLIST *list, STRVAL *keys, size_t keysc) {
    TBuff(char, RQST_LEN) buff = {0};
    char *cur = buff.buff;

    #define blen buff.len
    #define addc(c) *cur = c; blen++; cur++

    for (size_t i = 0; i < keysc; i++) {
        STRVAL key = keys[i];
        HASHSTRVAL *val = HashGet(list, key);
        if (val == NULL) continue; 
        memcpy(cur, key.p, key.len);
        cur += key.len;
        blen += key.len;
        addc('=');
        TYPEOBJECT obj = val->obj;
        switch (obj.type) {
            case DT_STRING: {
                addc(MA_STRING); 
                STRVAL *str = cast(obj.data, STRVAL*);
                size_t len = str->len;
                memcpy(cur, str->p, len);
                cur += len;
                blen += len;
            }
            break;
            case DT_LIST: {
                addc(MA_LIST);
                char *liststr = cast(obj.data, char*);
                size_t listlen = strlen(liststr);
                memcpy(cur, liststr, listlen);
                cur += listlen;
                blen += listlen;
            }
            break;
        }
        addc(MA_NXTELMNT);
    }
    addc(MA_LISTEND);

    char *output = ARRAlloc(char, blen + 1);
    memcpy(output, buff.buff, blen);
    output[blen] = Sf;
    #undef len
    #undef addc
    return output;
}

InlineApi void RQSTSetBodyVal(RQSTBODY *body, ValueSetterArgs) {
    if (isname("Host"))
        body->host = ValueSTR;
    else if (isname("Accept"))
        body->accept = ValueSTR;
    else if (isname("Accept-Encoding"))
        body->encoding = ValueSTR;
    else if (isname("Cache-Control"))
        body->cache = ValueSTR;
    else if (isname("Connection"))
        body->connection = ValueSTR;
    else if (isname("User-Agent"))
        body->useragent = ValueSTR;
    else if (isname("Content-Type")) {
        if (isval("text/plain"))
            body->contenttype = CT_TEXT;
        else if (isval("text/xml"))
            body->contenttype = CT_XML;
        else if (isval("application/xml"))
            body->contenttype = CT_APP_XML;
        else if (isval("application/json"))
            body->contenttype = CT_JSON;
        else if (isval("application/x-www-form-urlencoded"))
            body->contenttype = CT_URL;
        else
            body->type = CT_UNDEF;
    }
    else if (isname("Roblox-Id"))
        sscanf(val, "%d", &(body->robloxid));
    else if (isname("Content-Length"))
        sscanf(val, "%hd", &(body->msglen));
    else if (isname("Request-Type")) {
        if (isval("GET"))
            body->type = RT_GET;
        else if (isval("POST"))
            body->type = RT_POST;
        else
            body->type = RT_UNDEF;
    }
}

void RQSTParse(RQSTDAT *rqst) {
    char *buff = rqst->buff;

    RQSTBODY *body = rqst->body; 
    #define seekto(c, o) (buff = strchr(buff, c)+o)

    char *start;
    {
        start = buff;
        seekto('/', -2); // seek to request type end
        uint16_t len = buff - start;
        seekto(Cr, 2); // skip header

        RQSTSetBodyVal(body, "Request-Type", start, sizeof("Request-Type"), ++len);

option_read:
        // request footer check
        if (body->type == RT_GET && *buff == Cr) {
            goto parse_end;
        }
        else if (body->type == RT_POST && *buff == Cr) {
            body->msg = buff + 2;
            goto parse_end;
        }

        start = buff;
        seekto(':', -1); // seek to name end
        uint16_t nlen = buff - start;
        char *hname = start;
        seekto(':', 2); // seek to value start
        start = buff;
        seekto(Cr, -1);
        uint16_t vlen = buff - start;
        seekto(Cr, 2); // seek to name start

        RQSTSetBodyVal(body, hname, start, ++nlen, ++vlen);
        goto option_read;
    }

parse_end:

    #undef seekto
}