#include "header_files.h"
#include "storage_server.h"

#define PATH_BUFFER_SIZE 1024
#define NS_PORT 9090 // PORT for initializing connection with the NameServer


int ss_id;      // Storage server ID
int clientPort; // PORT for communication with the client
int clientSocketID; //Main binded socket for accepting requests from clients

int nsSocketID; // Main socket for communication with the Name server
int nsPort;     // PORT for communication with Name server (user-specified)
char nsIP[16];  // Assuming IPv4

//Global pointers for beggining and last file in struct linked list
File* fileHead = NULL;
File* fileTail = NULL;

int clientSocket[MAX_CLIENT_CONNECTIONS];
char filename[50] = "paths_SS.txt";
char ErrorMsg[ERROR_BUFFER_LENGTH];

/* Close the socket*/
void closeConnection(){
    close(clientSocketID);
    close(nsSocketID);
}

/* Signal handler in case Ctrl-Z or Ctrl-D is pressed -> so that the socket gets closed */
void handle_signal(int signum){
    closeConnection();
    exit(signum);
}

/////////////////// FUNCTIONS FOR FILE STRUCT /////////////////////////
int checkFilePathExists(char* path){
    File* ptr = fileHead;
    while(ptr != NULL) {
        if(strcmp(path, ptr->filepath)==0) return 1;
        ptr=ptr->next;
    }
    return 0;
}

void addFile(char* path)
{
    // Check if the path is not already stored in the storage server (TODO - Efficient search)
    if(checkFilePathExists(path)) {
        printf("PATH already exists in Accessible path list\n");
        return;
    }

    // Creating a new File struct if the path is not already stored
    File* newFile = (File*)malloc(sizeof(File));
    strcpy(newFile->filepath, path);
    newFile->read_write = -1;
    newFile->reader_count = 0;
    newFile->next = NULL;

    if(pthread_mutex_init(&newFile->get_reader_count_lock, NULL) < 0 ||
        pthread_mutex_init(&newFile->read_write_lock, NULL))
    {
        printf("Error addFile(): Unable to initialize mutex lock\n");
        return;
    }
    
    // Adding it to the File struct linked list
    if(fileHead == NULL){
        fileHead = newFile;
        return;
    }

    File* ptr = fileHead;
    while(ptr->next != NULL) ptr=ptr->next;
    ptr->next = newFile;
    return;
}

/* Check the existence of a path and whether it corresponds to a file/directory
 returns 0 in case of files and 1 in case of directories) */
int checkFileType(char* path)
{ 
    struct stat path_stat;
    stat(path, &path_stat);

    if(S_ISREG(path_stat.st_mode)) return 0;
    else if(S_ISDIR(path_stat.st_mode)) return 1;
    return -1;
}


/////////////////// FUNCTIONS FOR INITIALIZING THE CONNECTION WITH THE NAME SERVER /////////////////////
int sendInfoToNamingServer(const char *nsIP, int nsPort, int clientPort)
{
    /* Function to send vital information to the Naming Server and receive the ss_id */
    int nsSocket;
    struct sockaddr_in nsAddress;

    // Create a socket for communication with the Naming Server
    if ((nsSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Error: opening socket for Naming Server");
        return -1;
    }
    
    memset(&nsAddress, 0, sizeof(nsAddress));
    nsAddress.sin_family = AF_INET;
    nsAddress.sin_port = htons(NS_PORT);
    nsAddress.sin_addr.s_addr = inet_addr(nsIP);
    
    // Connect to the Naming Server
    if (connect(nsSocket, (struct sockaddr *)&nsAddress, sizeof(nsAddress)) < 0){
        perror("Error: connecting to Naming Server");
        close(nsSocket);
        return -1;
    }

    // Prepare the information to send
    char infoBuffer[PATH_BUFFER_SIZE], tempBuffer[PATH_BUFFER_SIZE];
    snprintf(infoBuffer, sizeof(infoBuffer), "SENDING|STORAGE|SERVER|INFORMATION");
    strcat(infoBuffer, ":");
    snprintf(tempBuffer, sizeof(infoBuffer), "%s;%d;%d", nsIP, nsPort, clientPort);
    strcat(infoBuffer, tempBuffer);
    strcat(infoBuffer, ":");

    // Open the file for reading
    FILE *pathFile = fopen(filename, "r");
    if (pathFile == NULL){
        perror("Error opening path file");
        close(nsSocket);
        return -1;
    }

    // Read and concatenat each path specified
    char path[PATH_BUFFER_SIZE];
    while (fgets(path, sizeof(path), pathFile) != NULL)
    {
        if(path[strlen(path)-1] == '\n') 
            path[strlen(path)-1] = '\0';
        strcat(infoBuffer, path);
        strcat(infoBuffer, ":");
    }

    // Concatenating a "COMPLETED" message
    const char *completedMessage = "COMPLETED";
    strcat(infoBuffer, completedMessage);

    // Sending the information buffer and closing the file descriptor
    if (send(nsSocket, infoBuffer, strlen(infoBuffer), 0) < 0){
        perror("Error: sending information to Naming Server");
        fclose(pathFile);
        close(nsSocket);
        return -1;
    }
    fclose(pathFile);

    //  Connecting a Storage Server ID from the Naming Server on the user input 
    if((nsSocketID = (open_a_connection_port(nsPort, 1))) == -1){
        printf("Error: opening a dedicated socket for communication with NameServer");
        return -1;
    }

    // Waiting for connection request from NameServer
    nsSocketID = accept(nsSocketID, NULL, NULL);
    if (nsSocketID < 0){
        perror("Name Server accept failed...");
        return -1;
    }

    // Receiving the Storage Server ID from NameServer
    char responseBuffer[PATH_BUFFER_SIZE];
    if (recv(nsSocket, responseBuffer, sizeof(responseBuffer), 0) < 0){
        perror("Error: receiving Storage Server ID from Naming Server");
        return -1;
    }
    close(nsSocket);

    ss_id = atoi(responseBuffer);
    printf("Assigned Storage Server ID: %d\n", ss_id);
    printf("Storage Server connected to Nameserver on PORT %d ...\n", nsPort);
    return 0;
}


int open_a_connection_port(int Port, int num_listener)
{
    /* Function to open a connection to a PORT and accept a single connection */
    int socketOpened = socket(AF_INET, SOCK_STREAM, 0);
    if (socketOpened < 0){
        perror("Error open_a_connection_port: opening socket");
        return -1;
    }
    
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(Port);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Binding the socket
    if(bind(socketOpened, (struct sockaddr *)&serverAddress, sizeof(serverAddress))<0){
        perror("Error open_a_connection_port: binding socket");
        return -1;
    }

    // Listening for connection
    if (listen(socketOpened, num_listener) == -1){
        perror("Error: Unable to listen");
        return -1;
    }
    return socketOpened;
}


void collectAccessiblePaths()
{
    /* Function to collect accessible paths from the user and store them in a file */
    FILE *file = fopen(filename, "w");
    if (file == NULL){
        perror("Error opening paths_SS.txt");
        return;
    }
    fflush(stdin);

    char path[PATH_BUFFER_SIZE];
    while (1)
    {
        printf("Enter an accessible path (or 'exit' to stop): ");
        fgets(path, sizeof(path), stdin);
        if (strcmp(path, "exit\n") == 0) 
            break;
        else{
            int index = strchr(path, '\n') - path;
            path[index] = '\0';
        }
        
        int type = checkFileType(path);

        // Invalid path
        if(type == -1) 
            printf("Invalid path\n");

        // Path corresponds to a File
        else if(type == 0) {
            fprintf(file, "%s", path);
            addFile(path); // Store the path in a File struct
        }
        
        // Path corresponds to a directory
        else if(type == 1){
            fprintf(file, "%s", path); // Write the path to the Path file 
        }

        else{
            printf("Internal server error\n");
        }

    }
    fclose(file);
}


//////////////// FUNCTIONS TO HANDLE KEY COMMUNICATION WITH NAMESERVER ///////////////////




////////////////////// FUNCTIONS TO HANDLE COMMUNICATION WITH CLIENT /////////////////////////
void handleErrorCodes(int valid, char* message){
    if(valid == 0) 
        strcpy(message, "VALID");
    else if(valid == 1)
        strcpy(message, "NAMESERVER ERROR");
    else if(valid == 2) 
        strcpy(message, "PATH DOES NOT EXIST");
    else if(valid == 3)
        strcpy(message, "STORAGE SERVER IS DOWN");
    else if(valid == 4)
        strcpy(message, "A CLIENT IS ALREADY READING THE FILE");
    else if(valid == 5)
        strcpy(message, "A CLIENT IS ALREADY WRITING TO THE FILE");
    else if(valid == 6)
        strcpy(message, "FILE DOES NOT EXIST");
    else if(valid == 7)
        strcpy(message, "FILE ALREADY EXISTS");
    else if(valid == 8)
        strcpy(message, "DIRECTORY DOES NOT EXIST");
    else if(valid == 9)
        strcpy(message, "DIRECTORY ALREADY EXIST");
    else if(valid == 10)
        strcpy(message, "INVALID OPERATION NUMBER");
}


int receive_validateRequestNo(int clientSocket)
{
    int valid = 0;
    char request[RECEIVE_BUFFER_LENGTH];
    char response[100];

    // Receiving the request no from the client
    if(recv(clientSocket, request, sizeof(request), 0) < 0){
        perror("Error receive_validateRequestNo(): receiving operation number from the client");
        return -1;
    }
    int request_no = atoi(request);

    // Validating the range of the request number
    if(request_no < 1 || request_no > 9) {
        valid = 10;
        request_no = -1;
    }
    else valid = 0;
    handleErrorCodes(valid, response);

    // Sending back the response
    if(send(clientSocket, response, strlen(response), 0) < 0){
        perror("Error receive_validateRequestNo(): Unable to send reply to request no.");
        return -1;
    }
    return request_no;
}


int validateFilePath(char* filepath, int operation_no, File* file)
{
    /* Function also locks the file based on the operation number */
    File* ptr = fileHead;
    while(ptr != NULL) 
    {
        // If the file found and the operation is to read
        if(strcmp(filepath, ptr->filepath)==0 && operation_no == 1)
        {
            file = ptr;
            pthread_mutex_lock(&ptr->read_write_lock);
            
            //Someone is already writing to the file
            if(ptr->read_write == 1) {
                pthread_mutex_unlock(&ptr->read_write_lock);
                return 5; 
            }
            else ptr->read_write = 0;
            
            // Incrementing the reader count
            pthread_mutex_lock(&ptr->get_reader_count_lock);
            ptr->reader_count++;
            pthread_mutex_unlock(&ptr->get_reader_count_lock);
            
            pthread_mutex_unlock(&ptr->read_write_lock);
            return 0;
        }

        // File path matches and operation is to write
        else if(strcmp(filepath, ptr->filepath)==0 && operation_no == 2)
        {
            pthread_mutex_lock(&ptr->read_write_lock);
            
            // Someone is already reading the file
            if(ptr->read_write == 0){
                pthread_mutex_unlock(&ptr->read_write_lock);
                return 4;
            }
            else ptr->read_write = 1;

            pthread_mutex_unlock(&ptr->read_write_lock);
            return 0;
        }

        ptr=ptr->next;
    }
    return 2;

}


int receive_validateFilePath(int clientSocket, char* filepath, int operation_no, File* file)
{
    // Receiving the filePath
    if(recv(clientSocket, filepath, MAX_PATH_LENGTH, 0) < 0){
        perror("Error receive_validateFilePath(): Unable to receive the file path");
        close(clientSocket);
        return -1;
    } 

    // Validating the filepath based on return value (ERROR_CODE)
    char response[1000];
    int valid = validateFilePath(filepath, operation_no, file);
    handleErrorCodes(valid, response);
    
    //Sending back the response
    if(send(clientSocket, response, strlen(response), 0) < 0){
        perror("Error receive_validateRequestNo(): Unable to send reply to request no.");
        return -1;
    }
    return valid;
}

// Decrease the reader count
void decreaseReaderCount(File* file){
    pthread_mutex_lock(&file->get_reader_count_lock);
    
    if(--file->reader_count == 0){
        pthread_mutex_lock(&file->read_write_lock);
        file->read_write = -1;
        pthread_mutex_unlock(&file->read_write_lock);
    }
    pthread_mutex_unlock(&file->get_reader_count_lock);
}

void handleClients()
{
    while(1){
        struct sockaddr_in client_address;
        socklen_t address_size = sizeof(struct sockaddr_in);
        int clientSocket = accept(clientSocketID, (struct sockaddr*)&client_address, &address_size);
        if (clientSocket < 0) {
            perror("Error handleClients(): Client accept failed");
            continue;
        }
        
        printf("Client connection request accepted from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
        
        // Create a new thread for each new client
        pthread_t clientThread;
        if (pthread_create(&clientThread, NULL, (void*)handleClientRequest, (void*)&clientSocket) < 0) {
            perror("Thread creation failed");
            continue;
        }

        // Detach the thread
        if (pthread_detach(clientThread) != 0) {
            perror("Error detaching client thread");
            continue;
        }
    }
    return;
}


void* handleClientRequest(void* argument)
{
    // Receiving and validating the request no. from the client
    File* file = NULL;
    int clientSocket = *(int*)argument;
    int request_no = receive_validateRequestNo(clientSocket);
    if(request_no == -1) return NULL;
    
    // Request Handler for that particular query
    if(request_no == 1)
    {
        char filepath[MAX_PATH_LENGTH];
        if(receive_validateFilePath(clientSocket, filepath, 1, file) != 0){
            return NULL;
        }
        sendFile_ServerToClient(filepath, clientSocket);
        decreaseReaderCount(file);
        close(clientSocket);
        return NULL;
    }

    else if(request_no == 2){
        char filepath[MAX_PATH_LENGTH];
        if(receive_validateFilePath(clientSocket, filepath, 1, file) != 0){
            return NULL;
        }
        uploadFile_ClientToServer(filepath, clientSocket);
        close(clientSocket);
        return NULL;
    }
    return NULL;
}



///////////////////////////// FILE FUNCTIONS //////////////////////////

/* void createFile(Directory *parent, const char *filename, int ownerID)
{
    File *newFile = (File *)malloc(sizeof(File));
    if (newFile == NULL)
    {
        printf("createFile: malloc failed\n");
        return;
    }
    strcpy(newFile->name, filename);
    strcpy(newFile->path, parent->path);
    strcat(newFile->path, filename);
    if(head == NULL)
    {
        head = newFile;
    }
    if(tail == NULL)
    {
        tail = newFile;
    }

    tail->nextfile = newFile;
    tail = newFile;
    newFile->nextfile = NULL;
    initialize meta data
    newFile->size = 0;
    newFile->ownerID = ownerID;
    newFile->reader_count = 0;
    newFile->write_client_id = 0;
    newFile->is_locked = 0;
    newFile->file_type = NULL;
    newFile->file_permissions = 666;
    newFile->last_accessed = 0;
    newFile->last_modified = 0;
}
*/

void createDir(Directory *parent, const char *dirname)
{
    Directory *newDir = (Directory *)malloc(sizeof(Directory));
    if (newDir == NULL)
    {
        printf("createDir: malloc failed\n");
        return;
    }
    strcpy(newDir->name, dirname);
    newDir->file_count = 0;
    newDir->subdir_count = 0;
    newDir->next_subDir = NULL;
    newDir->file_head = newDir->file_tail = NULL;
    newDir->subdir_head = newDir->subdir_tail = NULL;
    newDir->last_accessed = 0;
    newDir->last_modified = 0;
    newDir->file_permissions = 0;

    // Inserting the new node at the end of the parent linked list
    if (parent == NULL)
    {
        strcpy(newDir->path, "/");
    }
    else
    {
        strcpy(newDir->path, parent->path);
        strcat(newDir->path, dirname);
        strcat(newDir->path, "/");

        if (parent->subdir_count == 0)
        {
            parent->subdir_head = parent->subdir_tail = newDir;
            parent->subdir_count++;
            return;
        }
        parent->subdir_tail->next_subDir = newDir;
        parent->subdir_count++;
    }
}

void sendFile_ServerToClient(char *filename, int clientSocket)
{
    // Open the file for reading on the server side
    FILE *file = fopen(filename, "r");  // Use "rb" for binary mode
    if (file == NULL){
        bzero(ErrorMsg, ERROR_BUFFER_LENGTH);
        snprintf(ErrorMsg, ERROR_BUFFER_LENGTH, "File not found.");
        
        if (send(clientSocket, ErrorMsg, strlen(ErrorMsg), 0) < 0){
            perror("Error sendFile_ServerToClient(): FILE NOT FOUND");
            close(clientSocket);
        }
    }

    else {
        char buffer[1024] = {'\0'};
        while (fgets(buffer, sizeof(buffer), file) != NULL){
            if (send(clientSocket, buffer, sizeof(buffer), 0) < 0){
                perror("Error sending file");
                fclose(file);
                return;
            }
        }

        close(clientSocket);
        fclose(file);
        printf("File %s sent successfully.\n", filename);
    }
}

/* Upload a file from client to server - uploadFile()*/
int uploadFile_ClientToServer(char *filename, int clientSocketID)
{
    // Open the file for reading on the server side
    FILE * file = fopen(filename, "a");
    if (file == NULL) {
        printf("Error file not found.\n");
        return -1;
    }

    char buffer[10000];
    int bytesReceived;

    while(1)
    {
        if((bytesReceived = recv(clientSocketID, buffer, sizeof(buffer), 0)) < 0){
        perror("Recieve error");
        return -1;
        }

        if(strcmp(buffer, "STOP") == 0){
            break;
        }

        if (fprintf(file, "%s", buffer) < 0) {
        printf("Error writing to the file.\n");
        return -1;
        }
    }

    fclose(file);
    printf("File %s updated successfully.\n", filename);
    return 0;
}

/* void listDirectoryContents(Directory *parent)
{
    // Printing all the subdirectories
    for (Directory *dir = parent->subdir_head; dir != NULL; dir = dir->next_subDir)
    {
        printf("%s\n", dir->name);
    }

    // Printing all the files
    for (File *file = parent->file_head; file != NULL; file = file->nextfile)
    {
        printf("%s \t (%d bytes \t ownerID: %d)\n", file->name, file->size, file->ownerID);
    }
} */

/* Lock a File 
int lockFile(File *file, int lock_type)
{
    if (file->is_locked == 0)
    {
        file->is_locked = lock_type;
        return 0;
    }

    else if (lock_type == 1 && file->is_locked == 1)
    {
        file->reader_count++;
        return 0;
    }
    return 1;
}
*/

/* Release write lock on a file 
void write_releaseLock(File *file, int clientID)
{
    if (file->is_locked == 2 && file->write_client_id == clientID)
    {
        file->is_locked = 0;
    }
} */

/* Release read lock on a file 
void read_releaseLock(File *file)
{
    if (--file->reader_count == 0 && file->is_locked == 1)
    {
        file->is_locked = 0;
    }
} */





/* Delete a file - deleteFile() */
void deleteFile(char *filename, int clientSocketID)
{
    if (access(filename, F_OK) != 0)
    { // File does not exist
        if (send(clientSocketID, "No matching file", 17, 0) < 0)
        {
            perror("Unable to send message: No matching files");
        }
        return;
    }

    // Check for permission [TODO]
    if (remove(filename) == 0)
    {
        if (send(clientSocketID, "File deleted successfully", 26, 0) < 0)
        {
            perror("Unable to send message: File deleted successfully");
        }
    }
    else
    {
        if (send(clientSocketID, "Unable to delete the file", 26, 0) < 0)
        {
            perror("Unable to send message: Unable to delete the file");
        }
    }
}

/* Delete a folder - deleteDirectory() */
void deleteDirectory(const char *path, int clientSocketID)
{
    DIR *dir;
    struct stat stat_path, stat_entry;
    struct dirent *entry;
    // stat for the path
    stat(path, &stat_path);

    // if path does not exists or is not dir - exit with status -1
    if (S_ISDIR(stat_path.st_mode) == 0)
    {
        bzero(ErrorMsg, ERROR_BUFFER_LENGTH);
        sprintf(ErrorMsg, "Is not directory: %s\n", path);
        if (send(clientSocketID, ErrorMsg, sizeof(ErrorMsg), 0) < 0)
        {
            perror("Unable to send: Is not directory");
        }
        return;
    }

    // if not possible to read the directory for this user
    if ((dir = opendir(path)) == NULL)
    {
        bzero(ErrorMsg, ERROR_BUFFER_LENGTH);
        sprintf(ErrorMsg, "Can`t open directory: %s\n", path);
        if (send(clientSocketID, ErrorMsg, sizeof(ErrorMsg), 0) < 0)
        {
            perror("Unable to send: Can`t open directory");
        }
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue; // Skip current and parent directories
        }

        char entryPath[MAX_PATH_LENGTH];
        snprintf(entryPath, sizeof(entryPath), "%s/%s", path, entry->d_name);

        if (entry->d_type == 4)
        { // Recursive call for subdirectories
            deleteDirectory(entryPath, clientSocketID);
        }

        else
        {
            deleteFile(entryPath, clientSocketID); // Delete files

            if (remove(entryPath) != 0)
            {
                bzero(ErrorMsg, ERROR_BUFFER_LENGTH);
                sprintf(ErrorMsg, "Error deleting file: %s", entryPath);
                if (send(clientSocketID, ErrorMsg, sizeof(ErrorMsg), 0) < 0)
                {
                    printf("%s ", ErrorMsg);
                    perror("Error sending message:");
                }
            }
        }
    }
    closedir(dir);

    // Remove the empty directory after deleting its contents
    if (rmdir(path) != 0)
    {
        bzero(ErrorMsg, ERROR_BUFFER_LENGTH);
        sprintf(ErrorMsg, "Error: Deleting the repository %s", path);
        if (send(clientSocketID, ErrorMsg, sizeof(ErrorMsg), 0) < 0)
        {
            printf("%s ", ErrorMsg);
            perror("Error sending message:");
        }
    }
}

/*Function to get Meta Data*/
void getFileMetaData(char* filename, int clientSocketID) {

    char* filepath = (char*)malloc(sizeof(filename));
    strcpy(filepath, filename);

    struct stat fileStat;

   char buffer[10000];
    int bytesReceived;
        // Use the stat function to retrieve file metadata
        if (stat(filepath, &fileStat) == 0) {
            // Format file metadata into a single string
            int n = snprintf(buffer, sizeof(buffer), "%s:%ld:%o\n", filepath, (long)fileStat.st_size, (unsigned int)(fileStat.st_mode & 0777));
            
            if (n < 0) {
                perror("Error formatting file metadata");
            }

            // Send the formatted metadata
            if (send(clientSocketID, buffer, n, 0) < 0) {
                perror("Error sending file metadata");
            }
        } else {
            perror("stat");
        }
}

// Copy files between 2 servers - copyFilesender()*/                -------------------------- implement later
// int copyFile_sender(const char *sourcePath, struct sockaddr_in server_address) {
//     int serverSocket;
//     if(connect(serverSocket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1){
//         perror("Error connecting to server");
//         return 1;
//     }
//     sendFile_server_to_client(sourcePath, serverSocket);
//     return 0;
// }
// // Copy files between 2 servers - copyFileReceiver()*/                -------------------------- implement later
// int copyFile_receiver(const char *destinationPath, struct sockaddr_in server_address) {
//     int serverSocket;
//     if(connect(serverSocket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1){
//         perror("Error connecting to server");
//         return 1;
//     }
//     uploadFile_client_to_server(destinationPath, serverSocket);
//     return 0;
// }

/* Function to rename file */
int renameFile(const char *oldFileName, const char *newFileName, int clientSocketID)
{
    // Attempt to rename the file
    if (rename(oldFileName, newFileName) == 0)
    {
        // Successfully renamed the file, send a success response to the client
        char response[1024];
        snprintf(response, sizeof(response), "RENAME_SUCCESS %s %s", oldFileName, newFileName);
        if (send(clientSocketID, response, strlen(response), 0) < 0)
        {
            perror("Error sending rename success response to the client");
        }
        return 0; // Successfully renamed the file
    }
    else
    {
        // Failed to rename the file, send an error response to the client
        char response[1024];
        snprintf(response, sizeof(response), "RENAME_ERROR %s %s", oldFileName, newFileName);
        if (send(clientSocketID, response, strlen(response), 0) < 0)
        {
            perror("Error sending rename error response to the client");
        }
        perror("Error renaming the file");
        return -1; // Failed to rename the file
    }
}

// /*Function to copy directories copyDirectory()*/     -------------------------- implement later
// void copyDirectory(const char *sourceDir, const char *destinationDir) {
//     struct dirent *entry;
//     struct stat statInfo;
//     char sourcePath[MAX_PATH_LENGTH], destinationPath[MAX_PATH_LENGTH];
//     // Open source directory
//     DIR *dir = opendir(sourceDir);
//     if (dir == NULL) {
//         perror("Unable to open source directory");
//         return;
//     }
//     // Create destination directory
//     mkdir(destinationDir, 0777);
//     while ((entry = readdir(dir))) {
//         if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
//             sprintf(sourcePath, "%s/%s", sourceDir, entry->d_name);
//             sprintf(destinationPath, "%s/%s", destinationDir, entry->d_name);
//             if (stat(sourcePath, &statInfo) == 0) {
//                 if (S_ISDIR(statInfo.st_mode)) {
//                     // Recursively copy subdirectories
//                     copyDirectory(sourcePath, destinationPath);
//                 } else {
//                     // Copy files
//                     copyFile(sourcePath, destinationPath);
//                 }
//             }
//         }
//     }
//     closedir(dir);
// }







/////////////////////////// MAIN FUNCTION ///////////////////////////
int main(int argc, char *argv[])
{
    // Signal handler for Ctrl+C and Ctrl+Z
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Ask the user for the Naming Server's IP and port and Client communication Port
    printf("Enter the IP address of the Naming Server: ");
    scanf("%s", nsIP);
    printf("Enter the port to talk with the Naming Server: ");
    scanf("%d", &nsPort);
    printf("Enter the port to talk with the Client: ");
    scanf("%d", &clientPort);
    
    // Collect accessible paths from the user and store in a file
    collectAccessiblePaths();
    
    // Send vital information to the Naming Server and receive the Storage Server ID
    int initialiazed = sendInfoToNamingServer(nsIP, nsPort, clientPort);
    if (initialiazed == -1) {
        printf("Failed to send information to the Naming Server.\n");
    }
    else{
        snprintf(filename, sizeof(filename), "path_SS%d.txt", ss_id);
        if (rename("path_SS.txt", filename) != 0){
            perror("Error renaming the Paths file");
        }
    }

    // Accepting request from clients - This will loop for ever
    clientSocketID = open_a_connection_port(clientPort, MAX_CLIENT_CONNECTIONS);
    printf("Storage server listening for clients on PORT %d\n", clientPort);
    handleClients();
    
    // Closing connection - This part of the code is never reached
    closeConnection();
    return 0;
}