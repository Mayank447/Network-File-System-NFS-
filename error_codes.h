#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#define VALID 0
#define VALID_STRING "0"

// File errors
#define ERROR_PATH_DOES_NOT_EXIST 2
#define ERROR_ONE_PATH_DOES_NOT_EXIST 16
#define ERROR_CREATING_FILE 12
#define ERROR_A_CLIENT_ALREADY_READING_THE_FILE 4
#define ERROR_A_CLIENT_ALREADY_WRITING_TO_FILE 5
#define ERROR_FILE_DOES_NOT_EXIST 6
#define ERROR_FILE_ALREADY_EXISTS 7
#define ERROR_OPENING_FILE 13
#define ERROR_GETTING_FILE_PERMISSIONS 15
#define ERROR_UNABLE_TO_DELETE_FILE 14

// Directory errors
#define ERROR_DIRECTORY_DOES_NOT_EXIST 8
#define ERROR_DIRECTORY_ALREADY_EXISTS 9

// Network error
#define ERROR_INVALID_REQUEST_NUMBER 10
#define STORAGE_SERVER_ERROR 11
#define STORAGE_SERVER_DOWN 3
#define NAME_SERVER_ERROR 1
#define RECEIVE_ERROR 16




#endif