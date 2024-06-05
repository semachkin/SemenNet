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
static BOOL RBXRequestsRefreshInstances = 0;

// HISTORY
static HASHLIST DATAEXClientsHistory = {0};

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

    HashListRealloc(&DATAEXClientsHistory, HASHLIST_STARTSIZE);

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

    HashListClean(&DATAEXClientsHistory);

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
            HTTPRESPONSE(ERR_FORBIDDEN, package);
            goto send_start;
        };
        case STATUS_server_full: {
            HTTPRESPONSE(ERR_SERVICE_UNAVAILABLE, package);
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
        HTTPRESPONSE(SUCCESS_NO_CONTENT, package);
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
    HASHSTRVAL *typeobj; HashIndexing(list, "MT", typeobj);
    if (typeobj == NULL) goto incorrect_message;
    char *typestr = cast(typeobj->obj.data, STRVAL*)->p;

    // indexing token
    HASHSTRVAL *tokenobj; HashIndexing(list, "TOKEN", tokenobj);
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

        if (RBXClients.len >= DEFAULT_LOG_LEN) {
            printinfo2("Rbx Client %s login failed. Server full", clientname);
            HTTPRESPONSE(ERR_SERVICE_UNAVAILABLE, package);
            return;
        }
        RBXCLIENT client = {0};
        client.token = clienttoken;
        client.name = clientname;
        RBXClientAdd(client);

        HASHLIST *DATAEXHistory = ARRAlloc(HASHLIST, 1);
        memset(DATAEXHistory, 0, sizeof(HASHLIST));
        HashListRealloc(DATAEXHistory, HASHLIST_STARTSIZE);
        TYPEOBJECT DATAEXHistoryObj = TYPEObj(DATAEXHistory, DT_LIST);
        HashSetVal(&DATAEXClientsHistory, *namestr, DATAEXHistoryObj);

        printinfo2("Rbx Client %s login to server", clientname);
        HTTPRESPONSE(SUCCESS_CREATED, package);
    }

login_end:
    size_t RBXClientIdx = RBXClientFind(*tokenstr);
    if (RBXClientIdx == INVALID_TOKEN) {
        printwarn1("Unathorized request attempt");
        HTTPRESPONSE(ERR_UNAUTHORIZED, package);
        return;
    }
    RBXCLIENT *client = RBXClients.buff + RBXClientIdx;
    client->lrqst = time(0); // update last request time

    RBXCLIENTDAT *rbxtempdat = &client->tempdat;

    if (istype("DATAEX")) {
        HASHSTRVAL *nfobj; HashIndexing(list, "NF", nfobj);
        if (nfobj == NULL) goto incorrect_message;
        double *nfdoubleptr = cast(nfobj->obj.data, double*);
        uint8_t nfboolean = cast(*nfdoubleptr, uint8_t);

		HASHSTRVAL *reqisvi; HashIndexing(list, "ISVI", reqisvi);
		if (reqisvi == NULL) goto incorrect_message;
		double *reqisvidoubleptr = cast(reqisvi->obj.data, double*);
		uint8_t reqisviboolean = cast(*reqisvidoubleptr, uint8_t);

        HASHSTRVAL *instancesobj; HashIndexing(list, "INSTANCES", instancesobj);
        if (instancesobj == NULL) goto incorrect_message;
        STRVAL *instancesstr = cast(instancesobj->obj.data, STRVAL*);

        char *lastinstances = reqisviboolean ? rbxtempdat->validinstances : rbxtempdat->instances;
        size_t lastinstancessize = 0;

        if ((reqisviboolean && rbxtempdat->vnf) || (!reqisviboolean && rbxtempdat->nf)) {
            lastinstancessize = strlen(lastinstances);
        }
        size_t instancesstrsize = instancesstr->len + lastinstancessize;
        
        char *instancesreallcd = ARRAlloc(char, instancesstrsize + 1);
        if (reqisviboolean) {
            rbxtempdat->validinstances = instancesreallcd;
            rbxtempdat->vnf = nfboolean;
        } else {
            rbxtempdat->instances = instancesreallcd;
            rbxtempdat->nf = nfboolean;
        }

        if (lastinstancessize) memcpy(instancesreallcd, lastinstances, lastinstancessize);
        memcpy(instancesreallcd + lastinstancessize, instancesstr->p, instancesstr->len);
        instancesreallcd[instancesstrsize] = Sf;

        MEMFree(lastinstances);

        HASHSTRVAL *messagesobj; HashIndexing(list, "MESSAGES", messagesobj);
        if (messagesobj != NULL) {
            STRVAL *messagesstr = cast(messagesobj->obj.data, STRVAL*);
            MEMFree(rbxtempdat->messages);

            char *messagesdump; STRPrint(messagesdump, (*messagesstr));
            rbxtempdat->messages = messagesdump;
        }
        
        HASHLIST clientsdat = {0};
        TBuff(STRVAL, DEFAULT_LOG_LEN) namekeys = {0};
        HashListRealloc(&clientsdat, HASHLIST_STARTSIZE);
        
        HASHSTRVAL *DATAEXHistoryVal; HashIndexing(&DATAEXClientsHistory, client->name, DATAEXHistoryVal);
        if (DATAEXHistoryVal == NULL) { 
            printwarn2("Client %s data exchange history is lost", client->name);
            HashListClean(&clientsdat);
            return; 
        }
        HASHLIST *DATAEXHistory = cast(DATAEXHistoryVal->obj.data, HASHLIST*);
        
        for (size_t i = 0; i < DEFAULT_LOG_LEN; i++) {
            RBXCLIENT curclient = RBXClients.buff[i];
            STRVAL name = STRVALObj(curclient.name, 0);
            if (name.p == NULL) continue;
            if (strcmp(curclient.token, client->token) == 0) continue; // TODO: comment me for tests

            RBXCLIENTDAT tempdat = curclient.tempdat;
            if (tempdat.instances == NULL) continue;
            
            name.len = strlen(name.p);
            HASHLIST clientdat = {0};
            HashListRealloc(&clientdat, HASHLIST_STARTSIZE);
            STRVAL keys[3] = {STRConst("INSTANCES"), STRConst("MESSAGES"), STRConst("NF")};
            
            STRVAL instanceskey = keys[0];
            STRVAL messageskey = keys[1];
            STRVAL notfinishedkey = keys[2];

            double notfinishedv = FALSE;

            HASHSTRVAL *instancesstackval; HashIndexing(DATAEXHistory, name.p, instancesstackval);
            TYPEOBJECT instancesobj = TYPEObj(NULL, DT_NULL);

            if (instancesstackval != NULL && instancesstackval->obj.data != NULL) {
                STRVAL *stackval = cast(instancesstackval->obj.data, STRVAL*);
                instancesobj = TYPEObj(stackval, DT_STRING);
                HashSetVal(DATAEXHistory, name, TYPEObj(NULL, DT_NULL));
            }
            else if (!tempdat.nf && !tempdat.vnf) {
                STRVAL *instances = ARRAlloc(STRVAL, 1);
                instances->p = tempdat.instances;
                instances->len = strlen(tempdat.instances);
                instancesobj = TYPEObj(instances, DT_STRING);
            }
            if (instancesobj.data != NULL) {
                STRVAL *instancesstrval = cast(instancesobj.data, STRVAL*);
                char *instancesstr = instancesstrval->p;
                size_t instanceslen = instancesstrval->len;
                if (instanceslen > MAX_DECODED_INSTANCES_LEN) {
                    notfinishedv = TRUE;
                    STRVAL *stackstr = ARRAlloc(STRVAL, 1);
                    stackstr->p = instancesstr + MAX_DECODED_INSTANCES_LEN;
                    stackstr->len = instanceslen - MAX_DECODED_INSTANCES_LEN;
                    HashSetVal(DATAEXHistory, name, TYPEObj(stackstr, DT_STRING));
                    instancesstrval->len = MAX_DECODED_INSTANCES_LEN;
                }
            }

            HashSetVal(&clientdat, instanceskey, instancesobj);
            
            if (tempdat.messages != NULL) {
                STRVAL *messages = ARRAlloc(STRVAL, 1);
                messages->p = tempdat.messages;
                messages->len = strlen(tempdat.messages)+1;
                TYPEOBJECT messagestobj = TYPEObj(messages, DT_STRING);
                HashSetVal(&clientdat, messageskey, messagestobj);
            }
            if (notfinishedv) {
                double *notfinished = ARRAlloc(double, 1);
                *notfinished = notfinishedv;
                TYPEOBJECT notfinishedobj = TYPEObj(notfinished, DT_NUMBER);
                HashSetVal(&clientdat, notfinishedkey, notfinishedobj);
            }

            char *msgform = MSGDecode(&clientdat, keys, sizeof(keys)/sizeof(STRVAL));
            if (msgform == NULL) {
                printwarn3("Buffer overflow\nNotice: client %s decoded data more than %d", name.p, RQST_LEN); 
                continue;
            }
            TYPEOBJECT msgobj = TYPEObj(msgform, DT_LIST);
            HashSetVal(&clientsdat, name, msgobj);
            namekeys.buff[namekeys.len] = name;
            namekeys.len++;
            HashListClean(&clientdat);
        }

        HASHLIST responsedat = {0};
        HashListRealloc(&responsedat, HASHLIST_STARTSIZE);
        STRVAL responsekeys[2] = {STRConst("CLIENTS"), STRConst("VIR")};

        STRVAL clientskey = responsekeys[0];
        STRVAL virkey = responsekeys[1];

		char *clientspackage = MSGDecode(&clientsdat, namekeys.buff, namekeys.len);
		HashListClean(&clientsdat);
		HashSetVal(&responsedat, clientskey, TYPEObj(clientspackage, DT_LIST));
        
        if ((client->tempdat.validinstances == NULL || client->tempdat.vnf) && RBXRequestsRefreshInstances) {
            double *clientvir = ARRAlloc(double, 1);
            *clientvir = cast(TRUE, double);
		    HashSetVal(&responsedat, virkey, TYPEObj(clientvir, DT_NUMBER));
        }
        
        char *msgpackage = MSGDecode(&responsedat, responsekeys, sizeof(responsekeys)/sizeof(STRVAL));
        HashListClean(&responsedat);
        if (msgpackage == NULL) { 
            printwarn2("Buffer overflow\nNotice: response size more than %d", RQST_LEN);
            HTTPRESPONSE(ERR_INTERNAL_SERVER_ERR, package);
            return;
        }
        HTTPRESPONSE_OK(package, msgpackage);
        free(msgpackage);
    }
    if (istype("REQINSTANCES")) {
        TBuff(STRVAL, DEFAULT_LOG_LEN) namekeys = {0};
        RBXCLIENT *clientsbuff[DEFAULT_LOG_LEN] = {0};

        HASHSTRVAL *DATAEXHistoryVal; HashIndexing(&DATAEXClientsHistory, client->name, DATAEXHistoryVal);
        HASHLIST *DATAEXHistory = cast(DATAEXHistoryVal->obj.data, HASHLIST*);

        for (size_t i = 0; i < DEFAULT_LOG_LEN; i++) {
            RBXCLIENT *clientaddr = RBXClients.buff + i;
            RBXCLIENT curclient = *clientaddr;
            if (curclient.name == NULL) continue;
            if (strcmp(curclient.token, client->token) == 0) continue;

            RBXCLIENTDAT tempdat = curclient.tempdat;
            if (tempdat.validinstances == NULL && !tempdat.vnf) {
                RBXRequestsRefreshInstances = TRUE;
                HTTPRESPONSE(SUCCESS_NO_CONTENT, package);
                return;
            }

            clientsbuff[namekeys.len] = clientaddr;
            namekeys.buff[namekeys.len] = STRVALObj(curclient.name, strlen(curclient.name));
            namekeys.len++;
        }
        for (size_t i = 0; i < namekeys.len; i++) {
            RBXCLIENT *curclient = clientsbuff[i];
            STRVAL clientname = namekeys.buff[i];
            char *validinstances = curclient->tempdat.validinstances;

            size_t instancessize = strlen(validinstances);
            char *instancesclone = ARRAlloc(char, instancessize+1);
            memcpy(instancesclone, validinstances, instancessize);
            instancesclone[instancessize] = Sf;

            STRVAL *stackstr = ARRAlloc(STRVAL, 1);
            stackstr->len = instancessize;
            stackstr->p = instancesclone;

            HashSetVal(DATAEXHistory, clientname, TYPEObj(stackstr, DT_STRING));
            MEMFree(validinstances);
        }

        RBXRequestsRefreshInstances = FALSE;
        HTTPRESPONSE(SUCCESS_CREATED, package);
    }

    #undef istype
    return;

incorrect_message:
    HTTPRESPONSE(ERR_BAD_REQUEST, package);
}

void RBXClientsVerify() {
    time_t curtime = time(0);
    for (size_t i = 0; i < DEFAULT_LOG_LEN; i++) {
        RBXCLIENT client = RBXClients.buff[i];

        if (client.token == NULL) continue;
        uint16_t diff = (uint16_t)difftime(curtime, client.lrqst);

        if (diff > MAX_TIME_WITHOUT_REQUESTS) {
            HASHSTRVAL *DATAEXHistory; HashIndexing(&DATAEXClientsHistory, client.name, DATAEXHistory);
            if (DATAEXHistory != NULL) {
                HASHLIST *history = cast(DATAEXHistory->obj.data, HASHLIST*);
                HashListClean(history);
                HashSetVal(&DATAEXClientsHistory, STRVALObj(client.name, strlen(client.name)), TYPEObj(NULL, DT_NULL));
            }

            printinfo2("Rbx Client %s disconnected", client.name);
            RBXClientClean(client);

            RBXClients.buff[i] = (RBXCLIENT){0};
            RBXClients.len--;
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
