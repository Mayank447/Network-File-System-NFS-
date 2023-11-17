#ifndef NAME_SERVER_H
#define NAME_SERVER_H

#define MAX_NO_ACCESSIBLE_PATHS 300
#define MAX_LENGTH_OF_ACCESSIBLE_PATH 300
struct StorageServerInfo{
    int ss_id;
    int serverSocket;
    char ip_address[256];
    int naming_server_port;
    int client_server_port;
    char accessible_paths[MAX_NO_ACCESSIBLE_PATHS][MAX_LENGTH_OF_ACCESSIBLE_PATH];
    struct StorageServerInfo* next;
    struct StorageServerInfo* redundantSS_1;
    struct StorageServerInfo* redundantSS_2;
};

struct StorageServerInfo* addStorageServerInfo(const char *ip, int ns_port, int cs_port);
int initConnectionToStorageServer(struct StorageServerInfo* server);
void parseStorageServerInfo(const char *data, char *ip_address, int *ns_port, int *cs_port);
void* handleStorageServer(void* argument);
void* handleClientRequests(void*);
struct StorageServerInfo* searchStorageServer(char* file_path);

#endif