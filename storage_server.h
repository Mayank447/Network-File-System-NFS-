#ifndef SERVER_H
#define SERVER_H

#define MAX_CLIENT_CONNECTIONS 5
#define RECEIVE_BUFFER_LENGTH 1024
#define SEND_BUFFER_LENGTH 1024
#define ERROR_BUFFER_LENGTH 1024

// Some Macros
#define MAX_FILENAME_LENGTH 255
#define MAX_DIRNAME_LENGTH 255
#define MAX_FILES 10  // To be converted to a linked list
#define MAX_SUBDIRECTORIES 10 // To be converted to a linked list
#define MAX_PATH_LENGTH 1000
#define MAX_ACCESSIBLE_PATHS 100
#define MAX_DIR_TYPE 7

// Error codes
#define ERROR_LIMIT_REACHED 1
#define ERROR_STORAGE_SERVER_NOT_FOUND 2
#define ERROR_IO 3
#define ERROR_SOCKET 4

typedef struct file_struct{
    char filepath[MAX_PATH_LENGTH];   // filepath (wrt root directroy
    int read_write;                   // 0 for read/ 1 for write
    int reader_count;
    pthread_mutex_t read_write_lock;
    pthread_mutex_t get_reader_count_lock;  
    struct file_struct* next;  
} File;

typedef struct dir_struct{
    char name[MAX_DIRNAME_LENGTH];    // directory name
    char path[MAX_PATH_LENGTH];       // directory path (wrt root directory)
    int file_count;                   // no. of files inside the directory (not subdirectories)
    File* file_head;                  // head of singly linked list of files
    File* file_tail;                  // tail of singly linked list of files
    
    int subdir_count;                 // no. of subdirectories (not sub-subdirectories)
    struct dir_struct *subdir_head;   // head of singly linked list of subdirectories
    struct dir_struct* subdir_tail;   // tail of singly linked list of subdirectories
    struct dir_struct* next_subDir;   // pointer to the next subdirectory

    char dir_type[MAX_DIR_TYPE];      // type of dir created        --- folder by default
    int file_permissions;             // permissions of the file    --- 666 by default
    int last_accessed;                // time of last access
    int last_modified;                // time of last modification

} Directory;

struct StorageServer {
    char IP_Address[16];        // IP Address of the Storage Server
    int NS_Port;                // Port for communication with Name serve
    int Client_Port;            // Port for communication with Client
    char accessible_paths[MAX_ACCESSIBLE_PATHS][MAX_PATH_LENGTH];
    Directory* root_directory;  // Root directory for the storage server
    pthread_mutex_t lock;       // Lock mechanism (needs to be modified)
};

char ErrorMsg[1024];

// Functions for files
void createFile(char* path, int clientSocketID);
int uploadFile_ClientToServer(char* filename, int clientSocketID);
void sendFile_ServerToClient(char* filename, int clientSocketID);
void deleteFile(char* filename, int clientSocketID);
void getFileMetaData(char* filename, int clientSocketID);
int lockFile(File *file, int lock_type);
void write_releaseLock(File *file, int clientID);
void read_releaseLock(File *file);

// Functions for directory
void createDir(Directory* parent, const char* dirname);
void uploadDir_client_to_server(char* directoryname, int clientSocketID);
void sendDir_server_to_client(char* directoryname, int clientSocketID);
void deleteDirectory(const char* path, int clientSocketID);
void getDir(char* directoryname, int clientSocketID);
void listDirectoryContents(Directory* dir);

// Functions for initialization
int open_a_connection_port(int Port, int num_listener);
void* handleClientRequest(void*);
void requestHandler(int requestNo, int clientSocket);

#endif