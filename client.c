#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8000
#define BUFFER_SIZE 1024

int main() {
    int client_socket;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE];
    char new_server_ip[256];
    int new_server_port;

    // Creating socket file descriptor
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }

    printf("Choose operation:\n");
    printf("1. Read the file and save it to a directory\n");
    printf("2. Write to the file\n");
    printf("3. Obtain information about the file\n");
    int operation;
    scanf("%d", &operation);

    if (operation == 1) {
        // Perform read operation
        printf("Enter the file path to read: ");
        char file_path[256];
        scanf("%s", file_path);
        // Send the file path to the server
        send(client_socket, file_path, strlen(file_path), 0);
        // Receive the new server IP address and port number
        recv(client_socket, new_server_ip, sizeof(new_server_ip), 0);
        recv(client_socket, &new_server_port, sizeof(new_server_port), 0);
        // Create a new socket and connect to the new server address
        int new_client_socket;
        struct sockaddr_in new_server_address;
        if ((new_client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }
        new_server_address.sin_family = AF_INET;
        new_server_address.sin_port = htons(new_server_port);
        if (inet_pton(AF_INET, new_server_ip, &new_server_address.sin_addr) <= 0) {
            perror("Invalid address/Address not supported");
            exit(EXIT_FAILURE);
        }
        if (connect(new_client_socket, (struct sockaddr*)&new_server_address, sizeof(new_server_address) < 0)) {
            perror("Connection to new server failed");
            exit(EXIT_FAILURE);
        }
        // Receive and save the file content
        char local_file_name[256];
        char *filename = strrchr(file_path, '/');
        if (filename != NULL) {
            // If there's a '/' character in the path, use the portion after the last '/'
            filename++;  // Move past the '/'
        } else {
            // If there's no '/' character in the path, use the entire path as the filename
            filename = file_path;
        }
        snprintf(local_file_name, sizeof(local_file_name), "./%s", filename);
        FILE *local_file = fopen(local_file_name, "wb");
        if (local_file == NULL) {
            perror("Error opening local file for writing");
            exit(EXIT_FAILURE);
        }
        while (1) {
            int bytes_received = recv(new_client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0) {
                break;
            }
            // Check if the received data contains "STOP" and break if it does
            if (strstr(buffer, "STOP") != NULL) {
                break;
            }
            fwrite(buffer, 1, bytes_received, local_file);
        }
        printf("File received and saved as %s in the current directory.\n");
        fclose(local_file);
        close(new_client_socket);
        // Closing Storage server
    } else if (operation == 2) {
        // Perform write operation
        printf("Enter the file path to write: ");
        char file_path[256];
        scanf("%s", file_path);
        send(client_socket, file_path, strlen(file_path), 0);
        // Receive the new server IP address and port number
        recv(client_socket, new_server_ip, sizeof(new_server_ip), 0);
        recv(client_socket, &new_server_port, sizeof(new_server_port), 0);
        // Create a new socket and connect to the new server address
        int new_client_socket;
        struct sockaddr_in new_server_address;
        if ((new_client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }
        new_server_address.sin_family = AF_INET;
        new_server_address.sin_port = htons(new_server_port);
        if (inet_pton(AF_INET, new_server_ip, &new_server_address.sin_addr) <= 0) {
            perror("Invalid address/Address not supported");
            exit(EXIT_FAILURE);
        }
        if (connect(new_client_socket, (struct sockaddr*)&new_server_address, sizeof(new_server_address) < 0)) {
            perror("Connection to new server failed");
            exit(EXIT_FAILURE);
        }

        // Additional operation for the Write to the file option
        printf("Enter data to write (type 'STOP' to end): \n");
        while (1) {
            char input_data[256];
            fgets(input_data, sizeof(input_data), stdin);
            send(new_client_socket, input_data, strlen(input_data), 0);
            if (strstr(input_data, "STOP") != NULL) {
                break;
            }
        }
        printf("Data sent to the server.\n");

        close(new_client_socket);
    } else if (operation == 3) {
        // Perform file information operation
        // Send the request for information to the server
        send(client_socket, "INFO", strlen("INFO"), 0);

        // Receive and print file size and access permissions
        int file_size;
        char permissions[10];
        recv(client_socket, &file_size, sizeof(file_size), 0);
        recv(client_socket, permissions, sizeof(permissions), 0);

        printf("File Size: %d bytes\n", file_size);
        printf("Access Permissions: %s\n", permissions);
    } else {
        printf("Invalid operation choice.\n");
    }

    // Do not close client_socket here
    close(client_socket);
    return 0;
}
