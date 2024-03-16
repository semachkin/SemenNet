#ifndef SERVER_H
#define SERVER_H

#define SERVER_PORT 1917

#pragma region constants

#define Cr '\r'
#define Sf '\0'

#define DEFAULT_PROTOCOL 0
#define DEFAULT_LOG_LEN 15

#define MAX_TIME_WITHOUT_REQUESTS 5

#define INVALID_TOKEN ((~(size_t)0)-2)
#define MAX_HASHLIST_SIZE ((~(uint16_t)0)-2)

#define HASHLIST_STARTSIZE 2

#define LIST_EXTRA_CLOSE_BYTES 10*2

#define FD_SETSIZE DEFAULT_LOG_LEN+1

#define RQST_LEN 4096

#define RECV_FLAGS 0
#define SEND_FLAGS MSG_DONTROUTE

#define STATUS_success       0x1000
#define STATUS_invalid_agent 0x1001
#define STATUS_send_error    0x1002
#define STATUS_client_close  0x1003
#define STATUS_server_full   0x1004

#pragma endregion

#include <winsock2.h>

#pragma region types

#define TBuff(t, l) struct {t buff[l]; size_t len;}

typedef unsigned char ENUMT;
typedef unsigned short STATUSC;
typedef fd_set CLIENTSDAT;

typedef struct STRVAL_S {
    char *p; 
    size_t len;
} STRVAL;

typedef struct TYPEOBJECT_S {
    void *data;
    ENUMT type;
} TYPEOBJECT;

typedef struct RBXCLIENT_S {
    char *token;
    char *name;
    char *tempdat;
    time_t lrqst;
} RBXCLIENT;

typedef struct CLIENTSLIST_S {
    SOCKET buff[DEFAULT_LOG_LEN];
    SOCKET cur;
    uint16_t len;
} CLIENTSLIST;

typedef struct RQSTBODY_S {
    ENUMT type;
    ENUMT contenttype;
    uint16_t msglen;
    uint32_t robloxid;
    char *msg;
    STRVAL host;
    STRVAL accept;
    STRVAL encoding;
    STRVAL cache;
    STRVAL connection;
    STRVAL useragent;
} RQSTBODY;

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
 
typedef struct RQSTDAT_S {
    char *buff;
    int16_t bufflen;
    STATUSC err;
    RQSTBODY *body;
} RQSTDAT;

enum rqsttype {
    RT_UNDEF,
    RT_GET,
    RT_POST
};
enum contenttype {
    CT_UNDEF,
    CT_TEXT,
    CT_XML,
    CT_JSON,
    CT_APP_XML,
    CT_URL
};
enum msgattributes {
    MA_UNDEF,
    MA_NXTELMNT,
    MA_LISTEND,
    MA_LIST,
    MA_STRING,
    MA_NUMBER,
    MA_VECTOR3,
    MA_COLOR3
};
enum datatype {
    DT_NULL,
    DT_LIST,
    DT_STRING,
    DT_NUMBER,
    DT_VECTOR3,
    DT_COLOR3
};

#pragma endregion

#pragma region macros

#define printloghead time_t df = difftime(time(0), __server_start_time)
#define printdifftime (df/60), (df%60)
#define printlogform "========== at %02lld:%02lld ==========\n"

#define printlog1(f) { printloghead; printf(printlogform f "\n", printdifftime); }
#define printlog2(f, a0) { printloghead; printf(printlogform f "\n", printdifftime, a0); }
#define printlog3(f, a0, a1) { printloghead; printf(printlogform f "\n", printdifftime, a0, a1); }

#define cast(a, t) ((t)(a))

#define MEMFree(p) \
    if (p != NULL) free(p)

#define RBXClientAdd(c) \
    for (size_t i = 0; i < DEFAULT_LOG_LEN; i++) \
        if (RBXClients.buff[i].token == NULL) \
            { RBXClients.buff[i] = c; RBXClients.len++; break; }

#define RBXClientClean(c) \
    MEMFree(c.name); \
    MEMFree(c.token); \
    MEMFree(c.tempdat);

#define SOCKDInit(d) \
    d.sin_family = AF_INET; \
    d.sin_addr.s_addr = INADDR_ANY; \
    d.sin_port = SINPORT(SERVER_PORT)

#define FDClean(d) \
    FD_ZERO(&d); \
    FD_SET(serversock, &d)

#define LISTAlign(l) { \
        for (uint16_t o = i; o < (DEFAULT_LOG_LEN-1); o++) { \
            l.buff[o] = l.buff[o + 1]; \
        } \
        l.buff[DEFAULT_LOG_LEN-1] = cast(0, SOCKET); \
    }

#define STRPrint(pt, o) \
    { pt = malloc(o.len+1); memcpy(pt, o.p, o.len); pt[o.len] = Sf; }

#define HashIndexing(l, s, p) \
    { STRVAL key = STRVALObj(s, strlen(s)); p = HashGet(l, key); }

#define RQSTDInit(d) (d.err = RQSTInit(&d))

#define ARRAlloc(t, s) (cast(malloc(s*sizeof(t)), t*))

#define SINPORT htons
#define SINIP inet_ntoa

#define SOCKInit socket
#define SOCKBind bind
#define SOCKList listen
#define SOCKAccept accept
#define SOCKRecv recv
#define SOCKSend send
#define SOCKClose closesocket
#define SOCKOpt setsockopt
#define SOCKSel select
#define SOCKGetName getpeername

#define WSAAssert(a, b) if ((a) b) WSAfatalerr()
#define WSAAssertO(a, b, e, c) if ((a) b) { if (WSAGetLastError() == e) { c } else WSAfatalerr(); }

#define STRVALObj(p, l) ((STRVAL){p, l})
#define TYPEObj(d, t) ((TYPEOBJECT){d, t})

#define life for (;;)

#define InlineApi static inline

#pragma endregion

#pragma region strings

#define HTTPRESPONSE(s, m) (sprintf(s, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %lld\r\n\r\n%s", strlen(m), m))

#define ERR_INVALID_AGENT "Error 400 Invalid user agent"
#define ERR_INCORRECT "Error 400 Incorrect message signature"
#define ERR_SERVER_FULL "Error 400 Server full"
#define ERR_INVALID_CLIENT "Error 401 Unauthorized"

#define OK_SUCCESS "SUCCESS"

#pragma endregion

void RQSTParse(RQSTDAT *rqst);
HASHLIST *MSGEncode(char *msg, size_t msglen);

HASHSTRVAL *HashGet(HASHLIST *list, STRVAL key);
void HashSetVal(HASHLIST *list, STRVAL key, TYPEOBJECT val);
void HashListRealloc(HASHLIST *list, uint16_t newsize);
void HashListClean(HASHLIST *list);

#endif