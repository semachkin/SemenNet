#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "srvdata.h"

static time_t __server_start_time;
static CONFIG configs;

void WSAfatalerr() {
    printwarn2("WSA Error: %d", WSAGetLastError());
    WSACleanup();
    exit(EXIT_FAILURE);
}

STATUSC RQSTInit(RQSTDAT *dat);
STATUSC RPNSSend(RQSTDAT *rqst, SOCKET sock);
void MSGStartAction(HASHLIST *list, char *package);

// ROBLOX CLIENTS
static TBuff(RBXCLIENT, DEFAULT_LOG_LEN) RBXClients = {0};

size_t RBXClientFind(STRVAL token);
void RBXClientsVerify();
void CNFGInit(CONFIG *cnfg);

int main(void) {
    WSADATA wsadat;
    WSAAssert(WSAStartup(WINSOCK_VERSION, &wsadat), ==SOCKET_ERROR);

    CNFGInit(&configs);

    SOCKADDR_IN serverdat;
    SOCKDInit(serverdat, configs.srvport);
    
    SOCKET serversock;
    uint8_t socklen;
    
    socklen = sizeof(SOCKADDR_IN);
    WSAAssert(serversock = SOCKInit(serverdat.sin_family, SOCK_STREAM, DEFAULT_PROTOCOL), ==INVALID_SOCKET);

    __server_start_time = time(0);

    // sockets options
    BOOL reuse_val = TRUE;
    DWORD rcvtimeo_val = 1000;
    TIMEVAL seltimeo = {0, 100};

    WSAAssert(SOCKOpt(serversock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse_val, sizeof(BOOL)), ==SOCKET_ERROR);
    WSAAssert(SOCKBind(serversock, (const PSOCKADDR)&serverdat, socklen), ==SOCKET_ERROR);

    printinfo3("Server socket as %s:%d", SINIP(serverdat.sin_addr), SINPORT(serverdat.sin_port));

    WSAAssert(SOCKList(serversock, DEFAULT_LOG_LEN), ==SOCKET_ERROR);

    CLIENTSDAT clientsdat;
    CLIENTSLIST clientslist = {0};
    RQSTDAT temprqstdat = {0};

    temprqstdat.buff = ARRAlloc(char, RQST_LEN);

    #define lcur clientslist.cur
    #define lbuff clientslist.buff
    #define llen clientslist.len

    life {
        SOCKADDR_IN tempdat;
        FDClean(clientsdat);
        uint16_t i;

        int16_t selres;
        WSAAssert(selres = SOCKSel(0, &clientsdat, NULL, NULL, &seltimeo), ==SOCKET_ERROR);
        if (selres > 0) {
            WSAAssert(lcur = SOCKAccept(serversock, (const PSOCKADDR)&tempdat, (int*)&socklen), ==INVALID_SOCKET || (lcur ==SOCKET_ERROR));
            //printinfo3("Connected to %s:%d", SINIP(tempdat.sin_addr), SINPORT(tempdat.sin_port));

            if (llen < DEFAULT_LOG_LEN) {
                lbuff[llen] = lcur;
                llen++;
            }
            else {
                printwarn1("Server full.. request rejected\n");
                temprqstdat.err = STATUS_server_full;
                WSAAssert(RPNSSend(&temprqstdat, lcur), ==STATUS_send_error);
            }
        }

        RBXClientsVerify(); // clients time check

        for (i = 0; i < llen; i++) {
            lcur = lbuff[i];
            FD_SET(lcur, &clientsdat);
        }

        for (i = 0; i < llen; i++) {
            lcur = lbuff[i];
            if (!FD_ISSET(lcur, &clientsdat)) { 
                printwarn2("Client %d not activity.. disconnecting\n", i);
                goto remove_sock; 
            }

            WSAAssert(SOCKOpt(lcur, SOL_SOCKET, SO_RCVTIMEO, (char*)&rcvtimeo_val, sizeof(DWORD)), ==SOCKET_ERROR);

            temprqstdat.buff = memset(temprqstdat.buff, 0, RQST_LEN);
            temprqstdat.bufflen = (uint16_t)SOCKRecv(lcur, temprqstdat.buff, RQST_LEN, RECV_FLAGS);
            WSAAssertO(temprqstdat.bufflen, ==SOCKET_ERROR, WSAETIMEDOUT, goto remove_sock;);
            
            if (temprqstdat.bufflen == RQST_LEN) {
                printwarn2("Buffer overflow\nNotice: request size more than %d", RQST_LEN);
                goto remove_sock;
            }
            else if (temprqstdat.bufflen > 0) {
                RQSTDInit(temprqstdat);
                STATUSC sentres = RPNSSend(&temprqstdat, lcur);

                if (sentres > STATUS_success) goto remove_sock;
                continue;
            }
            else 
                goto remove_sock;
            continue;

remove_sock:
            WSAAssert(SOCKGetName(lcur, (const PSOCKADDR)&tempdat, (int*)&socklen), ==SOCKET_ERROR);
            //printinfo3("Disconnect at %s:%d", SINIP(tempdat.sin_addr), SINPORT(tempdat.sin_port));
            SOCKClose(lcur);
            lbuff[i] = 0;
            LISTAlign(clientslist);
            llen--;
            i--;
        }
    }

    #undef stk_ptr
    #undef stk_end

    WSACleanup();
}

STATUSC RQSTInit(RQSTDAT *dat) {
    RQSTBODY *body = ARRAlloc(RQSTBODY, 1);
    dat->body = body;

    RQSTParse(dat);
    #define is(a, b) (memcmp(a, b, sizeof(b)-1) == 0)

    if (!is(body->useragent.p, "RobloxStudio")) {
        MEMFree(body);
        return STATUS_invalid_agent;
    }

    #undef is
    return STATUS_success;
}

STATUSC RPNSSend(RQSTDAT *rqst, SOCKET sock) {
    char package[RQST_LEN];
    char *ppackage = package;
    
    switch (rqst->err) {
        case STATUS_invalid_agent: {
            HTTPRESPONSE(package, ERR_INVALID_AGENT);
            goto send_start;
        };
        case STATUS_server_full: {
            HTTPRESPONSE(package, ERR_SERVER_FULL);
            goto send_start;
        };
    }
    
    RQSTBODY *body = rqst->body;
    if (body->type == RT_POST) {
        HASHLIST *message = MSGEncode(body->msg, body->msglen);
        MSGStartAction(message, ppackage);
        HashListClean(message);
        free(message);
    }
    else if (body->type == RT_GET) {
        HTTPRESPONSE(package, OK_SUCCESS);
    }

    free(body);
send_start:
    
    size_t packagelen = strlen(ppackage);
    int32_t sentlen;

    while ((sentlen = SOCKSend(sock, ppackage, packagelen, SEND_FLAGS)) < packagelen) {
        if (sentlen < 1) { 
            SOCKClose(sock);
            break; 
        }
    }

    switch (sentlen) {
        case SOCKET_ERROR: {
            printwarn1("Error sending data, reconnect");
            return STATUS_send_error;
        };
        case 0: {
            printwarn1("Client closed connection");
            return STATUS_client_close;
        }
    }

    return STATUS_success;
}

void MSGStartAction(HASHLIST *list, char *package) {
    // indexing message type

    HASHSTRVAL *typeobj; 
    HashIndexing(list, "MT", typeobj);
    if (typeobj == NULL) goto incorrect_message;
    char *typestr = cast(typeobj->obj.data, STRVAL*)->p;

    // indexing token
    HASHSTRVAL *tokenobj; 
    HashIndexing(list, "TOKEN", tokenobj);
    if (tokenobj == NULL) goto incorrect_message;
    STRVAL *tokenstr = cast(tokenobj->obj.data, STRVAL*);

    #define istype(s) (memcmp(typestr, s, sizeof(s)-1) == 0)

    if (istype("LOGIN")) {
        // indexing name
        HASHSTRVAL *nameobj; HashIndexing(list, "NAME", nameobj);
        if (nameobj == NULL) goto incorrect_message;
        STRVAL *namestr = cast(nameobj->obj.data, STRVAL*);

        size_t idx = RBXClientFind(*tokenstr);
        if (idx != INVALID_TOKEN) {
            printinfo2("Rbx Client %lld Re authorization attempt", idx);
            goto login_end;
        }
        char *clienttoken; STRPrint(clienttoken, (*tokenstr));
        char *clientname; STRPrint(clientname, (*namestr));
        RBXCLIENT client = {0};
        client.token = clienttoken;
        client.name = clientname;
        RBXClientAdd(client);

        printinfo2("Rbx Client %s login to server", clientname);
        HTTPRESPONSE(package, OK_SUCCESS);
    }

login_end:
    size_t RBXClientIdx = RBXClientFind(*tokenstr);
    if (RBXClientIdx == INVALID_TOKEN) {
        printwarn1("Unathorized request attempt");
        HTTPRESPONSE(package, ERR_INVALID_CLIENT);
        return;
    }
    RBXCLIENT *client = RBXClients.buff + RBXClientIdx;
    client->lrqst = time(0); // update last request time

    if (istype("DATAEX")) {
        // indexing instances
        HASHSTRVAL *instancesobj; HashIndexing(list, "INSTANCES", instancesobj);
        if (instancesobj == NULL) goto incorrect_message;
        STRVAL *instancesstr = cast(instancesobj->obj.data, STRVAL*);

        MEMFree(client->tempdat.instances);

        char *instancesdump; STRPrint(instancesdump, (*instancesstr));
        client->tempdat.instances = instancesdump;

        // indexing messages
        HASHSTRVAL *messagesobj; HashIndexing(list, "MESSAGES", messagesobj);
        if (messagesobj != NULL) {
            STRVAL *messagesstr = cast(messagesobj->obj.data, STRVAL*);
            MEMFree(client->tempdat.messages);

            char *messagesdump; STRPrint(messagesdump, (*messagesstr));
            client->tempdat.messages = messagesdump;
        }
        
        HASHLIST responsedat = {0};
        TBuff(STRVAL, DEFAULT_LOG_LEN) namekeys = {0};
        HashListRealloc(&responsedat, HASHLIST_STARTSIZE);
        
        for (size_t i = 0; i < DEFAULT_LOG_LEN; i++) {
            RBXCLIENT curclient = RBXClients.buff[i];
            STRVAL name = STRVALObj(curclient.name, 0);
            if (name.p == NULL) continue;
            if (strcmp(curclient.token, client->token) == 0) continue;

            RBXCLIENTDAT tempdat = curclient.tempdat;
            if (tempdat.instances == NULL) continue;
            name.len = strlen(name.p);
            HASHLIST clientdat = {0};
            HashListRealloc(&clientdat, HASHLIST_STARTSIZE);
            STRVAL keys[2] = {STRConst("INSTANCES"), STRConst("MESSAGES")};

            STRVAL instanceskey = keys[0];
            STRVAL messageskey = keys[1];

            STRVAL *instances = ARRAlloc(STRVAL, 1);
            instances->p = tempdat.instances;
            instances->len = strlen(tempdat.instances);
            TYPEOBJECT instancesobj = TYPEObj(instances, DT_STRING);
            HashSetVal(&clientdat, instanceskey, instancesobj);

            TYPEOBJECT messagestobj = TYPEObj(NULL, DT_NULL);
            if (tempdat.messages != NULL) {
                STRVAL *messages = ARRAlloc(STRVAL, 1);
                messages->p = tempdat.messages;
                messages->len = strlen(tempdat.messages)+1;
                messagestobj = TYPEObj(messages, DT_STRING);
            }
            HashSetVal(&clientdat, messageskey, messagestobj);

            char *msgform = MSGDecode(&clientdat, keys, sizeof(keys)/sizeof(STRVAL));
            TYPEOBJECT msgobj = TYPEObj(msgform, DT_LIST);
            HashSetVal(&responsedat, name, msgobj);
            namekeys.buff[namekeys.len] = name;
            namekeys.len++;
            HashListClean(&clientdat);
        }
        char *msgpackage = MSGDecode(&responsedat, namekeys.buff, namekeys.len);
        HashListClean(&responsedat);
        HTTPRESPONSE(package, msgpackage);
        free(msgpackage);
    }

    #undef istype
    return;

incorrect_message:
    HTTPRESPONSE(package, ERR_INCORRECT);
}

void RBXClientsVerify() {
    time_t curtime = time(0);
    for (size_t i = 0; i < DEFAULT_LOG_LEN; i++) {
        RBXCLIENT client = RBXClients.buff[i];

        if (client.token == NULL) continue;
        uint16_t diff = (uint16_t)difftime(curtime, client.lrqst);

        if (diff > MAX_TIME_WITHOUT_REQUESTS) {
            printinfo2("Rbx Client %s disconnected", client.name);
            RBXClientClean(client);
            RBXClients.buff[i] = (RBXCLIENT){0};
        }
    }
}

inline size_t RBXClientFind(STRVAL token) {
    for (size_t i = 0; i < DEFAULT_LOG_LEN; i++) {
        char *clienttkn = RBXClients.buff[i].token;
        if (clienttkn == NULL) continue;
        if (memcmp(token.p, clienttkn, token.len) == 0)
            return i;
    }
    return INVALID_TOKEN;
}

void CNFGInit(CONFIG *cnfg) {
    LPCSTR inifile = CONFIG_FILE;

    INIReadInt(cnfg->srvport, DEFAULT_PORT, "server", "port", inifile);

    INIReadString(cnfg->logf, DEFAULT_LOGFILE, "logs", "log_file", inifile);
    INIReadInt(cnfg->loglvl, DEFAULT_LOGLVL, "logs", "log_level", inifile);
}