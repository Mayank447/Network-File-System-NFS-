#ifndef PARAMS_H
#define PARAMS_H


// name_server.h
#define CLIENT_PORT 8080 // Port no. for communication with the client
#define STORAGE_SERVER_PORT 9090 // Port no. for communication with the storage server

#define NO_CLIENTS_TO_LISTEN_TO 50 // Maximum no. of clients the name server can handle
#define NO_SERVER_TO_LISTEN_TO 100 // Maximum no. of storage servers can initialize

#define MAX_SERVERS 100
#define MAX_CLIENTS 100

#define BUFFER_LENGTH 10000 // Max length of string received from Storage Server for initialization

#endif