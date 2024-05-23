#ifndef SERVER_H
#define SERVER_H

#define CONFIG_FILE "./config.ini"

#pragma region constants

#define Cr '\r'
#define Sf '\0'

#define DEFAULT_PROTOCOL 0
#define DEFAULT_LOG_LEN 2

#define DEFAULT_PORT 1917
#define DEFAULT_LOGLVL 5
#define DEFAULT_LOGFILE "console.log"

#define MAX_TIME_WITHOUT_REQUESTS 3

#define RQST_LEN 10240

#define MAX_CLIENT_NAME_LEN 20
#define MAX_DECODED_CLIENT_DATA_LEN (RQST_LEN/DEFAULT_LOG_LEN - MAX_CLIENT_NAME_LEN - 4 - 64)
#define MAX_MESSAGE_LEN 255
#define MAX_DECODED_MESSAGES_LEN (MAX_MESSAGE_LEN + 4)
#define MAX_DECODED_MESSAGES_INDEX_LEN (MAX_DECODED_MESSAGES_LEN + 14)
#define MAX_DECODED_INSTANCES_LEN (MAX_DECODED_CLIENT_DATA_LEN - MAX_DECODED_MESSAGES_INDEX_LEN - 16)

#define LIST_EXTRA_CLOSE_BYTES 10*2

#define FD_SETSIZE DEFAULT_LOG_LEN+1

#define RECV_FLAGS 0
#define SEND_FLAGS MSG_DONTROUTE

#define STATUS_success       0x1000
#define STATUS_invalid_agent 0x1001
#define STATUS_send_error    0x1002
#define STATUS_client_close  0x1003
#define STATUS_server_full   0x1004

#pragma endregion

#include <winsock2.h>
#include <windows.h>
#include "srvhashlist.h"

#pragma region types

#define TBuff(t, l) struct {t buff[l]; size_t len;}

typedef FILE *filep;
typedef unsigned short STATUSC;
typedef fd_set CLIENTSDAT;

typedef struct CONFIG_S {
    uint32_t srvport;
    ENUMT loglvl;
    char logf[MAX_PATH];
} CONFIG;

typedef struct RBXCLIENTDAT_S {
    char *validinstances;
    char *instances;
    char *messages;
    uint8_t nf;
    uint8_t vnf;
} RBXCLIENTDAT;

typedef struct RBXCLIENT_S {
    char *token;
    char *name;
    RBXCLIENTDAT tempdat;
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

#define printhead filep lf = fopen(configs.logf, "a+"); time_t df = difftime(time(0), __server_start_time);
#define printdifftime (df/60), (df%60)
#define printinfocond (configs.loglvl > 0)
#define printwarncond (configs.loglvl > 1)
#define printinfoform "========== INFO at %02lld:%02lld ==========\n"
#define printwarnform "========== WARNING at %02lld:%02lld ==========\n"
#define printfoot fclose(lf);

#define printinfo1(f) if printinfocond { printhead fprintf(lf, printinfoform f "\n", printdifftime); printf(f "\n"); printfoot }
#define printinfo2(f, a0) if printinfocond { printhead fprintf(lf, printinfoform f "\n", printdifftime, a0); printf(f "\n", a0); printfoot }
#define printinfo3(f, a0, a1) if printinfocond { printhead fprintf(lf, printinfoform f "\n", printdifftime, a0, a1); printf(f "\n", a0, a1); printfoot }

#define printwarn1(f) if printwarncond { printhead fprintf(lf, printwarnform f "\n", printdifftime); printf(f "\n"); printfoot }
#define printwarn2(f, a0) if printwarncond { printhead fprintf(lf, printwarnform f "\n", printdifftime, a0); printf(f "\n", a0); printfoot }
#define printwarn3(f, a0, a1) if printwarncond { printhead fprintf(lf, printwarnform f "\n", printdifftime, a0, a1); printf(f "\n", a0, a1); printfoot }

#define cast(a, t) ((t)(a))

#define MEMFree(p) \
    if (p != NULL) free(p); \
    p = NULL;

#define RBXClientAdd(c) \
    for (size_t i = 0; i < DEFAULT_LOG_LEN; i++) \
        if (RBXClients.buff[i].token == NULL) \
            { RBXClients.buff[i] = c; RBXClients.len++; break; }

#define RBXClientCleanTempdat(d) \
    MEMFree(d.instances); \
    MEMFree(d.messages)

#define RBXClientClean(c) \
    free(c.name); \
    free(c.token); \
    RBXClientCleanTempdat(c.tempdat)

#define SOCKDInit(d, port) \
    d.sin_family = AF_INET; \
    d.sin_addr.s_addr = INADDR_ANY; \
    d.sin_port = SINPORT(port)

#define FDClean(d) \
    FD_ZERO(&d); \
    FD_SET(serversock, &d)

#define LISTAlign(l) { \
        for (uint16_t o = i; o < (DEFAULT_LOG_LEN-1); o++) { \
            l.buff[o] = l.buff[o + 1]; \
        } \
        l.buff[DEFAULT_LOG_LEN-1] = cast(0, SOCKET); \
    }

#define ARRAlloc(t, s) (cast(malloc(s*sizeof(t)), t*))
#define ARRRealloc(b, t, s) ((b) = cast(realloc(b, s*sizeof(t)), t*))

#define STRPrint(pt, o) \
    { pt = ARRAlloc(char, o.len+1); memcpy(pt, o.p, o.len); pt[o.len] = Sf; }

#define MAX_NUM(t) ((~(t)0)-2)

#define RQSTDInit(d) (d.err = RQSTInit(&d))

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

#define INIReadInt(v, d, a, k, f) (v = GetPrivateProfileIntA(a, k, d, f))
#define INIReadString(v, d, a, k, f) (GetPrivateProfileStringA(a, k, d, v, sizeof(v), f))

#define WSAAssert(a, b) if ((a) b) WSAfatalerr()
#define WSAAssertO(a, b, e, c) if ((a) b) { if (WSAGetLastError() == e) { c } else WSAfatalerr(); }

#define STRVALObj(p, l) ((STRVAL){p, l})
#define TYPEObj(d, t) ((TYPEOBJECT){d, t})

#define STRConst(s) (STRVALObj(s, sizeof(s)-1))

#define INVALID_TOKEN MAX_NUM(size_t)
#define MAX_HASHLIST_SIZE MAX_NUM(uint16_t)

#define life for (;;)

#define InlineApi static inline

#pragma endregion

#pragma region strings

#define SUCCESS_OK "200 OK"
#define SUCCESS_CREATED "201 Created"
#define SUCCESS_NO_CONTENT "204 No Content"
#define ERR_BAD_REQUEST "400 Bad Request"
#define ERR_UNAUTHORIZED "401 Unauthorized"
#define ERR_FORBIDDEN "403 Forbidden"
#define ERR_NOT_FOUND "404 Not Found"
#define ERR_INTERNAL_SERVER_ERR "500 Internal Server Error"
#define ERR_NOT_IMPLEMENTED "501 Not Implemented"
#define ERR_BAD_GATEWAY "502 Bad Gateway"
#define ERR_SERVICE_UNAVAILABLE "503 Service unavailable"
#define ERR_TIMEOUT "504 Gateway Timeout"

#define HTTPRESPONSE(s, b) (sprintf(b, "HTTP/1.1 %s\r\n", s))
#define HTTPRESPONSE_CONTENT(s, b, m) (sprintf(b, "HTTP/1.1 %s\r\nContent-Type: text/plain\r\nContent-Length: %lld\r\n\r\n%s", s, strlen(m), m))
#define HTTPRESPONSE_OK(b, m) HTTPRESPONSE_CONTENT(SUCCESS_OK, b, m)

#pragma endregion

#pragma region declaractions

void RQSTParse(RQSTDAT *rqst);
HASHLIST *MSGEncode(char *msg, size_t msglen);
char *MSGDecode(HASHLIST *list, STRVAL *keys, size_t keysc);

#pragma endregion

#endif
