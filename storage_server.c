#include "header_files.h"
#include "storage_server.h"
#include "helper_functions.h"
#include "file.h"
#include "directory.h"

#define PATH_BUFFER_SIZE 1024
#define NS_PORT 9090 // PORT for initializing connection with the NameServer

//Global pointers for beggining and last file in struct linked list
extern File* fileHead;
extern File* fileTail;

int ss_id;      // Storage server ID
int clientPort; // PORT for communication with the client
int clientSocketID; //Main binded socket for accepting requests from clients

int nsSocketID; // Main socket for communication with the Name server
int nsPort;     // PORT for communication with Name server (user-specified)
char nsIP[16];  // Assuming IPv4


int clientSocket[MAX_CLIENT_CONNECTIONS];
char Msg[ERROR_BUFFER_LENGTH];
char paths_file[50] = "paths_SS.txt";

/* Close the sockets*/
void closeConnection(){
    close(clientSocketID);
    close(nsSocketID);
    cleanUpFileStruct();
}

/* Signal handler in case Ctrl-Z or Ctrl-D is pressed -> so that the socket gets closed */
void handle_signal(int signum){
    closeConnection();
    exit(signum);
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
    FILE *pathFile = fopen(paths_file, "r");
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


// Function to open a connection to a PORT and accept a single connection
int open_a_connection_port(int Port, int num_listener)
{
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


// Function to collect accessible paths from the user and store them in a file
void collectAccessiblePaths()
{
    // Check if paths file exists

    FILE *file = fopen(paths_file, "w");
    if (file == NULL){
        perror("Error opening paths_SS.txt");
        return;
    }
    getchar(); //For linux users
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
            addFile(path, 1); // Store the path in a File struct
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
int receiveAndValidateRequestNo(int clientSocket)
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
    sprintf(response, "%d", valid);

    // Sending back the response
    if(send(clientSocket, response, strlen(response), 0) < 0){
        perror("Error receive_validateRequestNo(): Unable to send reply to request no.");
        return -1;
    }
    return request_no;
}


int receiveAndValidateFilePath(int clientSocket, char* filepath, int operation_no, File* file, int validate)
{
    // Receiving the filePath
    if(recv(clientSocket, filepath, MAX_PATH_LENGTH, 0) < 0){
        perror("Error receiveAndValidateFilePath(): Unable to receive the file path");
        close(clientSocket);
        return -1;
    } 

    // Validating the filepath based on return value (ERROR_CODE)
    char response[100];
    int valid = 0;

    if(validate){
        valid = validateFilePath(filepath, operation_no, file);
    }
    sprintf(response, "%d", valid);
    
    //Sending back the response
    if(send(clientSocket, response, strlen(response), 0) < 0){
        perror("Error receive_validateRequestNo(): Unable to send reply to request no.");
        return -1;
    }
    return valid;
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
    int clientSocket = *(int*)argument;
    int request_no = receiveAndValidateRequestNo(clientSocket);
    if(request_no == -1) return NULL;
    
    File file;
    char filepath[MAX_PATH_LENGTH];

    // CREATE FILE
    if(request_no == 1 && receiveAndValidateFilePath(clientSocket, filepath, 1, NULL, 0) == 0) {
        createFile(filepath, clientSocket);
    }

    // CREATE DIRECTORY
    else if(request_no == 2){

    }

    //READ FILE
    else if(request_no == 3 && receiveAndValidateFilePath(clientSocket, filepath, 3, &file, 1) == 0) {
        uploadFile(filepath, clientSocket);
        decreaseReaderCount(&file);
    }

    // WRITE FILE
    else if(request_no == 4){
        if(receiveAndValidateFilePath(clientSocket, filepath, 4, &file, 1) == 0){
            downloadFile(filepath, clientSocket);
            openWriteLock(&file);
        }
    }

    // GET PERMISSIONS
    else if(request_no == 5 && receiveAndValidateFilePath(clientSocket, filepath, 5, NULL, 1) == 0){
        getFileMetaData(filepath, clientSocket);
    }

    // DELETE FILE
    else if(request_no == 6 && receiveAndValidateFilePath(clientSocket, filepath, 6, NULL, 1) == 0){
        deleteFile(filepath, clientSocket);
    }

    // DELETE DIRECTORY
    else if(request_no == 7){

    }

    // COPY FOLDER
    else if(request_no == 8){

    }

    // COPY DIRECTORY
    else if(request_no == 9){

    }

    close(clientSocket);
    return NULL;
}





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
        char filename[50];
        sprintf(filename, "paths_SS%d.txt", ss_id);
        if (rename(paths_file, filename) != 0){
            perror("Error renaming the Paths file");
        }
    }

    // Create a thread to communicate with the nameserver

    // Accepting request from clients - This will loop for ever
    clientSocketID = open_a_connection_port(clientPort, MAX_CLIENT_CONNECTIONS);
    printf("Storage server listening for clients on PORT %d\n", clientPort);
    handleClients();
    
    // Closing connection - This part of the code is never reached
    closeConnection();
    return 0;
}